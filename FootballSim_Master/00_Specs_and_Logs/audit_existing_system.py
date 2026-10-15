"""Read-only source/asset/model inventory; outputs only beside this script."""
import ast
import hashlib
import json
import struct
import subprocess
from collections import Counter
from datetime import datetime, timezone, timedelta
from pathlib import Path

OUT = Path(__file__).resolve().parent
REPO = Path(r"D:\Hadi\proyek\manajer-sepak-bola")
MODELS = Path(r"D:\ComfyUI_windows_portable\ComfyUI\models")

def listed(root):
    result = subprocess.run(["rg", "--files", "-uuu", "-g", "!.git", str(root)], capture_output=True, text=True)
    if result.returncode not in (0, 1):
        raise RuntimeError(result.stderr)
    return sorted(Path(p) for p in result.stdout.splitlines())

def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def save(name, obj):
    (OUT / name).write_text(json.dumps(obj, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

def inspect_gltf(path):
    if path.suffix.lower() == ".glb":
        with path.open("rb") as stream:
            magic, version, total = struct.unpack("<4sII", stream.read(12))
            if magic != b"glTF" or version != 2:
                raise ValueError("Not a glTF 2 GLB")
            size, kind = struct.unpack("<II", stream.read(8))
            if kind != 0x4e4f534a:
                raise ValueError("Missing JSON chunk")
            obj = json.loads(stream.read(size))
    else:
        obj = json.loads(path.read_text(encoding="utf-8-sig"))
    nodes = obj.get("nodes", [])
    parents = {child: i for i, node in enumerate(nodes) for child in node.get("children", [])}
    skins = []
    for skin in obj.get("skins", []):
        skins.append({"name": skin.get("name"), "skeleton_node": skin.get("skeleton"),
                      "joints": [{"index": i, "name": nodes[i].get("name", f"node_{i}"),
                                  "parent_index": parents.get(i)} for i in skin["joints"]]})
    animations = []
    for index, animation in enumerate(obj.get("animations", [])):
        inputs = [obj["accessors"][s["input"]] for s in animation.get("samplers", [])]
        animations.append({"name": animation.get("name", f"animation_{index}"),
                           "channels": len(animation.get("channels", [])),
                           "start_seconds": min((a["min"][0] for a in inputs if "min" in a), default=None),
                           "end_seconds": max((a["max"][0] for a in inputs if "max" in a), default=None),
                           "target_paths": sorted({c["target"]["path"] for c in animation.get("channels", [])})})
    return {"generator": obj.get("asset", {}).get("generator"), "meshes": len(obj.get("meshes", [])),
            "node_count": len(nodes), "skins": skins, "animations": animations,
            "external_buffers": [b["uri"] for b in obj.get("buffers", []) if "uri" in b]}

def main():
    files = listed(REPO)
    sources = [p for p in files if (p.relative_to(REPO).parts[0] == "backend" and p.suffix == ".py")
               or ("app/src/" in p.relative_to(REPO).as_posix() and p.suffix in (".kt", ".gd"))]
    source_manifest = [{"path": p.relative_to(REPO).as_posix(), "sha256": digest(p)} for p in sources]
    save("source_integrity_before.json", source_manifest)
    tables = []
    functions = []
    for path in sorted((REPO / "backend").glob("*.py")):
        tree = ast.parse(path.read_text(encoding="utf-8-sig"))
        for node in ast.walk(tree):
            if isinstance(node, ast.Constant) and isinstance(node.value, str) and "CREATE TABLE" in node.value.upper():
                tables.append({"source": path.relative_to(REPO).as_posix(), "line": node.lineno, "sql": node.value.strip()})
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                functions.append({"source": path.relative_to(REPO).as_posix(), "line": node.lineno,
                                  "end_line": node.end_lineno, "function": node.name})
    assets = []
    for path in files:
        if path.suffix.lower() not in (".glb", ".gltf", ".fbx", ".blend"):
            continue
        item = {"path": path.relative_to(REPO).as_posix(), "bytes": path.stat().st_size, "sha256": digest(path)}
        if path.suffix.lower() in (".glb", ".gltf"):
            try:
                item.update(inspect_gltf(path))
                item["inspection"] = "glTF JSON chunk, no import or modification"
            except Exception as error:
                item["error"] = str(error)
        else:
            item["inspection"] = "pending Blender read-only inspection"
        assets.append(item)
    model_files = []
    extensions = {".safetensors", ".ckpt", ".pt", ".pth", ".onnx", ".bin", ".gguf"}
    for path in listed(MODELS):
        rel = path.relative_to(MODELS).as_posix()
        model_files.append({"relative_path": rel, "filename": path.name, "bytes": path.stat().st_size,
                            "is_model_weight": path.suffix.lower() in extensions})
    raw = {"recorded_at": datetime.now(timezone(timedelta(hours=7))).isoformat(), "repository": str(REPO),
           "top_level": [{"name": p.name, "kind": "directory" if p.is_dir() else "file"} for p in sorted(REPO.iterdir())],
           "file_count_excluding_git": len(files),
           "directory_file_counts": dict(Counter(p.relative_to(REPO).parts[0] for p in files)),
           "sql_definitions": tables, "python_functions": functions, "assets": assets,
           "comfyui_models_root": str(MODELS), "comfyui_model_files": model_files,
           "comfyui_model_directories": [p.name for p in sorted(MODELS.iterdir()) if p.is_dir()]}
    save("existing_system_inventory.json", raw)
    (OUT / "comfyui_model_paths.txt").write_text("Root: " + str(MODELS) + "\n\n" +
        "\n".join(f"{p['relative_path']}\t{p['bytes']} bytes" for p in model_files if p['is_model_weight']) + "\n", encoding="utf-8")
    print(json.dumps({"sources_hashed": len(sources), "assets": len(assets),
                      "asset_extensions": dict(Counter(Path(a['path']).suffix for a in assets)),
                      "model_files_all": len(model_files), "model_weights": sum(p['is_model_weight'] for p in model_files),
                      "asset_errors": [a['path'] for a in assets if 'error' in a]}, indent=2))

if __name__ == "__main__":
    main()
