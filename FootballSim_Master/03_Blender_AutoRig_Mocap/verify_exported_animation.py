import bpy,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
bpy.ops.import_scene.fbx(filepath=str(root/'04_Exported_GameAssets/animations_locomotion/sprint.fbx'))
rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
report={'rig':rig.name,'bones':[b.name for b in rig.data.bones],'actions':[a.name for a in bpy.data.actions], 'samples':[]}
for frame in [1,5,10,15,20]:
    bpy.context.scene.frame_set(frame);bpy.context.view_layer.update()
    report['samples'].append({'frame':frame,'bones':{b.name:[list(row) for row in b.matrix] for b in rig.pose.bones}})
(root/'00_Specs_and_Logs/stage3_2_fbx_animation_audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('SPRINT_POSE_CHANGES',report['samples'][0]['bones']!=report['samples'][2]['bones'])
