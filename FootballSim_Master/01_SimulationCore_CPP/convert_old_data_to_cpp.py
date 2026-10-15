"""Export SQLite club data to fsim.match_input.v1 without touching the old repo.

Only standard-library modules are used. The source DB/WAL are copied as files
to a verified-stable private snapshot; SQLite only opens that copy, mode=ro.
No old backend imports, migrations, checkpoints, or production SQL writes.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import hashlib
import json
import math
from pathlib import Path
import shutil
import sqlite3
import tempfile

CORE = Path(__file__).resolve().parent
DEFAULT_DB = Path(r"D:\Hadi\proyek\manajer-sepak-bola\backend\football_manager.db")
SCHEMA = json.loads((CORE.parent / "00_Specs_and_Logs/match_engine_schema.json").read_text(encoding="utf-8"))
STYLE_MAP = {
    "Balanced": "balanced", "Possession": "possession", "Counter": "counter_attack",
    "Pressing": "high_press", "Wide": "wing_play", "Direct": "direct_pass",
    "Tekanan Tinggi (High-Press)": "high_press",
    "Penguasaan Bola (Possession)": "possession",
    "Serangan Balik Cepat (Counter)": "counter_attack",
    "Kombinasi Taktikal Seimbang": "balanced",
    "Serangan Sayap & Umpan Silang": "wing_play",
    "Direct Play Intensitas": "direct_pass",
}
STYLE_MAP.update({value: value for value in SCHEMA["tactics"]["play_style"]})
SPECIALISTS = dict(zip(SCHEMA["specialists"], ("captain", "penalty", "freeKick", "cornerLeft", "cornerRight")))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def source_root(database):
    return database.parent.parent if database.parent.name.lower() == "backend" else database.parent


def outside_source(path, database):
    path = Path(path).resolve()
    require(not path.is_relative_to(source_root(database)), "Output/staging must be outside the source repository")
    return path


def source_manifest(database):
    result = {}
    require(database.is_file(), f"Database not found: {database}")
    require(not Path(str(database) + "-journal").exists(), "Rollback journal exists; retry after writer finishes")
    for suffix in ("", "-wal"):
        path = Path(str(database) + suffix)
        if path.exists():
            before = path.stat()
            digest = sha256(path)
            after = path.stat()
            require((before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns), "Source changed while hashing; retry when stable")
            result[suffix] = {"bytes": after.st_size, "mtime_ns": after.st_mtime_ns, "sha256": digest}
    return result


@contextmanager
def readonly_snapshot(database, snapshot_root=None):
    database = Path(database).resolve()
    base = outside_source(snapshot_root or CORE / "build/sqlite_snapshots", database)
    base.mkdir(parents=True, exist_ok=True)
    original = source_manifest(database)
    original_sidecars = sorted(p.name for p in database.parent.glob(database.name + "*"))
    with tempfile.TemporaryDirectory(prefix="read-", dir=base) as directory:
        # Verify the exact deletion target before TemporaryDirectory cleanup.
        private = Path(directory).resolve()
        require(private.is_relative_to(base) and private != base, "Unsafe snapshot directory")
        local = private / "source.db"
        for suffix in original:
            shutil.copyfile(Path(str(database) + suffix), Path(str(local) + suffix))
            require(sha256(Path(str(local) + suffix)) == original[suffix]["sha256"], "Source changed while copying; retry when stable")
        require(source_manifest(database) == original, "Source DB/WAL changed during snapshot; retry when stable")
        connection = sqlite3.connect(local.as_uri() + "?mode=ro", uri=True, timeout=5)
        connection.row_factory = sqlite3.Row
        proof = {"source_database": str(database), "source_files": original, "source_repo_writes": False,
                 "sqlite_open_mode": "mode=ro and query_only on private DB/WAL copy", "source_unchanged": False}
        try:
            connection.execute("PRAGMA query_only=ON")
            require(connection.execute("PRAGMA quick_check").fetchone()[0] == "ok", "Copied SQLite database failed quick_check")
            connection.execute("BEGIN")
            yield connection, proof
        finally:
            connection.close()
        require(source_manifest(database) == original, "Source changed during read; export aborted, retry when stable")
        require(original_sidecars == sorted(p.name for p in database.parent.glob(database.name + "*")), "Source sidecar set changed during read; retry when stable")
        proof["source_unchanged"] = True


def text(value, field, allow_empty=False):
    require(isinstance(value, str) and "\0" not in value and len(value.encode("utf-8")) <= 256 and (allow_empty or value), f"{field}: invalid string")
    return value


def numeric(value, field, low=0, high=100):
    require(type(value) in (int, float) and math.isfinite(value) and low <= value <= high, f"{field}: expected finite number in {low}..{high}")
    return value


def intensity(value, field):
    require(type(value) is int and 0 <= value <= 100, f"{field}: expected integer 0..100")
    return value


def bucket(value, names):
    return names[0 if value <= 33 else 1 if value <= 66 else 2]


def list_teams(connection):
    return [dict(row) for row in connection.execute("""
        SELECT t.id,t.league_id,t.team_name,COUNT(p.id) roster_size
        FROM league_teams t JOIN club_states s ON s.team_id=t.id
        LEFT JOIN club_players p ON p.team_id=t.id
        GROUP BY t.id,t.league_id,t.team_name ORDER BY t.league_id,t.id
    """)]


def convert_team(connection, team_id, unknown_style="error"):
    require(unknown_style in ("error", "balanced"), "Unknown style policy")
    row = connection.execute("""
        SELECT t.id,t.league_id,t.team_name,t.team_short_name,t.rating,
               s.revision,s.fitness,s.tactics,s.lineup,s.league_id state_league_id
        FROM league_teams t JOIN club_states s ON s.team_id=t.id WHERE t.id=?
    """, (team_id,)).fetchone()
    require(row is not None, f"Club/state not found: {team_id}")
    require(row["state_league_id"] == row["league_id"], "Club state league mismatch")
    original = json.loads(row["tactics"])
    lineup = json.loads(row["lineup"])
    require(isinstance(original, dict), "tactics must be an object")
    require(isinstance(lineup, list) and len(lineup) == 11 and all(isinstance(i, str) for i in lineup) and len(set(lineup)) == 11, "Expected 11 distinct lineup IDs")
    warnings = []
    players = []
    for record in connection.execute("SELECT id,league_id,body FROM club_players WHERE team_id=? ORDER BY id", (team_id,)):
        p = json.loads(record["body"])
        require(isinstance(p, dict) and p.get("id") == record["id"] and record["league_id"] == row["league_id"], "Player JSON/SQL identity mismatch")
        attributes = {}
        for key in SCHEMA["player_attributes"]:
            old_key = "control" if key == "ball_control" else key
            require(old_key in p, f"Player {record['id']} missing attribute {old_key}; no synthetic value is invented")
            attributes[key] = numeric(p[old_key], f"{record['id']}.{old_key}")
        anchor = p.get("anchor")
        require(isinstance(anchor, list) and len(anchor) == 2, "Player anchor must be [x,z]")
        numeric(anchor[0], "anchor.x", -52.5, 52.5); numeric(anchor[1], "anchor.z", -34, 34)
        require(p.get("role") in ("GK", "DF", "MF", "FW"), "Unsupported player role")
        require(type(p.get("number")) is int and 0 <= p["number"] <= 2**32-1, "Invalid shirt number")
        secondary = p.get("secondaryPositions", [])
        require(isinstance(secondary, list), "secondaryPositions must be an array")
        players.append({"id": text(p["id"], "player.id"), "name": text(p["name"], "player.name"),
                        "number": p["number"], "role": p["role"],
                        "primary_position": text(p.get("primaryPosition", ""), "primaryPosition", True),
                        "secondary_positions": [text(v, "secondary position") for v in secondary],
                        "anchor": anchor, "attributes": attributes})
    ids = {p["id"] for p in players}
    require(11 <= len(players) <= 64 and len(ids) == len(players), "Expected 11..64 distinct roster players")
    require(set(lineup) <= ids, "Lineup refers to a missing/transferred player")
    formation = original.get("formation")
    require(formation in SCHEMA["formations"], f"Unsupported formation {formation}")
    style = original.get("style")
    require(isinstance(style, str), "style must be a string")
    mapped_style = STYLE_MAP.get(style)
    if mapped_style is None:
        require(unknown_style == "balanced", f"Unmapped style {style!r}; provide an explicit alias or use --unknown-style balanced")
        mapped_style = "balanced"
        warnings.append(f"Unmapped style {style!r} explicitly defaulted to balanced; original retained")
    mentality = original.get("mentality")
    require(mentality in ("Defensive", "Balanced", "Attacking"), f"Unsupported mentality {mentality}")
    pressing = intensity(original.get("pressing"), "pressing")
    tempo = intensity(original.get("tempo"), "tempo")
    width = intensity(original.get("width"), "width")
    specialists, backups = {}, {}
    for target, old in SPECIALISTS.items():
        value = original.get(old, original.get("corner") if old in ("cornerLeft", "cornerRight") else None)
        require(isinstance(value, str) and value in lineup, f"{old} must identify a starter")
        specialists[target] = value
        backup = original.get(old + "Backup")
        if backup is not None:
            require(isinstance(backup, str) and backup in ids and backup != value, f"Invalid {old}Backup")
            backups[target] = backup
    tactics = {"formation": formation, "play_style": mapped_style, "mentality": mentality.lower(),
               "pressing": bucket(pressing, SCHEMA["tactics"]["pressing"]),
               "tempo": bucket(tempo, SCHEMA["tactics"]["tempo"]), "pressing_intensity": pressing,
               "tempo_intensity": tempo, "width": width, "specialists": specialists, "specialist_backups": backups}
    require(type(row["revision"]) is int and 0 <= row["revision"] <= 2**32-1, "Invalid club revision")
    return {"id": text(row["id"], "team.id"), "league_id": text(row["league_id"], "league_id"),
            "name": text(row["team_name"], "team.name"), "short_name": text(row["team_short_name"], "short_name", True),
            "rating": numeric(row["rating"], "team.rating"), "fitness": numeric(row["fitness"], "team.fitness"),
            "club_revision": row["revision"], "tactics": tactics, "players": players, "lineup": lineup,
            "source_metadata": {"legacy_tactics": original, "attribute_alias": {"control": "ball_control"}, "warnings": warnings}}


def convert_match(connection, home_team, away_team, seed=0, unknown_style="error"):
    require(home_team != away_team, "Home and away must differ")
    require(type(seed) is int and 0 <= seed <= 2**64-1, "seed must fit uint64")
    teams = [convert_team(connection, identity, unknown_style) for identity in (home_team, away_team)]
    require(not ({p["id"] for p in teams[0]["players"]} & {p["id"] for p in teams[1]["players"]}), "Player ID overlaps between clubs")
    return {"input_version": "fsim.match_input.v1", "seed": str(seed), "teams": teams,
            "conversion": {"adapter_version": "sqlite-to-fsim-1", "unknown_style_policy": unknown_style,
                           "numeric_bucket_policy": "0..33 low/slow; 34..66 medium/normal; 67..100 high/fast; exact values also retained",
                           "fitness_policy": "stored club_states.fitness; no time recovery or training mutation",
                           "attributes_policy": "raw roster attributes; positional penalties are not applied or fabricated",
                           "legacy_replay_compatible": False}}


def write_output(path, value, database, force=False):
    path = outside_source(path, database)
    require(not path.exists() or force, f"Output exists; use --force to replace: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w" if force else "x", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", type=Path, default=DEFAULT_DB)
    parser.add_argument("--list-teams", action="store_true")
    parser.add_argument("--home-team")
    parser.add_argument("--away-team")
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--unknown-style", choices=("error", "balanced"), default="error")
    parser.add_argument("--output", type=Path, default=CORE / "replay_samples/converted_match_input.json")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    try:
        database = args.db.resolve()
        if not args.list_teams:
            require(args.home_team and args.away_team, "Supply --home-team and --away-team, or --list-teams")
            outside_source(args.output, database)
            if args.report:
                outside_source(args.report, database)
                require(args.report.resolve() != args.output.resolve(), "Report and output paths must differ")
        with readonly_snapshot(database) as (connection, proof):
            if args.list_teams:
                result = list_teams(connection)
            else:
                result = convert_match(connection, args.home_team, args.away_team, args.seed, args.unknown_style)
        if args.list_teams:
            print(json.dumps(result, ensure_ascii=True, indent=2))
            return 0
        result["conversion"]["source_read"] = proof
        write_output(args.output, result, database, args.force)
        report = {"status": "PASS", "output": str(args.output.resolve()), "clubs": 2,
                  "roster_players": sum(len(t["players"]) for t in result["teams"]), "starting_players": 22,
                  "warnings": [w for t in result["teams"] for w in t["source_metadata"]["warnings"]], "source_read": proof}
        if args.report:
            write_output(args.report, report, database, args.force)
        print(json.dumps(report, ensure_ascii=True, indent=2))
        return 0
    except (ValueError, OSError, sqlite3.Error, KeyError, TypeError) as error:
        parser.exit(1, f"Conversion failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
