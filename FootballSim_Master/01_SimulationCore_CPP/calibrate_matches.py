"""Fixed, consecutive seeds; records every outcome, never selects favourable results."""
import argparse
import json
from pathlib import Path
from statistics import mean
from match_bridge import MatchLibrary

ROOT = Path(__file__).resolve().parent

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--count', type=int, default=10)
    parser.add_argument('--seed', type=int, default=31001)
    args = parser.parse_args()
    data = json.loads((ROOT / 'replay_samples/converted_match_input.json').read_text(encoding='utf-8-sig'))
    lib = MatchLibrary(ROOT / 'build/engine/Release/match_bridge.dll')
    rows = []
    for seed in range(args.seed, args.seed + args.count):
        data['seed'] = str(seed)
        with lib.create(data) as match:
            match.tick(54000)
            s = match.snapshot()
            rows.append({'seed': seed, 'score': s['score'], **s['statistics']})
            print(json.dumps(rows[-1]), flush=True)
    report = {'seeds': [args.seed, args.seed + args.count - 1], 'matches': rows,
              'mean_total_goals': mean(sum(r['score']) for r in rows),
              'mean_shots_per_team': [mean(r['shots'][i] for r in rows) for i in range(2)],
              'all_matches_in_target': all(sum(r['score']) <= 5 and all(8 <= n <= 22 for n in r['shots']) for r in rows)}
    path = ROOT.parent / '00_Specs_and_Logs/stage3_1_calibration.json'
    path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k != 'matches'}), flush=True)

if __name__ == '__main__': main()
