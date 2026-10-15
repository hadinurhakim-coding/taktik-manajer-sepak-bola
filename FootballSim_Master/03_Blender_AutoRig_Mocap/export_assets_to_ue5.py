"""blender --background --factory-startup --disable-autoexec --python this_file

Append source datablocks only; never save the source .blend or execute its code.
Exports one common skeleton, skinned LOD meshes and 27 individual action FBXs.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy
from mathutils import Matrix

ROOT = Path(__file__).resolve().parents[1]
GROUPS = {
    'animations_locomotion': 'idle jog sprint turn_left turn_spin turn_sprint turn_walk defend_stance defend_shuffle'.split(),
    'animations_onball': 'dribble pass shot header receive shield skill_juggle tackle_stand tackle_lunge tackle_slide react_miss'.split(),
    'animations_goalkeeper': 'gk_ready gk_dive_left gk_dive_right gk_high gk_smother gk_pass'.split(),
    'animations_setpieces': ['penalty'],
}

def sha(path):
    with path.open('rb') as f: return hashlib.file_digest(f, 'sha256').hexdigest()

def select(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.hide_set(False); obj.hide_viewport = False; obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]

def export(path, objects, animation=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    select(objects)
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True,
        object_types={'ARMATURE','MESH'}, global_scale=1.0, apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_NONE', axis_forward='-Y', axis_up='Z',
        add_leaf_bones=False, use_armature_deform_only=False, armature_nodetype='NULL',
        use_mesh_modifiers=False, mesh_smooth_type='FACE', path_mode='COPY', embed_textures=True,
        bake_anim=animation, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
        bake_anim_use_all_bones=True, bake_anim_force_startend_keying=True,
        bake_anim_step=1.0, bake_anim_simplify_factor=0.0)
    if not path.is_file() or path.stat().st_size < 1000: raise RuntimeError('FBX export failed: '+str(path))
    return {'path':str(path.relative_to(ROOT)), 'bytes':path.stat().st_size,'sha256':sha(path)}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--source-root',type=Path,default=Path(r'D:\Hadi\proyek\manajer-sepak-bola'))
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    inventory=json.loads((ROOT/'00_Specs_and_Logs/blend_asset_details.json').read_text(encoding='utf-8-sig'))
    sources=[args.source_root/'build/nico-paz/blender-session-autosave.blend', args.source_root/'art/animation/football_20261015/football_actions.blend']
    known={f['path'] for f in inventory['files']}
    for path in sources:
        if path.relative_to(args.source_root).as_posix() not in known: raise RuntimeError('Source absent from audited inventory')
    before={str(p):sha(p) for p in sources}
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
    with bpy.data.libraries.load(str(sources[0]),link=False) as (src,dst):
        dst.objects=['Armature_NicoPaz','NicoPaz_Como1907']
        dst.actions=src.actions
    for obj in dst.objects:
        if obj: bpy.context.scene.collection.objects.link(obj)
    rig=next(o for o in dst.objects if o and o.type=='ARMATURE')
    mesh=next(o for o in dst.objects if o and o.type=='MESH')
    actions={a.name:a for a in dst.actions if a}
    with bpy.data.libraries.load(str(sources[1]),link=False) as (src,dst): dst.actions=src.actions
    actions.update({a.name:a for a in dst.actions if a})
    missing=set(sum(GROUPS.values(),[]))-set(actions)
    if missing: raise RuntimeError('Missing source actions: '+str(missing))
    rig.animation_data_clear()
    for bone in rig.pose.bones: bone.matrix_basis=Matrix.Identity(4)
    # Audit source uses Y-up bone coordinates. Rotate the object as a whole so
    # mesh, bind pose and all local bone animation channels share the same basis.
    head=rig.data.bones['mixamorig:Head'].head_local
    hips=rig.data.bones['mixamorig:Hips'].head_local
    correction=Matrix.Rotation(math.pi/2,4,'X') if abs((head-hips).y)>abs((head-hips).z) else Matrix.Identity(4)
    rig.matrix_world=correction @ rig.matrix_world
    rig.name='Armature_NicoPaz'
    bpy.context.scene.unit_settings.system='METRIC'
    bpy.context.scene.unit_settings.scale_length=1.0
    bpy.context.scene.render.fps=30
    bpy.context.scene.frame_set(0)
    rig.data.pose_position='REST'
    bpy.context.view_layer.update()
    bones=[{'name':b.name,'parent':b.parent.name if b.parent else None} for b in rig.data.bones]
    materials=[m.name if m else None for m in mesh.data.materials]
    out=ROOT/'04_Exported_GameAssets'
    report={'status':'RUNNING','blender':bpy.app.version_string,'source_sha256':before,'bones':bones,'materials':materials,'character_lods':[],'animations':[]}
    for lod,ratio in enumerate((1.0,0.5,0.2)):
        copy=mesh.copy(); copy.data=mesh.data.copy(); copy.name=f'NicoPaz_LOD{lod}'
        bpy.context.scene.collection.objects.link(copy)
        select([copy,rig])
        bpy.context.view_layer.objects.active=copy
        if ratio<1:
            dec=copy.modifiers.new('LOD_Reduction','DECIMATE'); dec.ratio=ratio; dec.use_collapse_triangulate=True
            bpy.ops.object.modifier_move_up(modifier=dec.name)
            bpy.ops.object.modifier_apply(modifier=dec.name)
        copy.data.calc_loop_triangles()
        entry=export(out/f'characters_lod/NicoPaz_LOD{lod}.fbx',[rig,copy])
        entry.update(lod=lod,ratio=ratio,triangles=len(copy.data.loop_triangles),vertices=len(copy.data.vertices),vertex_groups=len(copy.vertex_groups))
        report['character_lods'].append(entry)
        bpy.data.objects.remove(copy,do_unlink=True)
        print('EXPORTED_LOD',lod,entry['triangles'],flush=True)
    rig.data.pose_position='POSE'
    for group,names in GROUPS.items():
        for name in names:
            action=actions[name]
            rig.animation_data_create(); rig.animation_data.action=action
            if action.slots: rig.animation_data.action_slot=action.slots[0]
            bpy.context.scene.frame_start=math.floor(action.frame_range[0])
            bpy.context.scene.frame_end=math.ceil(action.frame_range[1])
            bpy.context.scene.frame_set(bpy.context.scene.frame_start)
            # Select only the armature: every FBX carries exactly one action.
            entry=export(out/group/(name+'.fbx'),[rig],True)
            entry.update(name=name,group=group,frame_start=bpy.context.scene.frame_start,frame_end=bpy.context.scene.frame_end,fps=30)
            report['animations'].append(entry)
            print('EXPORTED_ANIMATION',name,flush=True)
    after={str(p):sha(p) for p in sources}
    if before!=after: raise RuntimeError('Source file integrity changed')
    report.update(status='SUCCESS',source_unchanged=True)
    (out/'asset_manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('EXPORT_COMPLETE',len(report['character_lods']),len(report['animations']),flush=True)

if __name__=='__main__': main()
