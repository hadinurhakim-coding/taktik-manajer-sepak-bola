"""Reproducible local ComfyUI jobs for the new Footballer01 character.

No previous character, rig, animation or texture is an input. No UI automation.
"""
import argparse
import json
from pathlib import Path
import time
import uuid
import requests

ROOT = Path(__file__).resolve().parents[2]
FACTORY = ROOT / '02_ComfyUI_AssetFactory'
OUT = FACTORY / 'generated/footballer01'
LOG = ROOT / '00_Specs_and_Logs'
URL = 'http://127.0.0.1:8190'


def upload(path):
    with path.open('rb') as f:
        r = requests.post(URL+'/upload/image', files={'image':(path.name,f)},
                          data={'type':'input','overwrite':'false'}, timeout=30)
    r.raise_for_status()
    data = r.json()
    return '/'.join(filter(None,[data.get('subfolder'),data['name']]))


def node(kind, **inputs):
    return {'class_type':kind, 'inputs':inputs}


def run(workflow, stage):
    (FACTORY/'workflows_api').mkdir(parents=True,exist_ok=True)
    OUT.mkdir(parents=True,exist_ok=True)
    (FACTORY/f'workflows_api/footballer01_{stage}.json').write_text(json.dumps(workflow,indent=2),encoding='utf-8')
    queue=requests.get(URL+'/queue',timeout=10).json()
    if queue['queue_running'] or queue['queue_pending']:
        raise RuntimeError('ComfyUI queue is busy; retry after current jobs finish.')
    r=requests.post(URL+'/prompt',json={'prompt':workflow,'client_id':'footballer01-'+uuid.uuid4().hex},timeout=30)
    if not r.ok: raise RuntimeError(r.text)
    job=r.json()['prompt_id']
    (LOG/f'footballer01_{stage}_job.json').write_text(json.dumps({'prompt_id':job,'stage':stage},indent=2))
    print('SUBMITTED',stage,job,flush=True)
    deadline=time.monotonic()+3600
    while time.monotonic()<deadline:
        history=requests.get(URL+'/history/'+job,timeout=30).json().get(job)
        if history:
            (LOG/f'footballer01_{stage}_history.json').write_text(json.dumps(history,indent=2),encoding='utf-8')
            if history['status']['status_str']!='success': raise RuntimeError(json.dumps(history['status']))
            for output in history['outputs'].values():
                for item in output.get('images',[]):
                    dest=OUT/(stage+'_'+item['filename'])
                    data=requests.get(URL+'/view',params=item,timeout=120)
                    data.raise_for_status(); dest.write_bytes(data.content)
                    print('IMAGE',dest,flush=True)
                if output.get('mesh'): print('MESH',output['mesh'],flush=True)
            print('COMPLETE',stage,flush=True)
            return history
        time.sleep(3)
    raise TimeoutError('ComfyUI job exceeded one hour; inspect saved prompt id before resubmitting.')


def reference(seed):
    # Standard OpenPose palette/order, A-pose with arms clear of the torso.
    from PIL import Image, ImageDraw
    im=Image.new('RGB',(1024,1024)); draw=ImageDraw.Draw(im)
    pts=[(512,145),(512,235),(415,250),(330,355),(240,460),(609,250),(694,355),(784,460),
         (460,545),(446,720),(430,920),(564,545),(578,720),(594,920),
         (495,132),(529,132),(480,145),(544,145)]
    links=[(1,2),(1,5),(2,3),(3,4),(5,6),(6,7),(1,8),(8,9),(9,10),(1,11),(11,12),(12,13),(1,0),(0,14),(14,16),(0,15),(15,17)]
    colors=[(255,0,0),(255,85,0),(255,170,0),(255,255,0),(170,255,0),(85,255,0),(0,255,0),(0,255,85),(0,255,170),(0,255,255),(0,170,255),(0,85,255),(0,0,255),(85,0,255),(170,0,255),(255,0,255),(255,0,170),(255,0,85)]
    for i,(a,b) in enumerate(links): draw.line((pts[a],pts[b]),fill=colors[i],width=7)
    for i,(x,y) in enumerate(pts): draw.ellipse((x-7,y-7,x+7,y+7),fill=colors[i])
    OUT.mkdir(parents=True,exist_ok=True); pose=OUT/'new_apose_control.png'; im.save(pose)
    prompt=('Full body front orthographic view of one fictional adult male professional football player, '
            'realistic game character concept, athletic lean natural proportions, 180 cm tall, short dark hair, '
            'neutral friendly face, symmetrical relaxed A pose, both straight arms spread diagonally down away from torso, '
            'hands open palms forward, both legs straight separated at shoulder width, plain royal blue short sleeve soccer jersey, '
            'plain white football shorts ending mid thigh, blue knee socks, black football boots, '
            'entire body head to both feet inside image with margin, even flat studio lighting, clean white background, '
            'high quality anatomy, sharp details, no logos, no letters, no ball, single character')
    negative=('cropped, close up, portrait, extra person, multiple views, text, logo, watermark, ball, stadium, '
              'running, bent arms, crossed limbs, extra arms, extra legs, missing hands, deformed hands, '
              'long sleeves, pants, cloak, muscular bodybuilder, cartoon, illustration, wide angle')
    workflow={
      '1':node('CheckpointLoaderSimple',ckpt_name='sd_xl_base_1.0.safetensors'),
      '2':node('CLIPTextEncode',clip=['1',1],text=prompt),
      '3':node('CLIPTextEncode',clip=['1',1],text=negative),
      '4':node('EmptyLatentImage',width=1024,height=1024,batch_size=1),
      '5':node('LoadImage',image=upload(pose)),
      '6':node('ControlNetLoader',control_net_name='controlnet-openpose-sdxl-1.0.safetensors'),
      '7':node('ControlNetApplyAdvanced',positive=['2',0],negative=['3',0],control_net=['6',0],image=['5',0],strength=0.85,start_percent=0.0,end_percent=0.9),
      '8':node('KSampler',model=['1',0],positive=['7',0],negative=['7',1],latent_image=['4',0],seed=seed,steps=30,cfg=6.5,sampler_name='dpmpp_2m',scheduler='karras',denoise=1.0),
      '9':node('VAEDecode',samples=['8',0],vae=['1',2]),
      '10':node('SaveImage',images=['9',0],filename_prefix='FootballSim_Fresh/footballer01_reference')}
    run(workflow,'reference')


def shape(path, seed):
    workflow={
      '1':node('LoadImage',image=upload(path)),
      '2':node('LoadBackgroundRemovalModel',bg_removal_name='birefnet.safetensors'),
      '3':node('RemoveBackground',bg_removal_model=['2',0],image=['1',0]),
      '4':node('Hy3DModelLoader',model='hy3dgen\\hunyuan3d-dit-v2-0-fp16.safetensors',attention_mode='sdpa',cublas_ops=False),
      '5':node('Hy3DGenerateMesh',pipeline=['4',0],image=['1',0],mask=['3',0],guidance_scale=5.5,steps=35,seed=seed,force_offload=True),
      '6':node('Hy3DVAEDecode',vae=['4',1],latents=['5',0],box_v=1.01,octree_resolution=384,num_chunks=4000,mc_level=0.0,mc_algo='mc',enable_flash_vdm=True,force_offload=True),
      '7':node('Hy3DPostprocessMesh',trimesh=['6',0],remove_floaters=True,remove_degenerate_faces=True,reduce_faces=True,max_facenum=45000,smooth_normals=True),
      '8':node('Hy3DMeshUVWrap',trimesh=['7',0]),
      '9':node('Hy3DExportMesh',trimesh=['8',0],filename_prefix='FootballSim_Fresh/footballer01_shape',file_format='glb',save_file=True)}
    run(workflow,'shape')


def paint(path, seed):
    # The installed wrapper attaches .uv to ColorVisuals, which GLB export drops.
    # Convert to TextureVisuals explicitly and verify the persisted UV attribute.
    import numpy as np
    import trimesh
    import xatlas
    from PIL import Image
    mesh=trimesh.load(OUT/'footballer01_shape.glb',force='mesh')
    vmapping,faces,uv=xatlas.parametrize(mesh.vertices,mesh.faces)
    material=trimesh.visual.material.PBRMaterial(baseColorTexture=Image.new('RGB',(2,2),(220,220,220)),metallicFactor=0.0,roughnessFactor=0.8)
    repaired=trimesh.Trimesh(vertices=mesh.vertices[vmapping],faces=faces,process=False,
                           visual=trimesh.visual.TextureVisuals(uv=uv,material=material))
    uv_path=FACTORY/'Saved/ComfyRuntime/input/footballer01_uv.glb'
    repaired.export(uv_path)
    loaded=trimesh.load(uv_path,force='mesh',process=False)
    assert loaded.visual.uv is not None and np.isfinite(loaded.visual.uv).all()
    workflow={
      '1':node('LoadImage',image=upload(path)),
      '2':node('Load3DAdvanced',model_file='footballer01_uv.glb',viewport_state={},width=512,height=512),
      '3':node('HunyuanPaint21LocalD',model_3d=['2',0],reference=['1',0],steps=20,view_size=512,texture_size=2048,seed=seed,reuse_result=True),
      '4':node('SaveGLB',mesh=['3',0],filename_prefix='FootballSim_Fresh/footballer01_painted'),
      '5':node('SaveImage',images=['3',1],filename_prefix='FootballSim_Fresh/footballer01_albedo'),
      '6':node('SaveImage',images=['3',2],filename_prefix='FootballSim_Fresh/footballer01_mr')}
    run(workflow,'paint')


if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('stage',choices=['reference','shape','paint']); p.add_argument('--image',type=Path); p.add_argument('--seed',type=int,default=2026101501)
    args=p.parse_args()
    if args.stage=='reference': reference(args.seed)
    elif args.stage=='shape': shape(args.image,args.seed)
    else: paint(args.image,args.seed)
