"""Reopen the new .blend, audit deformation and render sampled actions."""
import json
from pathlib import Path
import sys
import bpy
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'03_Blender_AutoRig_Mocap/fresh_character'
sys.path.insert(0,str(Path(__file__).parent))
from inspect_fresh_character import setup_render,aim

def main():
    bpy.ops.wm.open_mainfile(filepath=str(WORK/'Footballer01_rigged.blend'))
    rig=bpy.data.objects['Footballer01_Rig'];mesh=bpy.data.objects['Footballer01_LOD0']
    report={'status':'RUNNING','bones':len(rig.data.bones),'actions':[], 'deformation_checks':[]}
    sums=[sum(g.weight for g in v.groups) for v in mesh.data.vertices]
    report['unweighted_vertices']=sum(s<.999 for s in sums)
    report['max_weight_error']=max(abs(s-1) for s in sums)
    assert report['unweighted_vertices']==0 and report['max_weight_error']<1e-4
    rest=np.array([v.co[:] for v in mesh.data.vertices]);edges=np.array([e.vertices[:] for e in mesh.data.edges]);rest_lengths=np.linalg.norm(rest[edges[:,0]]-rest[edges[:,1]],axis=1)
    graph=bpy.context.evaluated_depsgraph_get()
    for action in bpy.data.actions:
        if action.name not in {e['name'] for e in json.loads((ROOT/'04_Exported_GameAssets/Footballer01/asset_manifest.json').read_text())['animations']}:continue
        rig.animation_data.action=action
        if action.slots:rig.animation_data.action_slot=action.slots[0]
        for frame in [1,int(action.frame_range[1]*.25),int(action.frame_range[1]*.5),int(action.frame_range[1]*.75)]:
            bpy.context.scene.frame_set(frame);bpy.context.view_layer.update();evaluated=mesh.evaluated_get(graph);data=evaluated.to_mesh()
            positions=np.array([v.co[:] for v in data.vertices]);evaluated.to_mesh_clear()
            ratios=np.linalg.norm(positions[edges[:,0]]-positions[edges[:,1]],axis=1)/np.maximum(rest_lengths,1e-5)
            check={'action':action.name,'frame':frame,'finite':bool(np.isfinite(positions).all()),'min_z':float(positions[:,2].min()),'max_edge_stretch':float(ratios.max()),'p99_edge_stretch':float(np.quantile(ratios,.99))}
            check['pass']=check['finite'] and check['p99_edge_stretch']<2.0
            check['root_translation_m']=float(rig.pose.bones['Root'].location.length)
            check['pass']=check['pass'] and check['root_translation_m']<1e-5
            report['deformation_checks'].append(check)
        report['actions'].append(action.name)
    scene,camera=setup_render(WORK/'previews');scene.render.resolution_x=700;scene.render.resolution_y=900;scene.cycles.samples=16
    camera.location=(2,-4,1.2);aim(camera,(0,0,.9));camera.data.ortho_scale=2.2
    for name in ['idle','jog','sprint','shot','gk_ready','gk_dive_left']:
        action=bpy.data.actions[name];rig.animation_data.action=action
        if action.slots:rig.animation_data.action_slot=action.slots[0]
        scene.frame_set(int(action.frame_range[1]*(.48 if name!='jog' else .2)))
        scene.render.filepath=str(WORK/'previews'/(name+'.png'));bpy.ops.render.render(write_still=True)
    report['status']='NUMERICAL_CHECKS_PASS_REQUIRES_VISUAL_REVIEW' if all(c['pass'] for c in report['deformation_checks']) else 'FAILED_DEFORMATION'
    (ROOT/'00_Specs_and_Logs/footballer01_deformation_audit.json').write_text(json.dumps(report,indent=2))
    print('DEFORMATION_AUDIT_COMPLETE',flush=True)
    assert report['status']!='FAILED_DEFORMATION','Inspect deformation audit and preview images'

if __name__=='__main__':main()
