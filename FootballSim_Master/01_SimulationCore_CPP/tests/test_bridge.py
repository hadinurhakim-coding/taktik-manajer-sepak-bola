import copy
import ctypes as C
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest

CORE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CORE))
from convert_old_data_to_cpp import readonly_snapshot, convert_match, convert_team, sha256, write_output, bucket
from match_bridge import MatchLibrary
from read_fsim import decode_fsim

LIBRARY_PATH = sys.argv.pop(1)


class BridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.library = MatchLibrary(LIBRARY_PATH)
        cls.temp_base = CORE / "build/test_data"
        cls.temp_base.mkdir(parents=True, exist_ok=True)

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(dir=self.temp_base)
        self.folder = Path(self.directory.name).resolve()
        self.assertTrue(self.folder.is_relative_to(self.temp_base.resolve()))
        self.db = self.folder / "legacy.db"
        self.connection = sqlite3.connect(self.db)
        self.connection.executescript("""
            CREATE TABLE league_teams(id TEXT,league_id TEXT,team_name TEXT,team_short_name TEXT,rating REAL);
            CREATE TABLE club_states(team_id TEXT,league_id TEXT,revision INTEGER,fitness REAL,tactics TEXT,lineup TEXT);
            CREATE TABLE club_players(id TEXT,league_id TEXT,team_id TEXT,body TEXT);
        """)
        for team in ("home", "away"):
            players = []
            for i in range(12):
                p = {"id": f"{team}:{i}", "name": "Pemain é 日本", "number": i+1, "role": "GK" if i == 0 else "MF", "anchor": [0, 0],
                     **{key: 75 for key in ("speed", "acceleration", "stamina", "passing", "control", "finishing", "heading", "tackling", "positioning", "goalkeeping")}}
                p["control"] = 81.5
                self.connection.execute("INSERT INTO club_players VALUES(?,?,?,?)", (p["id"], "league", team, json.dumps(p)))
                players.append(p["id"])
            tactics = {"formation": "4-2-3-1", "style": "Penguasaan Bola (Possession)", "mentality": "Balanced", "pressing": 67, "tempo": 33, "width": 48,
                       **{key: players[0] for key in ("captain", "penalty", "freeKick", "cornerLeft", "cornerRight")}}
            self.connection.execute("INSERT INTO league_teams VALUES(?,?,?,?,?)", (team, "league", "Klub é 日本", "K", 75))
            self.connection.execute("INSERT INTO club_states VALUES(?,?,?,?,?,?)", (team, "league", 4, 95, json.dumps(tactics), json.dumps(players[:11])))
        self.connection.commit()

    def tearDown(self):
        self.connection.close()
        self.directory.cleanup()

    def export(self):
        before = sha256(self.db)
        with readonly_snapshot(self.db) as (connection, proof):
            data = convert_match(connection, "home", "away", 2**64 - 1)
            with self.assertRaises(sqlite3.OperationalError):
                connection.execute("UPDATE league_teams SET rating=0")
        self.assertTrue(proof["source_unchanged"])
        self.assertEqual(before, sha256(self.db))
        return data

    def test_schema_matches_requested_constants(self):
        spec = json.loads((CORE.parent / "00_Specs_and_Logs/match_engine_schema.json").read_text())
        self.assertEqual(self.library.schema(), spec)

    def test_sqlite_to_native_end_to_end_and_clocks(self):
        data = self.export()
        self.assertEqual(data["teams"][0]["players"][0]["attributes"]["ball_control"], 81.5)
        self.assertEqual(data["teams"][0]["tactics"]["pressing"], "high")
        self.assertEqual(data["teams"][0]["tactics"]["tempo"], "slow")
        with self.library.create(data) as match, self.library.create(data) as other:
            initial = match.snapshot()
            self.assertEqual(initial["seed"], str(2**64 - 1))
            self.assertEqual(initial["teams"][0]["name"], "Klub é 日本")
            self.assertEqual(len(initial["players"]), 22)
            match.tick(27000)
            middle = match.snapshot()
            self.assertEqual((middle["real_elapsed_seconds"], middle["game_elapsed_seconds"]), (450, 2700))
            self.assertEqual(middle["phase"], "second_half")
            self.assertEqual(other.snapshot()["tick"], 0)
            match.tick(2**32 - 1)
            final = match.snapshot()
            self.assertEqual((final["real_elapsed_seconds"], final["game_elapsed_seconds"]), (900, 5400))
            self.assertEqual(final["phase"], "completed")
            self.assertTrue(final["score_is_final_simulated_result"])
            match.tick(0)
            self.assertEqual(match.snapshot(), final)

    def test_bad_inputs_and_destroyed_handles(self):
        data = self.export()
        for change in (
            lambda d: d.update(input_version="friendly-1.6"),
            lambda d: d.update(seed=str(2**64)),
            lambda d: d["teams"][0]["players"][0]["attributes"].update(speed=True),
            lambda d: d["teams"][0]["lineup"].__setitem__(0, "missing"),
            lambda d: d["teams"][0]["tactics"].update(pressing="low"),
        ):
            bad = copy.deepcopy(data); change(bad)
            with self.assertRaises(RuntimeError): self.library.create(bad)
        handle = C.c_uint64(999)
        malformed = b'{"broken":'
        self.assertEqual(self.library.api.init_match(malformed, len(malformed), C.byref(handle)), 1)
        self.assertEqual(handle.value, 0)
        nested = ("[" * 100 + "]" * 100).encode()
        self.assertEqual(self.library.api.init_match(nested, len(nested), C.byref(handle)), 1)
        match = self.library.create(data); stale = match.handle; match.close()
        self.assertEqual(self.library.api.tick_match_fixed(stale, 1), 2)
        self.assertEqual(self.library.api.destroy_match(stale), 2)
        with self.assertRaises(ValueError): match.tick(-1)

    def test_buffer_contract_no_truncation(self):
        with self.library.create(self.export()) as match:
            required = C.c_size_t()
            small = C.create_string_buffer(b"ABCD", 5)
            status = self.library.api.get_match_snapshot(match.handle, small, 5, C.byref(required))
            self.assertEqual(status, 3)
            self.assertEqual(small.raw, b"ABCD\0")
            self.assertGreater(required.value, 5)
            self.assertEqual(self.library.api.get_match_snapshot(match.handle, None, 10, C.byref(required)), 1)

    def test_unknown_style_requires_explicit_policy(self):
        self.connection.row_factory = sqlite3.Row
        row = self.connection.execute("SELECT tactics FROM club_states WHERE team_id='home'").fetchone()
        tactics = json.loads(row[0]); tactics["style"] = "Unmapped legacy style"
        self.connection.execute("UPDATE club_states SET tactics=? WHERE team_id='home'", (json.dumps(tactics),))
        self.connection.commit()
        with readonly_snapshot(self.db) as (c, _):
            with self.assertRaises(ValueError): convert_team(c, "home")
            result = convert_team(c, "home", "balanced")
            self.assertEqual(result["tactics"]["play_style"], "balanced")
            self.assertTrue(result["source_metadata"]["warnings"])

    def test_missing_heading_not_fabricated_and_no_output_to_source(self):
        body = json.loads(self.connection.execute("SELECT body FROM club_players WHERE id='home:0'").fetchone()[0])
        del body["heading"]
        self.connection.execute("UPDATE club_players SET body=? WHERE id='home:0'", (json.dumps(body),))
        self.connection.commit()
        with readonly_snapshot(self.db) as (c, _):
            with self.assertRaises(ValueError): convert_team(c, "home")
        with self.assertRaises(ValueError): write_output(self.folder / "forbidden.json", {}, self.db)
        self.assertFalse((self.folder / "forbidden.json").exists())

    def test_wal_snapshot_includes_committed_rows(self):
        self.connection.execute("PRAGMA journal_mode=WAL")
        self.connection.execute("PRAGMA wal_autocheckpoint=0")
        self.connection.execute("UPDATE league_teams SET rating=88 WHERE id='home'")
        self.connection.commit()
        self.assertTrue(Path(str(self.db)+"-wal").is_file())
        with readonly_snapshot(self.db) as (c, proof):
            self.assertEqual(convert_team(c, "home")["rating"], 88)
        self.assertIn("-wal", proof["source_files"])
        self.assertTrue(proof["source_unchanged"])

    def test_bucket_boundaries(self):
        self.assertEqual([bucket(n, ["low", "medium", "high"]) for n in [0, 33, 34, 66, 67, 100]], ["low", "low", "medium", "medium", "high", "high"])

    def test_replay_c_api_roundtrip_and_validation(self):
        data = self.export()
        with self.library.create(data) as match:
            initial = decode_fsim(match.replay_bytes())
            self.assertEqual(len(initial["frames"]), 1)
            self.assertIsNone(match.goals()["latest"])
            match.tick(1800)
            encoded = match.replay_bytes()
            replay = decode_fsim(encoded)
            self.assertEqual(len(replay["frames"]), 301)
            self.assertEqual([replay["frames"][i]["tick"] for i in (0, -1)], [900, 1800])
            self.assertEqual(replay["frames"][-1]["score"], match.snapshot()["score"])
            for actual, expected in zip(replay["frames"][-1]["ball"], match.snapshot()["ball"]):
                self.assertAlmostEqual(actual, expected, places=4)
            self.assertEqual(match.goals(), match.goals())  # polling never consumes a goal
            required = C.c_size_t()
            output = (C.c_uint8 * 4)(1, 2, 3, 4)
            self.assertEqual(self.library.api.export_match_replay(match.handle, output, 4, C.byref(required)), 3)
            self.assertEqual(bytes(output), b'\x01\x02\x03\x04')
            self.assertEqual(required.value, len(encoded))
            self.assertEqual(self.library.api.export_match_replay(match.handle, None, 10, C.byref(required)), 1)
            for corrupt in (b'BAD!' + encoded[4:], encoded[:-1], encoded + b'x'):
                with self.assertRaises(ValueError): decode_fsim(corrupt)
            match.tick(54000)
            fulltime = match.snapshot()
            self.assertGreaterEqual(match.goals()["sequence"], 0)
            self.assertEqual(match.goals()["sequence"], sum(fulltime["score"]))
            final_bytes = match.replay_bytes()
            final_replay = decode_fsim(final_bytes)
            self.assertEqual([final_replay["frames"][i]["tick"] for i in (0, -1)], [53100, 54000])
            match.tick(100)
            self.assertEqual(final_bytes, match.replay_bytes())
            stats = fulltime["statistics"]
            # A tactical match can legitimately contain no dribbling decisions.
            self.assertGreater(stats["actions"]["short_pass"], 0)
            self.assertGreater(stats["actions"]["off_ball_movement"], 0)
            self.assertEqual(sum(stats["shots"]), stats["actions"]["shoot"])
            self.assertTrue(all(0 <= stats["shots_on_target"][i] <= stats["shots"][i] for i in range(2)))

    def test_simulation_seed_repeatability(self):
        data = self.export()
        with self.library.create(data) as single, self.library.create(data) as chunked:
            single.tick(3000)
            for _ in range(100): chunked.tick(30)
            self.assertEqual(single.snapshot(), chunked.snapshot())
            self.assertEqual(single.replay_bytes(), chunked.replay_bytes())
            data['seed'] = '12345'
            with self.library.create(data) as different:
                different.tick(3000)
                self.assertNotEqual(single.snapshot()['ball'], different.snapshot()['ball'])

    def test_goal_hold_reports_stopped_players(self):
        data = self.export()
        # Deliberately weak defending fixture exercises a goal transition;
        # ordinary balanced games are allowed to finish 0-0.
        for p in data['teams'][1]['players']:
            for key in ('speed', 'acceleration', 'tackling', 'positioning', 'goalkeeping'):
                p['attributes'][key] = 0
        with self.library.create(data) as match:
            for _ in range(18000):
                match.tick(3)
                if match.goals()['sequence']:
                    break
            self.assertGreater(match.goals()['sequence'], 0)
            match.tick(1)
            state = match.snapshot()
            self.assertGreater(state['restart_ticks_remaining'], 0)
            self.assertTrue(all(p['velocity'] == [0, 0, 0] for p in state['players']))
            self.assertEqual(state['ball_velocity'], [0, 0, 0])


if __name__ == "__main__":
    unittest.main(verbosity=2)
