"""Run the real converted clubs for 90 game minutes; export and verify FSIM clips."""
import hashlib
import json
import math
from pathlib import Path
import time
from datetime import datetime, timezone

from match_bridge import MatchLibrary
from read_fsim import decode_fsim

CORE = Path(__file__).resolve().parent


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def save_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + '\n', encoding='utf-8')


def main():
    input_path = CORE / 'replay_samples/converted_match_input.json'
    data = json.loads(input_path.read_text(encoding='utf-8'))
    source = Path(data['conversion']['source_read']['source_database'])
    source_before = digest(source)
    output = CORE / 'replay_samples/stage2_1'
    output.mkdir(parents=True, exist_ok=True)
    library_path = CORE / 'build/engine/Release/match_bridge.dll'
    library = MatchLibrary(library_path)
    goals, snapshots, moved = [], {}, set()
    first_goal_clip, clip_at, ball_moved = None, None, False
    start = time.perf_counter()
    with library.create(data) as match:
        snapshots['initial'] = initial = match.snapshot()
        for tick in range(3, 54001, 3):
            match.tick(3)
            status = match.goals()
            if status['sequence'] > len(goals):
                assert status['sequence'] == len(goals) + 1, 'Missed goal event'
                goals.append({'score': status['score'], **status['latest']})
                if clip_at is None:
                    clip_at = min(54000, tick + 60)
            if tick == clip_at:
                first_goal_clip = match.replay_bytes()
                clip = decode_fsim(first_goal_clip)
                assert any(f['flags'] & 1 for f in clip['frames'])
                (output / 'first_goal_last15s.fsim').write_bytes(first_goal_clip)
            if tick % 60 == 0:
                snapshot = match.snapshot()
                assert all(math.isfinite(v) for v in snapshot['ball'] + snapshot['ball_velocity'])
                assert snapshot['ball'][1] >= 0.11 - 1e-8
                ball_moved |= snapshot['ball'] != initial['ball']
                for i, player in enumerate(snapshot['players']):
                    x, y, z = player['position']
                    assert all(math.isfinite(v) for v in player['position'] + player['velocity'])
                    assert abs(x) <= 52.5 and y == 0 and abs(z) <= 34
                    if player['position'] != initial['players'][i]['position']:
                        moved.add(i)
                assert sum(snapshot['score']) == status['sequence']
                if tick == 27000:
                    snapshots['halftime'] = snapshot
                if tick == 54000:
                    snapshots['fulltime'] = snapshot
        replay = match.replay_bytes()
        final = snapshots['fulltime']
        assert final['phase'] == 'completed' and final['game_elapsed_seconds'] == 5400 and final['real_elapsed_seconds'] == 900
        assert len(goals) > 0 and len(moved) == 22 and ball_moved
        assert all(count > 0 for count in final['statistics']['actions'].values())
        match.tick(99)
        assert match.snapshot() == final and match.replay_bytes() == replay
    elapsed = time.perf_counter() - start
    decoded = decode_fsim(replay)
    assert len(decoded['frames']) == 301
    assert decoded['frames'][0]['tick'] == 53100 and decoded['frames'][-1]['tick'] == 54000
    assert decoded['frames'][-1]['score'] == final['score']
    with library.create(data) as repeated:
        repeated.tick(54000)
        assert repeated.snapshot() == final and repeated.replay_bytes() == replay
    source_after = digest(source)
    assert source_before == source_after
    (output / 'fulltime_last15s.fsim').write_bytes(replay)
    save_json(output / 'snapshots.json', snapshots)
    save_json(output / 'match_events.json', {'seed': data['seed'], 'goals': goals})
    artifacts = {p.name: {'path': str(p), 'bytes': p.stat().st_size, 'sha256': digest(p)} for p in sorted(output.iterdir()) if p.is_file()}
    report = {
        'status': 'PASS', 'stage': '2.1', 'checked_at_utc': datetime.now(timezone.utc).isoformat(),
        'input': str(input_path), 'seed': data['seed'], 'teams': [t['name'] for t in data['teams']],
        'score': final['score'], 'goal_events': len(goals), 'statistics': final['statistics'],
        'clock': {'ticks': 54000, 'presentation_seconds': 900, 'game_seconds': 5400, 'scale': 6},
        'players_observed_moving': len(moved), 'ball_observed_moving': ball_moved,
        'simulator_and_polling_wall_seconds': elapsed, 'repeatability_same_build': 'bit-identical snapshots and replay for batched vs three-tick calls',
        'replay': {'format': 'FSIM v1 little-endian', 'frames': 301, 'sample_presentation_seconds': 0.05,
                   'window_presentation_seconds': 15, 'first_tick': 53100, 'last_tick': 54000, 'reference_reader_roundtrip': 'PASS'},
        'source_database_unchanged': source_before == source_after, 'source_database_sha256': source_after,
        'library_sha256': digest(library_path), 'artifacts': artifacts,
        'limitations': ['Prototype AI generates too many goals; football balance is not calibrated.',
                       'No offside, fouls/cards, substitutions, full set-piece laws or physical post/net collisions.',
                       'Restart repositioning is discontinuous and flagged in replay; the game clock advances through brief restart holds.',
                       'The ring retains only the latest 15 presentation seconds, not the whole match.',
                       'UE5.8/Android readers and rendering are not implemented or tested.',
                       'Determinism is verified for the same Windows x64 build; cross-platform floating point identity is not promised.']
    }
    report_path = CORE.parent / '00_Specs_and_Logs/stage2_1_simulation.json'
    save_json(report_path, report)
    print(json.dumps({k: report[k] for k in ('status', 'teams', 'score', 'goal_events', 'statistics', 'clock', 'players_observed_moving', 'replay', 'source_database_unchanged')}, indent=2))
    print(f'Report: {report_path}')


if __name__ == '__main__':
    main()
