import unreal

asset_path = '/Game/Viewer/M_ViewerColor'
material = unreal.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None
if material is None:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_ViewerColor', '/Game/Viewer', unreal.Material, unreal.MaterialFactoryNew())
    tint = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -300, 0)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(0.1, 0.5, 0.2, 1))
    unreal.MaterialEditingLibrary.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)
    # Small emissive contribution keeps team colours legible in a simple scene.
    emissive = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionMultiply, -100, 160)
    emissive.set_editor_property('const_b', 0.18)
    unreal.MaterialEditingLibrary.connect_material_expressions(tint, '', emissive, 'A')
    unreal.MaterialEditingLibrary.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -100, 300)
    rough.set_editor_property('r', 0.85)
    unreal.MaterialEditingLibrary.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('Could not save viewer material')
unreal.log('FSIM_ASSET_SETUP_OK: ' + asset_path)

# An unlit material keeps turf, lines and locator rings readable independently
# of the scene lighting, while the player bodies retain their lit 3D shading.
flat_path = '/Game/Viewer/M_ViewerFlat'
if not unreal.EditorAssetLibrary.does_asset_exist(flat_path):
    flat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_ViewerFlat', '/Game/Viewer', unreal.Material, unreal.MaterialFactoryNew())
    flat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    tint = unreal.MaterialEditingLibrary.create_material_expression(flat, unreal.MaterialExpressionVectorParameter, -300, 0)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(0.016, 0.16, 0.06, 1))
    unreal.MaterialEditingLibrary.connect_material_property(tint, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(flat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(flat):
        raise RuntimeError('Could not save flat viewer material')
unreal.log('FSIM_ASSET_SETUP_OK: ' + flat_path)

import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from import_fresh_character import main as import_characters
import_characters()
from setup_broadcast_assets import setup
setup()
