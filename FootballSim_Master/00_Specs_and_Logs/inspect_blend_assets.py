"""Load only armature/action datablocks, never execute or save an old .blend."""
import bpy
import json
from pathlib import Path

out = Path(__file__).resolve().parent
inventory = json.loads((out / "existing_system_inventory.json").read_text(encoding="utf-8"))
results = []
for item in inventory["assets"]:
    if not item["path"].lower().endswith(".blend"):
        continue
    path = Path(inventory["repository"]) / item["path"]
    result = {"path": item["path"], "inspection": "bpy.data.libraries.load; no scene execution or save"}
    try:
        with bpy.data.libraries.load(str(path), link=False) as (source, target):
            result["mesh_names"] = list(source.meshes)
            target.armatures = source.armatures
            target.actions = source.actions
        result["armatures"] = [{"name": armature.name, "bones": [
            {"name": bone.name, "parent": bone.parent.name if bone.parent else None}
            for bone in armature.bones]} for armature in target.armatures if armature]
        result["actions"] = [{"name": action.name, "frame_range": list(action.frame_range)}
                             for action in target.actions if action]
        for armature in target.armatures:
            if armature:
                bpy.data.armatures.remove(armature)
        for action in target.actions:
            if action:
                bpy.data.actions.remove(action)
    except Exception as error:
        result["error"] = str(error)
    results.append(result)
    print("AUDIT_BLEND", item["path"], len(result.get("armatures", [])), len(result.get("actions", [])), result.get("error", "OK"), flush=True)
    (out / "blend_asset_details.json").write_text(json.dumps({"blender_version": bpy.app.version_string, "files": results}, indent=2), encoding="utf-8")
