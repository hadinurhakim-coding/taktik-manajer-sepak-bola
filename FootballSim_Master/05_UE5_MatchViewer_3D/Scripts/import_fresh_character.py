"""Import Footballer01 into its own UE package and persist every dependency."""
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve().parent
BASE='/Game/Footballer01'

def import_file(source,dest,name,options=None):
    task=unreal.AssetImportTask()
    for key,value in {'filename':str(source),'destination_path':dest,'destination_name':name,'automated':True,'replace_existing':True,'save':True}.items():task.set_editor_property(key,value)
    if options is not None:
        task.set_editor_property('factory',unreal.FbxFactory());task.set_editor_property('options',options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assets=[unreal.load_asset(p) for p in task.imported_object_paths]
    if not assets:raise RuntimeError('No imported assets: '+str(source))
    return assets

def materials(manifest,mesh):
    lib=unreal.MaterialEditingLibrary
    textures={}
    for entry in manifest['textures']:
        tex=import_file(ROOT/entry['path'],BASE+'/Textures','T_Footballer01_'+entry['role'])[0]
        if entry['role']!='basecolor':tex.set_editor_property('srgb',False)
        unreal.EditorAssetLibrary.save_loaded_asset(tex);textures[entry['role']]=tex
    def material(name):
        path=BASE+'/Materials/'+name
        mat=unreal.load_asset(path)
        if mat is None:mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
        lib.delete_all_material_expressions(mat);mat.set_editor_property('used_with_skeletal_mesh',True)
        return mat
    mat=material('M_Footballer01_Kit')
    def node(cls,**props):
        n=lib.create_material_expression(mat,cls,0,0)
        for k,v in props.items():n.set_editor_property(k,v)
        return n
    def link(a,out,b,slot):
        if slot=='Input':slot=lib.get_material_expression_input_names(b)[0]
        if not lib.connect_material_expressions(a,out,b,slot):raise RuntimeError('Material link failed: '+slot)
    base=node(unreal.MaterialExpressionTextureSample,texture=textures['basecolor'])
    gray=node(unreal.MaterialExpressionDesaturation);link(base,'RGB',gray,'Input')
    tint=node(unreal.MaterialExpressionVectorParameter,parameter_name='KitTint',default_value=unreal.LinearColor(.04,.2,.8,1))
    colored=node(unreal.MaterialExpressionMultiply);link(gray,'',colored,'A');link(tint,'',colored,'B')
    boost=node(unreal.MaterialExpressionMultiply,const_b=1.6);link(colored,'',boost,'A')
    mask=node(unreal.MaterialExpressionVertexColor)
    blend=node(unreal.MaterialExpressionLinearInterpolate);link(base,'RGB',blend,'A');link(boost,'',blend,'B');link(mask,'R',blend,'Alpha')
    lib.connect_material_property(blend,'',unreal.MaterialProperty.MP_BASE_COLOR)
    if 'metallic_roughness' in textures:
        mr=node(unreal.MaterialExpressionTextureSample,texture=textures['metallic_roughness'],sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        rough=node(unreal.MaterialExpressionClamp,min_default=.55,max_default=.95);link(mr,'G',rough,'Input');lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
    skin=material('M_Footballer01_Skin')
    c=lib.create_material_expression(skin,unreal.MaterialExpressionConstant3Vector,0,0);c.set_editor_property('constant',unreal.LinearColor(.48,.285,.18,1));lib.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
    r=lib.create_material_expression(skin,unreal.MaterialExpressionConstant,0,100);r.set_editor_property('r',.65);lib.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(skin);unreal.EditorAssetLibrary.save_loaded_asset(skin)
    slots=list(mesh.get_editor_property('materials'))
    for i,slot in enumerate(slots):
        slot.set_editor_property('material_interface',mat if i==0 else skin);slots[i]=slot
    mesh.set_editor_property('materials',slots);unreal.EditorAssetLibrary.save_loaded_asset(mesh)

def main():
    manifest=json.loads((ROOT/'04_Exported_GameAssets/Footballer01/asset_manifest.json').read_text())
    audit=json.loads((ROOT/'00_Specs_and_Logs/footballer01_deformation_audit.json').read_text())
    if audit['status']!='NUMERICAL_CHECKS_PASS_REQUIRES_VISUAL_REVIEW':raise RuntimeError('New rig has not passed deformation checks')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
    opts=unreal.FbxImportUI()
    for k,v in {'automated_import_should_detect_type':False,'mesh_type_to_import':unreal.FBXImportType.FBXIT_SKELETAL_MESH,
                'import_as_skeletal':True,'import_mesh':True,'import_animations':False,'import_materials':False,'import_textures':False}.items():opts.set_editor_property(k,v)
    opts.skeletal_mesh_import_data.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
    canonical=BASE+'/Characters/SK_Footballer01_Skeleton'
    if unreal.EditorAssetLibrary.does_asset_exist(canonical):opts.set_editor_property('skeleton',unreal.load_asset(canonical))
    assets=import_file(ROOT/manifest['character_lods'][0]['path'],BASE+'/Characters','SK_Footballer01',opts)
    mesh=next(a for a in assets if isinstance(a,unreal.SkeletalMesh));skeleton=mesh.get_editor_property('skeleton')
    if skeleton.get_path_name().split('.')[0]!=canonical:
        if not unreal.EditorAssetLibrary.rename_asset(skeleton.get_path_name(),canonical):raise RuntimeError('Skeleton rename failed')
    unreal.EditorAssetLibrary.save_loaded_asset(skeleton,only_if_is_dirty=False)
    subsystem=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    for entry in manifest['character_lods'][1:]:
        if subsystem.import_lod(mesh,entry['lod'],str(ROOT/entry['path']))<0:raise RuntimeError('LOD import failed')
    materials(manifest,mesh)
    report={'mesh':mesh.get_path_name(),'skeleton':skeleton.get_path_name(),'lod_count':subsystem.get_lod_count(mesh),'animations':[]}
    for entry in manifest['animations']:
        opts=unreal.FbxImportUI()
        for k,v in {'automated_import_should_detect_type':False,'mesh_type_to_import':unreal.FBXImportType.FBXIT_ANIMATION,'import_as_skeletal':True,
                    'import_mesh':False,'import_animations':True,'import_materials':False,'import_textures':False,'skeleton':skeleton}.items():opts.set_editor_property(k,v)
        opts.anim_sequence_import_data.set_editor_property('animation_length',unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        assets=import_file(ROOT/entry['path'],BASE+'/Animations','A_'+entry['name'],opts)
        anim=next(a for a in assets if isinstance(a,unreal.AnimSequence));anim.set_editor_property('enable_root_motion',False);anim.set_editor_property('force_root_lock',True)
        unreal.EditorAssetLibrary.save_loaded_asset(anim);report['animations'].append({'name':entry['name'],'seconds':anim.get_play_length()})
    unreal.EditorAssetLibrary.save_directory(BASE,only_if_is_dirty=False,recursive=True)
    report['status']='SUCCESS';(ROOT/'00_Specs_and_Logs/footballer01_ue_import.json').write_text(json.dumps(report,indent=2))
    unreal.log('FOOTBALLER01_IMPORT_SUCCESS')

if __name__=='__main__':main()
