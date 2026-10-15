"""Export packed source images without saving or modifying the source .blend."""
import bpy
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = Path(r'D:\Hadi\proyek\manajer-sepak-bola\build\nico-paz\blender-session-autosave.blend')
out = root/'04_Exported_GameAssets/kits_textures'
out.mkdir(exist_ok=True)
with bpy.data.libraries.load(str(source),link=False) as (src,dst):
    dst.materials = ['nico_paz','perbaikan_wajah_local_portrait']
report = []
for index, mat in enumerate(dst.materials):
    if mat is None: raise RuntimeError('Source material missing')
    for n in mat.node_tree.nodes:
        if n.type == 'TEX_IMAGE' and n.image:
            path = out/f'nico_material_{index}_{len(report)}.png'
            # Save original pixels, not a rendered/color-managed material preview.
            image=n.image.copy(); image.filepath_raw=str(path); image.file_format='PNG'; image.save()
            report.append({'material':mat.name,'image':n.image.name,'path':str(path),
                           'outputs':[(link.to_node.name,link.to_socket.name) for socket in n.outputs for link in socket.links]})
            bpy.data.images.remove(image)
(out/'texture_manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('TEXTURE_EXPORT',json.dumps(report),flush=True)
