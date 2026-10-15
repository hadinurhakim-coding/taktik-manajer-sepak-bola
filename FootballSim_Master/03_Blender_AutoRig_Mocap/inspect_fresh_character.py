"""Inspect and render a newly generated mesh in headless Blender."""
import argparse
import json
import math
from pathlib import Path
import sys
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]

def aim(obj, point): obj.rotation_euler=(Vector(point)-obj.location).to_track_quat('-Z','Y').to_euler()

def setup_render(directory):
    scene=bpy.context.scene
    scene.render.engine='CYCLES'; scene.cycles.device='CPU'; scene.cycles.samples=24
    scene.render.resolution_x=900; scene.render.resolution_y=1000; scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG'; scene.world.color=(0.18,0.18,0.18)
    scene.view_settings.view_transform='AgX'
    for name,loc,power,size in [('Key',(3,-4,5),450,4),('Fill',(-3,-2,3),250,4),('Rim',(2,3,4),550,3)]:
        data=bpy.data.lights.new(name,'AREA'); data.energy=power; data.shape='DISK'; data.size=size
        o=bpy.data.objects.new(name,data); scene.collection.objects.link(o); o.location=loc; aim(o,(0,0,1))
    bpy.ops.object.camera_add(location=(0,-4,0.95)); camera=bpy.context.object
    camera.data.type='ORTHO'; camera.data.ortho_scale=2.12; aim(camera,(0,0,0.95)); scene.camera=camera
    directory.mkdir(parents=True,exist_ok=True)
    return scene,camera

def main():
    p=argparse.ArgumentParser();p.add_argument('--mesh',type=Path,required=True);args=p.parse_args(sys.argv[sys.argv.index('--')+1:])
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.gltf(filepath=str(args.mesh))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for obj in meshes: obj.select_set(True)
    bpy.context.view_layer.objects.active=meshes[0];bpy.ops.object.join();mesh=bpy.context.object
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    lo=Vector(tuple(min(v.co[i] for v in mesh.data.vertices) for i in range(3)))
    hi=Vector(tuple(max(v.co[i] for v in mesh.data.vertices) for i in range(3)))
    scale=1.8/(hi.z-lo.z);center=Vector(((hi.x+lo.x)/2,(hi.y+lo.y)/2,lo.z))
    for v in mesh.data.vertices: v.co=(v.co-center)*scale
    out=ROOT/'03_Blender_AutoRig_Mocap/fresh_character'
    scene,camera=setup_render(out)
    report={'input':str(args.mesh),'vertices':len(mesh.data.vertices),'faces':len(mesh.data.polygons),'height_m':1.8,'sections':[]}
    for z in [0.10,0.30,0.52,0.72,0.88,1.00,1.20,1.42,1.52,1.65]:
        verts=[v.co for v in mesh.data.vertices if abs(v.co.z-z)<0.015]
        if verts: report['sections'].append({'z':z,'min':[min(v[i] for v in verts) for i in range(3)],'max':[max(v[i] for v in verts) for i in range(3)]})
    (out/'mesh_audit.json').write_text(json.dumps(report,indent=2))
    bpy.ops.wm.save_as_mainfile(filepath=str(out/'footballer01_mesh.blend'))
    for name,loc in [('front',(0,-4,.95)),('side',(4,0,.95)),('back',(0,4,.95))]:
        camera.location=loc;aim(camera,(0,0,.95));scene.render.filepath=str(out/(name+'.png'));bpy.ops.render.render(write_still=True)
    print('FRESH_MESH_INSPECTED',json.dumps(report),flush=True)

if __name__=='__main__': main()
