"""Headless stadium material setup and character audit; no source FBX reimport."""
import json
from pathlib import Path
import unreal

def player_material(root, mesh):
    manifest = root/'04_Exported_GameAssets/kits_textures/texture_manifest.json'
    if not manifest.exists(): raise RuntimeError('Run export_character_textures.py first')
    images = json.loads(manifest.read_text(encoding='utf-8'))[:3]
    textures = []
    for i, entry in enumerate(images):
        task = unreal.AssetImportTask()
        for key,value in {'filename':entry['path'],'destination_path':'/Game/Football/Textures',
                          'destination_name':['T_Nico_BaseColor','T_Nico_ORM','T_Nico_Normal'][i],
                          'automated':True,'replace_existing':True,'save':True}.items():
            task.set_editor_property(key,value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        tex = unreal.load_asset(task.imported_object_paths[0])
        if i > 0: tex.set_editor_property('srgb',False)
        if i == 2:
            tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property('flip_green_channel',True)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
        textures.append(tex)
    mat = unreal.load_asset('/Game/Viewer/M_PlayerKit')
    if mat is None: mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_PlayerKit','/Game/Viewer',unreal.Material,unreal.MaterialFactoryNew())
    lib=unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_skeletal_mesh',True)
    def node(cls, **props):
        n=lib.create_material_expression(mat,cls,0,0)
        for k,v in props.items(): n.set_editor_property(k,v)
        return n
    def link(a,out,b,slot):
        if slot=='Input': slot=lib.get_material_expression_input_names(b)[0]
        if not lib.connect_material_expressions(a,out,b,slot):
            raise RuntimeError('Material connection failed: '+str(b.get_class().get_name())+' / '+slot)
    base=node(unreal.MaterialExpressionTextureSample,texture=textures[0])
    diff=node(unreal.MaterialExpressionSubtract); link(base,'B',diff,'A'); link(base,'R',diff,'B')
    gain=node(unreal.MaterialExpressionMultiply,const_b=8.0); link(diff,'',gain,'A')
    mask=node(unreal.MaterialExpressionClamp,min_default=0.0,max_default=1.0); link(gain,'',mask,'Input')
    gray=node(unreal.MaterialExpressionDesaturation); link(base,'RGB',gray,'Input')
    tint=node(unreal.MaterialExpressionVectorParameter,parameter_name='KitTint',default_value=unreal.LinearColor(0.035,0.22,0.8,1))
    colored=node(unreal.MaterialExpressionMultiply); link(gray,'',colored,'A'); link(tint,'',colored,'B')
    boost=node(unreal.MaterialExpressionMultiply,const_b=2.5); link(colored,'',boost,'A')
    blend=node(unreal.MaterialExpressionLinearInterpolate); link(base,'RGB',blend,'A'); link(boost,'',blend,'B'); link(mask,'',blend,'Alpha')
    lib.connect_material_property(blend,'',unreal.MaterialProperty.MP_BASE_COLOR)
    orm=node(unreal.MaterialExpressionTextureSample,texture=textures[1],sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    lib.connect_material_property(orm,'G',unreal.MaterialProperty.MP_ROUGHNESS)
    normal=node(unreal.MaterialExpressionTextureSample,texture=textures[2],sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    lib.connect_material_property(normal,'RGB',unreal.MaterialProperty.MP_NORMAL)
    lib.recompile_material(mat); unreal.EditorAssetLibrary.save_loaded_asset(mat)
    slots=list(mesh.get_editor_property('materials'))
    for i,slot in enumerate(slots):
        slot.set_editor_property('material_interface',mat)
        slots[i]=slot
    mesh.set_editor_property('materials',slots); unreal.EditorAssetLibrary.save_loaded_asset(mesh)

def setup():
    name = 'M_StadiumSurface'
    path = '/Game/Viewer/' + name
    mat = unreal.load_asset(path)
    if mat is None:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, '/Game/Viewer', unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    tint = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -300, 0)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(0.15, 0.25, 0.1, 1))
    unreal.MaterialEditingLibrary.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 160)
    rough.set_editor_property('r', 0.88)
    unreal.MaterialEditingLibrary.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    mesh = unreal.load_asset('/Game/Footballer01/Characters/SK_Footballer01')
    if mesh is None: raise RuntimeError('Missing rigged character: run setup_assets.py first')
    root = Path(unreal.Paths.project_dir()).resolve().parent
    # Footballer01's material and textures are configured by its own importer.
    sky=unreal.load_asset('/Game/Viewer/M_StadiumSky')
    if sky is None: sky=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_StadiumSky','/Game/Viewer',unreal.Material,unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(sky)
    sky.set_editor_property('two_sided',True)
    sky.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    c=unreal.MaterialEditingLibrary.create_material_expression(sky,unreal.MaterialExpressionConstant3Vector,0,0)
    c.set_editor_property('constant',unreal.LinearColor(0.30,0.42,0.58,1))
    unreal.MaterialEditingLibrary.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(sky); unreal.EditorAssetLibrary.save_loaded_asset(sky)
    bounds = mesh.get_bounds()
    report = {'material': path, 'bounds_origin': [bounds.origin.x,bounds.origin.y,bounds.origin.z],
              'bounds_extent': [bounds.box_extent.x,bounds.box_extent.y,bounds.box_extent.z], 'materials': []}
    for slot in mesh.get_editor_property('materials'):
        m = slot.material_interface
        textures = unreal.MaterialEditingLibrary.get_material_used_textures(m) if m else []
        report['materials'].append({'slot':str(slot.material_slot_name),'path':m.get_path_name() if m else None,
                                    'textures':[t.get_path_name() for t in textures]})
    root = Path(unreal.Paths.project_dir()).resolve().parent
    (root/'00_Specs_and_Logs/stage3_2_asset_audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log('STAGE32_ASSETS_READY '+json.dumps(report))

if __name__ == '__main__': setup()
