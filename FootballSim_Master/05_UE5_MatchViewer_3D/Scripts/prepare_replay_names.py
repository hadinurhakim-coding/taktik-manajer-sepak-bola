"""Enrich replay identity metadata from the converted roster; binary frames stay intact."""
import json
import sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'01_SimulationCore_CPP'))
from read_fsim import decode_fsim, HEADER
from highlight_slicer import encode_metadata

data=json.loads((root/'01_SimulationCore_CPP/replay_samples/converted_match_input.json').read_text(encoding='utf-8-sig'))
names={p['id']:p['name'] for t in data['teams'] for p in t['players']}
for folder in [root/'01_SimulationCore_CPP/replay_samples/stage2_1',root/'05_UE5_MatchViewer_3D/Content/Replays']:
    for path in folder.glob('*.fsim'):
        before=path.read_bytes(); replay=decode_fsim(before); meta=replay['metadata']
        for p in meta['players']: p['name']=names.get(p['id'],p['id'])
        version=HEADER.unpack_from(before)[1]
        after=encode_metadata(before,meta,version)
        assert before[40+HEADER.unpack_from(before)[3]:]==after[40+HEADER.unpack_from(after)[3]:]
        decode_fsim(after); path.write_bytes(after)
        print('Updated names; frames unchanged:',path.name)
