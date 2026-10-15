import json
from pathlib import Path
from match_bridge import MatchLibrary
from read_fsim import decode_fsim
from highlight_slicer import slice_highlights, legacy_clip

ROOT=Path(__file__).resolve().parent
data=json.loads((ROOT/'replay_samples/converted_match_input.json').read_text(encoding='utf-8-sig'))
data['seed']='31001'
lib=MatchLibrary(ROOT/'build/engine/Release/match_bridge.dll')
with lib.create(data) as match:
    match.tick(54000)
    snapshot=match.snapshot()
    archive=match.replay_bytes(archive=True)
replay=decode_fsim(archive)
goals=[i for i,f in enumerate(replay['frames']) if f['flags']&1]
first_end=min(18000,goals[0]+20) if goals else 300
outputs={'highlights_15min.fsim':slice_highlights(archive), 'first_goal_last15s.fsim':legacy_clip(archive,first_end), 'fulltime_last15s.fsim':legacy_clip(archive,18000)}
for dest in (ROOT/'replay_samples/stage2_1', ROOT.parent/'05_UE5_MatchViewer_3D/Content/Replays'):
    for name,blob in outputs.items(): (dest/name).write_bytes(blob)
timeline=decode_fsim(outputs['highlights_15min.fsim'])['metadata']
report={'seed':data['seed'],'score':snapshot['score'],'statistics':snapshot['statistics'],'timeline_seconds':timeline['timeline_seconds'],
        'segments':timeline['segments'],'archive_frames':len(replay['frames']),'goals':[replay['frames'][i]['tick'] for i in goals],
        'files':{name:len(blob) for name,blob in outputs.items()}}
(ROOT.parent/'00_Specs_and_Logs/stage3_1_highlights.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
(ROOT/'replay_samples/stage2_1/snapshots.json').write_text(json.dumps({'fulltime':snapshot},indent=2),encoding='utf-8')
(ROOT/'replay_samples/stage2_1/match_events.json').write_text(json.dumps({'seed':data['seed'],'events':[{'tick':f['tick'],'flags':f['flags'],'score':f['score']} for f in replay['frames'] if f['flags']&57]},indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k!='segments'},indent=2))
