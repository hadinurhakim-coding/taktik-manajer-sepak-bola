"""Verify captured runtime evidence and save a reproducible stage report."""
import hashlib
import json
import math
import re
import struct
from pathlib import Path

root=Path(__file__).resolve().parents[2]
logs=root/'00_Specs_and_Logs'
def vector(text): return [float(v) for v in re.findall(r'=(-?[\d.]+)',text)]
captures=[]
for name,fov in [('stage3_2_broadcast_3d',35),('stage3_2_replay_closeup',50)]:
    data=json.loads((logs/(name+'.json')).read_text(encoding='utf-8-sig'))
    pixels=(logs/(name+'.png')).read_bytes()
    assert pixels[:8]==b'\x89PNG\r\n\x1a\n'
    dimensions=struct.unpack('>II',pixels[16:24]); assert dimensions==(1600,900)
    assert data['projection']=='Perspective' and data['fov']==fov
    assert data['skeletal_players']==22 and len(data['players'])==22
    assert len(data['bone_names'])>=24 and 'Head' in data['bone_names']
    assert all(any(abs(v)>0 for v in vector(p['head'])) for p in data['players'])
    camera=vector(data['camera_position']); ball=vector(data['ball_position'])
    if fov==35:
        assert camera[1]==-5000 and camera[2]==1650
        pitch=vector(data['camera_rotation'])[0]; assert -26.01<=pitch<=-22.99
        assert not data['low_angle'] and data['effective_rate']==1
    else:
        assert 180<=camera[2]<=300 and data['low_angle'] and data['effective_rate']==0.5
        assert 799.9<=math.dist(camera,ball)<=1200.1
    assert data['playing']
    captures.append({'path':str(logs/(name+'.png')),'resolution':dimensions,'sha256':hashlib.sha256(pixels).hexdigest(),
                     'fov':fov,'camera':camera,'match_tick':data['source_tick'],'playback_rate':data['effective_rate'],
                     'skeletal_players':22,'animation_states':sorted({p['animation'] for p in data['players']})})
runtime=(logs/'stage3_2_capture_runtime.log').read_text(encoding='utf-8-sig',errors='replace')
for failure in ['Failed to compile Material','has no skeleton','missing usage flag','LogScript: Error']:
    assert failure not in runtime, failure
assert 'STAGE32_CAPTURE_COMPLETE' in runtime
build=(logs/'stage3_2_ue_build.log').read_text(encoding='utf-8-sig',errors='replace')
assert 'Result: Succeeded' in build
test=(logs/'stage3_2_tests.log').read_text(encoding='utf-8-sig',errors='replace')
assert test.count('Test Completed. Result={Success}')==5 and 'Result={Fail}' not in test
core=(logs/'stage3_2_core_tests.log').read_text(encoding='utf-8-sig',errors='replace')
assert '100% tests passed' in core
assets=root/'05_UE5_MatchViewer_3D/Content/Football'
assert (assets/'Characters/SK_NicoPaz_Skeleton.uasset').stat().st_size>1000
assert len(list((assets/'Animations').glob('*.uasset')))==27
source=json.loads((root/'04_Exported_GameAssets/asset_manifest.json').read_text(encoding='utf-8-sig'))
unchanged={}
for path,expected in source['source_sha256'].items():
    with Path(path).open('rb') as f: actual=hashlib.file_digest(f,'sha256').hexdigest()
    unchanged[path]=actual==expected
assert all(unchanged.values())
report={'stage':'3.2','status':'SUCCESS','engine':'Unreal Engine 5.8.3',
        'ue_build':'SUCCESS','ue_tests_passed':5,'native_test_groups_passed':4,
        'mouse_keyboard_automation_used':False,'capture_method':'FScreenshotRequest / RenderOffscreen / hidden process',
        'captures':captures,'assets':{'skeletal_players':22,'bind_height_cm':180,'mesh_lods':3,'imported_animation_assets':27},
        'scene':['perspective broadcast camera','low action replay camera','lit turf and dynamic shadows','3D goal frames and nets',
                 'perimeter adboards','four-sided tiered stands','instanced seats and procedural spectators','240x150 Slate radar','possession nameplates'],
        'repairs':['persisted missing skeleton dependency','restored shared-skeleton animation imports','explicit PBR texture export and kit shader',
                   'cancelled horizontal hip travel to preserve FSIM position','vertical lens shift preserves framing with prescribed camera pitch'],
        'source_blend_unchanged':unchanged,'render_errors':[],
        'limitations':['All 22 players share the NicoPaz base mesh; sample roster names are synthetic.',
                       'Stadium and spectators use simple procedural geometry, not finished production art.',
                       'Offline replay viewer; no Shipping or Android package produced.']}
(logs/'stage3_2_report.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
print(json.dumps({'status':report['status'],'ue_tests':5,'native_tests':4,'screenshots':[c['path'] for c in captures],'source_blend_unchanged':all(unchanged.values())},indent=2))
