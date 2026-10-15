"""Build Footballer01's skeleton, skin and original procedural animation library.

Only the new ComfyUI GLB is loaded. No legacy Blender, FBX, rig or action is used.
Procedural clips are a reproducible baseline, not motion-capture recordings.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy
import bmesh
from mathutils import Vector, Matrix, Quaternion

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'03_Blender_AutoRig_Mocap/fresh_character'
OUT=ROOT/'04_Exported_GameAssets/Footballer01'
GROUPS={
 'animations_locomotion':'idle jog sprint turn_left turn_spin turn_sprint turn_walk defend_stance defend_shuffle'.split(),
 'animations_onball':'dribble pass shot header receive shield skill_juggle tackle_stand tackle_lunge tackle_slide react_miss'.split(),
 'animations_goalkeeper':'gk_ready gk_dive_left gk_dive_right gk_high gk_smother gk_pass'.split(),
 'animations_setpieces':['penalty']}
TARGET_POSES={}
POSE_YAW=0.0

def select(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects: obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]

def file_record(path):
    return {'path':path.relative_to(ROOT).as_posix(),'bytes':path.stat().st_size,
            'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}

def capsule(a,b,radius,name,parts):
    a,b=Vector(a),Vector(b); mid=(a+b)/2
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12,ring_count=8,location=mid)
    o=bpy.context.object;o.name=name;o.scale=(radius,radius,(b-a).length/2+radius)
    o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler();parts.append(o)

def repair_hands(mesh):
    """Replace webbed AI hands with five separated modeled fingers per hand."""
    hands=[];finger_specs=[]
    skin=bpy.data.materials.new('Fresh_HandSkin');skin.diffuse_color=(.48,.285,.18,1);skin.use_nodes=True
    bs=skin.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=skin.diffuse_color;bs.inputs['Roughness'].default_value=.6
    bm=bmesh.new();bm.from_mesh(mesh.data)
    # GLB duplicates vertices along UV seams. Weld geometry first, retaining
    # per-loop UVs, so seam edges are not mistaken for holes in the surface.
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.00001)
    remove=[]
    for v in bm.verts:
        s=1 if v.co.x>=0 else -1
        w=Vector((s*.585,-.005,.98));d=Vector((s*.69,0,-.724)).normalized()
        if abs(v.co.x)>.54 and v.co.z<1.08 and (v.co-w).dot(d)>0: remove.append(v)
    bmesh.ops.delete(bm,geom=remove,context='VERTS')
    # Cap the cut cuffs before binding; no dark open wrist cavities in close-ups.
    boundary=[e for e in bm.edges if e.is_boundary]
    if boundary:bmesh.ops.holes_fill(bm,edges=boundary,sides=0)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh.data);bm.free()
    for side,s in [('L',1),('R',-1)]:
        w=Vector((s*.585,-.005,.98));d=Vector((s*.69,0,-.724)).normalized();across=Vector((s*.724,0,.69))
        parts=[]
        # Palm and wrist overlap the cut boundary to avoid an open silhouette.
        capsule(w-d*.040,w+d*.018,.035,'Wrist',parts)
        bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=10,location=w+d*.035)
        palm=bpy.context.object;palm.scale=(.036,.018,.058);palm.rotation_euler=d.to_track_quat('Z','Y').to_euler();parts.append(palm)
        for i,(label,length) in enumerate([('Index',.069),('Middle',.078),('Ring',.072),('Pinky',.057)]):
            offset=(1.5-i)*.019;start=w+d*.074+across*offset
            direction=(d+across*((1.5-i)*.065)+Vector((0,-.07,0))).normalized()
            joint=start+direction*(length*.52);end=start+direction*length+Vector((0,-.004,0))
            capsule(start,joint,.0095 if i<3 else .008,label,parts);capsule(joint,end,.0085 if i<3 else .0075,label,parts)
            finger_specs.extend([(f'{label}_01_{side}',start,joint,f'Hand_{side}'),(f'{label}_02_{side}',joint,end,f'{label}_01_{side}')])
        start=w+d*.025+across*.027;joint=start+d*.020+across*.025;end=joint+d*.025+across*.018
        capsule(start,joint,.013,'Thumb',parts);capsule(joint,end,.011,'Thumb',parts)
        finger_specs.extend([(f'Thumb_01_{side}',start,joint,f'Hand_{side}'),(f'Thumb_02_{side}',joint,end,f'Thumb_01_{side}')])
        select(parts);bpy.ops.object.join();hand=bpy.context.object;hand.name='FreshHand_'+side
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        remesh=hand.modifiers.new('JoinHandSurface','REMESH');remesh.mode='VOXEL';remesh.voxel_size=.003
        bpy.ops.object.modifier_apply(modifier=remesh.name)
        smooth=hand.modifiers.new('SmoothHand','SMOOTH');smooth.factor=.55;smooth.iterations=3;bpy.ops.object.modifier_apply(modifier=smooth.name)
        hand.data.materials.clear();hand.data.materials.append(skin)
        for face in hand.data.polygons:face.use_smooth=True
        hands.append(hand)
    return hands,finger_specs

def skeleton(finger_specs):
    data=bpy.data.armatures.new('Footballer01_Rig');rig=bpy.data.objects.new('Footballer01_Rig',data);bpy.context.collection.objects.link(rig)
    select([rig]);bpy.ops.object.mode_set(mode='EDIT')
    specs=[('Root',(0,0,0),(0,0,.12),None),('Pelvis',(0,.025,.86),(0,.025,1.04),'Root'),
      ('Spine_01',(0,.025,1.04),(0,.025,1.24),'Pelvis'),('Spine_02',(0,.025,1.24),(0,.025,1.43),'Spine_01'),
      ('Neck',(0,.025,1.43),(0,.0,1.57),'Spine_02'),('Head',(0,0,1.57),(0,-.015,1.79),'Neck')]
    for side,s in [('L',1),('R',-1)]:
        specs.extend([(f'Clavicle_{side}',(0,.025,1.43),(s*.22,.025,1.43),'Spine_02'),
         (f'UpperArm_{side}',(s*.22,.025,1.43),(s*.43,.015,1.19),f'Clavicle_{side}'),
         (f'Forearm_{side}',(s*.43,.015,1.19),(s*.585,-.005,.98),f'UpperArm_{side}'),
         (f'Hand_{side}',(s*.585,-.005,.98),(s*.637,-.005,.925),f'Forearm_{side}'),
         (f'Thigh_{side}',(s*.115,.025,.86),(s*.145,-.005,.49),'Pelvis'),
         (f'Calf_{side}',(s*.145,-.005,.49),(s*.16,.065,.115),f'Thigh_{side}'),
         (f'Foot_{side}',(s*.16,.065,.115),(s*.16,-.115,.055),f'Calf_{side}'),
         (f'Toe_{side}',(s*.16,-.115,.055),(s*.16,-.20,.05),f'Foot_{side}')])
    specs+=finger_specs
    for name,a,b,parent in specs:
        bone=data.edit_bones.new(name);bone.head=a;bone.tail=b;bone.use_deform=name!='Root'
        if parent: bone.parent=data.edit_bones[parent]
        bone.align_roll(Vector((0,1,0)))
    bpy.ops.object.mode_set(mode='OBJECT');rig.show_in_front=True
    return rig

def skin_mesh(mesh,rig):
    """Heat weighting with explicit normalization and per-vertex coverage audit."""
    select([mesh]);bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.remove_doubles(threshold=.00001);bpy.ops.object.mode_set(mode='OBJECT')
    select([mesh,rig]);bpy.context.view_layer.objects.active=rig
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    deform={b.name for b in rig.data.bones if b.use_deform}
    for v in mesh.data.vertices:
        groups=sorted([(g.group,g.weight) for g in v.groups if mesh.vertex_groups[g.group].name in deform and g.weight>1e-6],key=lambda a:-a[1])[:4]
        # Use the same wrist transition on the AI forearm and modeled hand.
        side='L' if v.co.x>0 else 'R';s=1 if side=='L' else -1
        wrist=Vector((s*.585,-.005,.98));direction=Vector((s*.69,0,-.724)).normalized()
        if (v.co-wrist).length<.060:
            weight=max(0,min(1,.45+(v.co-wrist).dot(direction)/.09))
            groups=[(mesh.vertex_groups[f'Forearm_{side}'].index,1-weight),(mesh.vertex_groups[f'Hand_{side}'].index,weight)]
        if not groups:
            # Fail instead of silently assigning vertices to an unrelated bone.
            raise RuntimeError(f'Unweighted vertex {v.index} in {mesh.name}; heat bind must be repaired')
        total=sum(w for _,w in groups)
        for g in list(v.groups):mesh.vertex_groups[g.group].remove([v.index])
        for index,w in groups:mesh.vertex_groups[index].add([v.index],w/total,'REPLACE')
    for mod in mesh.modifiers:
        if mod.type=='ARMATURE':mod.use_deform_preserve_volume=False
    for face in mesh.data.polygons:face.use_smooth=True

def solve_chain(a,target,l1,l2,pole):
    a=Vector(a);delta=Vector(target)-a;distance=max(.001,min(delta.length,l1+l2-.001));direction=delta.normalized()
    end=a+direction*distance
    along=(l1*l1-l2*l2+distance*distance)/(2*distance)
    perp=Vector(pole)-direction*Vector(pole).dot(direction)
    if perp.length<.001:perp=Vector((1,0,0))
    joint=a+direction*along+perp.normalized()*math.sqrt(max(0,l1*l1-along*along))
    return joint,end

def set_bone(rig,name,head,tail):
    rest=rig.data.bones[name];delta=(rest.tail_local-rest.head_local).rotation_difference(Vector(tail)-Vector(head))
    rotation=delta.to_matrix() @ rest.matrix_local.to_3x3()
    desired=Matrix.Rotation(POSE_YAW,4,'Z') @ Matrix.Translation(Vector(head)) @ rotation.to_4x4()
    # PoseBone.matrix uses evaluated parent transforms, stale during a bulk edit.
    # Explicit parent matrices make every local key independent of prior frames.
    kwargs={}
    if rest.parent:
        kwargs={'parent_matrix':TARGET_POSES.get(rest.parent.name,rest.parent.matrix_local),
                'parent_matrix_local':rest.parent.matrix_local}
    rig.pose.bones[name].matrix_basis=rest.convert_local_to_pose(desired,rest.matrix_local,invert=True,**kwargs)
    TARGET_POSES[name]=desired

def pose(rig,name,t,duration):
    global POSE_YAW
    TARGET_POSES.clear()
    phase=t/duration;wave=2*math.pi*phase
    POSE_YAW=2*math.pi*phase if name=='turn_spin' else (.55 if name=='turn_left' else .25)*math.sin(math.pi*phase) if name in ('turn_left','turn_sprint','turn_walk') else 0.0
    for b in rig.pose.bones:b.matrix_basis=Matrix.Identity(4)
    moving=name in ('jog','sprint','dribble','turn_left','turn_spin','turn_sprint','turn_walk','defend_shuffle')
    running=name in ('sprint','turn_sprint')
    defensive=name in ('defend_stance','defend_shuffle','gk_ready','shield','tackle_stand')
    kick=name in ('pass','shot','gk_pass','penalty','skill_juggle','receive','tackle_lunge')
    slide=name=='tackle_slide';dive=name in ('gk_dive_left','gk_dive_right');high=name in ('gk_high','header')
    smother=name=='gk_smother'
    pulse=math.sin(math.pi*phase)**2
    kick_pulse=math.sin(math.pi*max(0,min(1,(phase-.12)/.7)))
    z=.86+(.006*math.sin(wave) if not moving else -.055+.013*math.cos(2*wave))
    pitch=math.radians(10 if moving else 0);roll=0.0;y=0.025
    if defensive:z-=.10;pitch=.17
    if high:z+=.20*pulse;pitch=-.12*pulse
    if kick:z-=.025*pulse;pitch=.13*pulse
    if slide:z-=.48*pulse;pitch=-.55*pulse;y+=.08*pulse
    if smother:z-=.40*pulse;pitch=.5*pulse
    if dive:z-=.39*pulse;roll=(-1 if name.endswith('left') else 1)*1.20*pulse
    orient=Quaternion((1,0,0),pitch) @ Quaternion((0,1,0),roll)
    pelvis=Vector((0,y,z))
    def torso_point(v):return pelvis+orient @ Vector(v)
    points=[pelvis,torso_point((0,0,.18)),torso_point((0,0,.38)),torso_point((0,0,.57)),torso_point((0,-.025,.71)),torso_point((0,-.04,.93))]
    for i,b in enumerate(['Pelvis','Spine_01','Spine_02','Neck','Head']):set_bone(rig,b,points[i],points[i+1])
    for side,s,offset in [('L',1,0),('R',-1,.5)]:
        p=(phase+offset)%1;stride=.56 if running else .40;lift=.20 if running else .115
        foot=Vector((s*.16,.065,.115))
        if moving:
            if p<.60:foot.y=-stride/2+stride*p/.60
            else:
                q=(p-.60)/.40;foot.y=stride/2*math.cos(math.pi*q);foot.z+=lift*math.sin(math.pi*q)
            if name=='defend_shuffle':foot.x+=.055*math.sin(wave);foot.y*=.3
            if name=='dribble':foot.y*=.75
        if defensive:foot.x=s*.23
        if kick and side=='R':
            foot.y=.10+.22*math.sin(2*math.pi*phase)-.30*kick_pulse
            foot.z+=.22*max(0,kick_pulse)
            if name=='receive':foot.z-=.10*max(0,kick_pulse)
        if slide:foot.y-=.52*pulse;foot.z+=.03*pulse
        if smother:foot.x=s*.22
        if dive:foot=(Vector((s*.16,.065,.115))*(1-pulse)+(pelvis+orient @ Vector((s*.16,.02,-.65)))*pulse)
        hip=torso_point((s*.115,0,0))
        thigh=rig.data.bones[f'Thigh_{side}'];calf=rig.data.bones[f'Calf_{side}']
        knee,ankle=solve_chain(hip,foot,thigh.length,calf.length,orient @ Vector((0,-1,0)))
        set_bone(rig,f'Thigh_{side}',hip,knee);set_bone(rig,f'Calf_{side}',knee,ankle)
        foot_direction=orient @ Vector((0,-.18,-.060)) if dive else Vector((0,-.18,-.060))
        toe=ankle+foot_direction;set_bone(rig,f'Foot_{side}',ankle,toe);set_bone(rig,f'Toe_{side}',toe,toe+Vector((0,-.085,-.005)))
        shoulder=torso_point((s*.22,0,.57));clavicle=points[3];set_bone(rig,f'Clavicle_{side}',clavicle,shoulder)
        hand=torso_point((s*.29,-.025,.08))
        if moving:hand=torso_point((s*.28,-.20+.14*math.sin(wave+offset*2*math.pi),.32+.06*math.sin(wave+offset*2*math.pi)))
        if defensive:hand=torso_point((s*.34,-.18,.22))
        if kick:hand=torso_point((s*(.29+.08*pulse),-.02-.06*pulse,.08+.1*pulse))
        if high:hand=torso_point((s*.28,-.06,.10+(.85 if name=='gk_high' else .28)*pulse))
        if smother:hand=torso_point((s*.18,-.35,.10))
        if slide:hand=torso_point((s*.36,.05,.12))
        if dive:hand=torso_point((s*.18,-.10,.1+.9*pulse))
        if name=='react_miss':hand=torso_point((s*.22,-.12,.08+.6*pulse))
        upper=rig.data.bones[f'UpperArm_{side}'];lower=rig.data.bones[f'Forearm_{side}']
        elbow,wrist=solve_chain(shoulder,hand,upper.length,lower.length,orient @ Vector((s*.3,1,-.3)))
        set_bone(rig,f'UpperArm_{side}',shoulder,elbow);set_bone(rig,f'Forearm_{side}',elbow,wrist)
        direction=(wrist-elbow).normalized()
        set_bone(rig,f'Hand_{side}',wrist,wrist+direction*rig.data.bones[f'Hand_{side}'].length)
        for finger in ('Index','Middle','Ring','Pinky'):
            for segment in ('01','02'):
                bone=rig.pose.bones[f'{finger}_{segment}_{side}'];bone.rotation_mode='QUATERNION'
                bone.rotation_quaternion=Quaternion((1,0,0),(.45 if segment=='01' else .65) if moving else .12)
    bpy.context.view_layer.update()

def export(path,objects,animated=False):
    path.parent.mkdir(parents=True,exist_ok=True);select(objects)
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'ARMATURE','MESH'},
      global_scale=1.0,apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',
      add_leaf_bones=False,use_armature_deform_only=False,armature_nodetype='NULL',use_mesh_modifiers=False,
      mesh_smooth_type='FACE',path_mode='COPY',embed_textures=True,bake_anim=animated,
      bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_use_all_bones=True,
      bake_anim_force_startend_keying=True,bake_anim_step=1,bake_anim_simplify_factor=0)
    return file_record(path)

def main():
    p=argparse.ArgumentParser();p.add_argument('--mesh',type=Path,required=True);args=p.parse_args(sys.argv[sys.argv.index('--')+1:])
    if not args.mesh.resolve().is_relative_to(ROOT):raise ValueError('New character input must be inside FootballSim_Master')
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.gltf(filepath=str(args.mesh))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH'];select(meshes);bpy.ops.object.join();mesh=bpy.context.object;mesh.name='Footballer01_Body'
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    lo=Vector(tuple(min(v.co[i] for v in mesh.data.vertices) for i in range(3)));hi=Vector(tuple(max(v.co[i] for v in mesh.data.vertices) for i in range(3)))
    center=Vector(((hi.x+lo.x)/2,(hi.y+lo.y)/2,lo.z));scale=1.8/(hi.z-lo.z)
    for v in mesh.data.vertices:v.co=(v.co-center)*scale
    # Persist the newly generated PBR maps; no previous character texture is used.
    texture_dir=OUT/'textures';texture_dir.mkdir(parents=True,exist_ok=True)
    textures=[]
    for mat in mesh.data.materials:
        if not mat or not mat.use_nodes:continue
        for n in mat.node_tree.nodes:
            if n.type!='TEX_IMAGE' or not n.image:continue
            role='basecolor' if n.image.colorspace_settings.name=='sRGB' else 'metallic_roughness'
            path=texture_dir/(role+'.png');n.image.filepath_raw=str(path);n.image.file_format='PNG';n.image.save()
            textures.append({'role':role,**file_record(path)})
    # Clothing-only team tint: vertex colour keeps skin, hair and boots unchanged.
    colors=mesh.data.color_attributes.new(name='KitMask',type='FLOAT_COLOR',domain='CORNER')
    for loop in mesh.data.loops:
        v=mesh.data.vertices[loop.vertex_index].co;x=abs(v.x);z=v.z
        jersey=(.81<z<1.48 and x<.24) or (1.27<z<1.48 and .22<x<.38)
        shorts=.56<z<.84 and x<.24
        socks=.13<z<.41 and x<.24
        mask=1.0 if jersey or shorts or socks else 0.0
        colors.data[loop.index].color=(mask,mask,mask,1)
    hands,finger_specs=repair_hands(mesh);rig=skeleton(finger_specs)
    for part in [mesh]+hands:skin_mesh(part,rig)
    select([mesh]+hands);bpy.ops.object.join();mesh=bpy.context.object;mesh.name='Footballer01_LOD0'
    # Joining skinned parts retains groups; retain exactly one armature modifier.
    modifiers=[m for m in mesh.modifiers if m.type=='ARMATURE']
    for mod in modifiers[1:]:mesh.modifiers.remove(mod)
    scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1;scene.render.fps=30
    OUT.mkdir(parents=True,exist_ok=True);WORK.mkdir(parents=True,exist_ok=True)
    report={'status':'BUILDING','source':file_record(args.mesh),'legacy_inputs':[],
      'rig':'Footballer01_Rig','height_m':1.8,'bones':[b.name for b in rig.data.bones],
      'skinning':'Blender bone heat; normalized top 4 influences','animation_origin':'new procedural keyframes with analytic two-bone IK',
      'hand_repair':'Both AI webbed hands replaced by five-finger geometry','textures':textures,'character_lods':[],'animations':[]}
    rig.data.pose_position='REST';rig.animation_data_clear()
    for lod,ratio in enumerate((1,.55,.28)):
        copy=mesh.copy();copy.data=mesh.data.copy();bpy.context.collection.objects.link(copy);copy.name=f'Footballer01_LOD{lod}'
        if lod:
            select([copy]);dec=copy.modifiers.new('LOD','DECIMATE');dec.ratio=ratio;dec.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=dec.name)
        copy.data.calc_loop_triangles();entry=export(OUT/f'characters_lod/Footballer01_LOD{lod}.fbx',[rig,copy]);entry.update(lod=lod,triangles=len(copy.data.loop_triangles),vertices=len(copy.data.vertices));report['character_lods'].append(entry)
        bpy.data.objects.remove(copy,do_unlink=True)
    rig.data.pose_position='POSE';rig.animation_data_create()
    for group,names in GROUPS.items():
        for name in names:
            duration=.70 if name in ('jog','dribble','turn_left','turn_walk','defend_shuffle') else .52 if name in ('sprint','turn_sprint') else 2.4 if name in ('idle','defend_stance','gk_ready') else 1.4
            frames=round(duration*30);duration=frames/30
            action=bpy.data.actions.new(name);rig.animation_data.action=action
            for frame in range(frames+1):
                scene.frame_set(frame+1);pose(rig,name,frame/30,duration)
                for bone in rig.pose.bones:
                    bone.rotation_mode='QUATERNION'
                    bone.keyframe_insert('location',frame=frame+1,group=bone.name)
                    bone.keyframe_insert('rotation_quaternion',frame=frame+1,group=bone.name)
                    bone.keyframe_insert('scale',frame=frame+1,group=bone.name)
            action.use_fake_user=True;scene.frame_start=1;scene.frame_end=frames+1
            entry=export(OUT/group/(name+'.fbx'),[rig],True);entry.update(name=name,group=group,seconds=duration,fps=30);report['animations'].append(entry)
            print('FRESH_ANIMATION',name,flush=True)
    rig.animation_data.action=bpy.data.actions['idle'];scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(WORK/'Footballer01_rigged.blend'))
    report['status']='EXPORTED_PENDING_VISUAL_QA'
    (OUT/'asset_manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('FRESH_CHARACTER_EXPORTED',len(report['animations']),flush=True)

if __name__=='__main__':main()
