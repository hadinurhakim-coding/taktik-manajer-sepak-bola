"""Called by setup_assets.py; legacy FBX importer, shared skeleton and real mesh LODs."""
import json
from pathlib import Path
import unreal

def import_characters():
    project = Path(unreal.Paths.project_dir()).resolve()
    root = project.parent
    manifest_path = root/'04_Exported_GameAssets/asset_manifest.json'
    if not manifest_path.exists():
        unreal.log_warning('No Blender asset manifest yet; placeholder viewer remains available.')
        return
    manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
    report={'status':'RUNNING','animations':[]}
    def run_import(source, dest, name, options):
        task=unreal.AssetImportTask()
        task.set_editor_property('filename',str(source))
        task.set_editor_property('destination_path',dest)
        task.set_editor_property('destination_name',name)
        task.set_editor_property('automated',True)
        task.set_editor_property('replace_existing',True)
        task.set_editor_property('save',True)
        task.set_editor_property('factory',unreal.FbxFactory())
        task.set_editor_property('options',options)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        paths=task.get_editor_property('imported_object_paths')
        if not paths: raise RuntimeError('No imported assets for '+str(source))
        return [unreal.load_asset(p) for p in paths]
    options=unreal.FbxImportUI()
    options.set_editor_property('automated_import_should_detect_type',False)
    options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property('import_as_skeletal',True)
    options.set_editor_property('import_mesh',True)
    options.set_editor_property('import_animations',False)
    options.set_editor_property('import_materials',True)
    options.set_editor_property('import_textures',True)
    assets=run_import(root/manifest['character_lods'][0]['path'],'/Game/Football/Characters','SK_NicoPaz',options)
    mesh=next((a for a in assets if isinstance(a,unreal.SkeletalMesh)),None)
    if mesh is None: raise RuntimeError('Character did not import as SkeletalMesh')
    skeleton=mesh.get_editor_property('skeleton')
    subsystem=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    for entry in manifest['character_lods'][1:]:
        result=subsystem.import_lod(mesh,entry['lod'],str(root/entry['path']))
        if result < 0: raise RuntimeError('LOD import failed: '+str(entry['lod']))
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    report.update(mesh=mesh.get_path_name(),skeleton=skeleton.get_path_name(),lod_count=subsystem.get_lod_count(mesh))
    for entry in manifest['animations']:
        opts=unreal.FbxImportUI()
        opts.set_editor_property('automated_import_should_detect_type',False)
        opts.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_ANIMATION)
        opts.set_editor_property('import_as_skeletal',True)
        opts.set_editor_property('import_mesh',False)
        opts.set_editor_property('import_animations',True)
        opts.set_editor_property('skeleton',skeleton)
        opts.set_editor_property('import_materials',False)
        opts.set_editor_property('import_textures',False)
        opts.anim_sequence_import_data.set_editor_property('animation_length',unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        loaded=run_import(root/entry['path'],'/Game/Football/Animations','A_'+entry['name'],opts)
        anim=next((a for a in loaded if isinstance(a,unreal.AnimSequence)),None)
        if anim is None: raise RuntimeError('No animation: '+entry['name'])
        # The replay supplies locomotion coordinates. Prevent source root motion
        # from displacing the character a second time.
        anim.set_editor_property('enable_root_motion',False)
        anim.set_editor_property('force_root_lock',True)
        unreal.EditorAssetLibrary.save_loaded_asset(anim)
        report['animations'].append({'name':entry['name'],'path':anim.get_path_name(),'seconds':anim.get_play_length()})
        unreal.log('FSIM_ANIMATION_IMPORTED '+entry['name'])
    report['status']='SUCCESS'
    (root/'00_Specs_and_Logs/stage3_1_ue_import.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log('FSIM_CHARACTER_IMPORT_OK')

if __name__=='__main__': import_characters()
