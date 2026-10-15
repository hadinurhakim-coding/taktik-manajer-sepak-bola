import bpy, json
from pathlib import Path
root=Path(__file__).resolve().parent
source=Path(r'D:\Hadi\proyek\manajer-sepak-bola\build\nico-paz\blender-session-autosave.blend')
with bpy.data.libraries.load(str(source),link=False) as (src,dst):
    dst.objects=src.objects
    dst.actions=src.actions
for obj in dst.objects:
    if obj: bpy.context.scene.collection.objects.link(obj)
result={'objects':[{'name':o.name,'type':o.type,'parent':o.parent.name if o.parent else None,'scale':list(o.scale),'dimensions':list(o.dimensions),'location':list(o.location),'modifiers':[(m.name,m.type) for m in o.modifiers], 'vertices':len(o.data.vertices) if o.type=='MESH' else 0} for o in dst.objects if o],
        'actions':[{'name':a.name,'frames':list(a.frame_range),'slots':[(s.identifier,s.target_id_type) for s in a.slots]} for a in dst.actions if a]}
(root/'source_inspection.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print('EXPORT_SOURCE_INSPECTED',flush=True)
