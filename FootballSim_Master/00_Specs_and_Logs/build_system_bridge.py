"""Assemble evidence-backed documentation without importing the old backend."""
import ast
import hashlib
import json
import re
from datetime import datetime, timezone, timedelta
from pathlib import Path

OUT = Path(__file__).resolve().parent
ROOT = OUT.parent
REPO = Path(r"D:\Hadi\proyek\manajer-sepak-bola")
raw = json.loads((OUT / "existing_system_inventory.json").read_text(encoding="utf-8"))
blend = json.loads((OUT / "blend_asset_details.json").read_text(encoding="utf-8"))

def save(name, obj):
    (OUT / name).write_text(json.dumps(obj, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def ref(path, line):
    return {"path": path, "line": line}

def constants(path):
    result = {}
    for node in ast.parse((REPO / path).read_text(encoding="utf-8-sig")).body:
        if isinstance(node, ast.Assign):
            try:
                value = ast.literal_eval(node.value)
            except (ValueError, TypeError):
                continue
            for target in node.targets:
                if isinstance(target, ast.Name) and target.id.isupper():
                    result[target.id] = value
    return result

constant_files = ["backend/friendly_simulation.py", "backend/player_motion.py", "backend/action_phase.py",
                  "backend/off_ball.py", "backend/ball_trajectory.py", "backend/attacking_play.py", "backend/broadcast.py"]
all_constants = {path: constants(path) for path in constant_files}
flat = {key: value for values in all_constants.values() for key, value in values.items()}
tables = {}
for block in raw["sql_definitions"]:
    for match in re.finditer(r"CREATE TABLE IF NOT EXISTS\s+(\w+)\s*\(.*?;", block["sql"], re.S | re.I):
        name = match.group(1)
        source_lines = (REPO / block["source"]).read_text(encoding="utf-8-sig").splitlines()
        line = next(i + 1 for i, text in enumerate(source_lines) if i + 1 >= block["line"] and re.search(r"CREATE TABLE IF NOT EXISTS\s+" + name + r"\s*\(", text))
        definition = {"source": ref(block["source"], line), "sql": match.group(0)}
        if not any(d["sql"] == definition["sql"] for d in tables.get(name, [])):
            tables.setdefault(name, []).append(definition)

asset_files = raw["assets"]
blend_by_path = {item["path"]: item for item in blend["files"]}
for item in asset_files:
    if item["path"] in blend_by_path:
        item.update(blend_by_path[item["path"]])
main_asset = next(a for a in asset_files if a["path"] == "app/src/main/assets/lawatan/models/nico_paz_como1907.glb")
extra_asset = next(a for a in asset_files if a["path"] == "art/animation/football_20261015/football_actions.glb")
source_before = json.loads((OUT / "source_integrity_before.json").read_text(encoding="utf-8"))
changed_sources = [item["path"] for item in source_before if digest(REPO / item["path"]) != item["sha256"]]
changed_assets = [item["path"] for item in asset_files if digest(REPO / item["path"]) != item["sha256"]]
integrity = {"status": "PASS" if not changed_sources + changed_assets else "CHANGED_DURING_AUDIT",
             "source_files_checked": len(source_before), "asset_files_checked": len(asset_files),
             "changed_sources": changed_sources, "changed_assets": changed_assets,
             "method": "SHA-256 before and after; does not claim to monitor every file in repository",
             "audit_writes_to_old_repository": False,
             "old_backend_imported_or_executed": False, "sqlite_database_opened": False,
             "blender_method": "--background --factory-startup --disable-autoexec; load armature/action datablocks only; never save"}
save("repository_readonly_verification.json", integrity)

model_files = raw["comfyui_model_files"]
weights = [item for item in model_files if item["is_model_weight"]]
for item in weights:
    category, name = item["relative_path"].split("/", 1)
    item["category"] = category
    item["category_relative_path"] = name
    item["category_relative_windows_path"] = name.replace("/", "\\")
external_root = Path(r"D:\AI\Hunyuan3D-Paint-2.1\models")
external_weights = [{"absolute_path": str(p), "relative_path": p.relative_to(external_root).as_posix(),
                     "bytes": p.stat().st_size} for p in sorted(external_root.rglob("*"))
                    if p.is_file() and p.suffix.lower() in (".safetensors", ".bin", ".pt", ".pth", ".ckpt")]

exe = ROOT / "01_SimulationCore_CPP/build/toolchain_check/Release/toolchain_check.exe"
build_log = (OUT / "toolchain_build.log").read_text(encoding="utf-8-sig")
assert "TOOLCHAIN_CHECK=PASS" in build_log and exe.is_file()
toolchain = {
    "status": "PASS", "installed_using": "Visual Studio Installer CLI modify (additive components)",
    "installation_path": r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools",
    "components": ["Microsoft.VisualStudio.Workload.VCTools", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                   "Microsoft.VisualStudio.Component.Windows11SDK.26100", "Microsoft.VisualStudio.Component.VC.CMake.Project"],
    "msvc_version": "19.44.35222.0", "toolset_directory": "14.44.35207",
    "cl_path": r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe",
    "cmake_version": "3.31.6-msvc6",
    "cmake_path": r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "windows_sdk": "10.0.26100.0", "is_complete": True, "is_reboot_required": False,
    "activation": str(ROOT / "01_SimulationCore_CPP/activate_toolchain.cmd"),
    "activation_usage": "In cmd.exe: call activate_toolchain.cmd. This initializes x64 include/lib/PATH for that terminal.",
    "global_path_changed": False,
    "test_source": str(ROOT / "01_SimulationCore_CPP/tests/toolchain_check.cpp"),
    "reproduce_build": str(ROOT / "01_SimulationCore_CPP/run_toolchain_check.cmd"),
    "generator": "Visual Studio 17 2022", "architecture": "x64", "configuration": "Release",
    "cpp_standard": "C++20", "cplusplus": 202002, "msc_ver": 1944,
    "sizeof_pointer": 8, "sizeof_Vec3": 12, "sizeof_PlayerState": 20, "alignof_PlayerState": 4,
    "windows_sdk_header_and_link_probe": "GetCurrentProcessId from windows.h: OK",
    "build_and_executable_exit_code": 0, "exe_path": str(exe), "exe_sha256": digest(exe),
    "struct_note": "Probe structs only; no .fsim serialization contract is defined by this test.",
    "logs": ["toolchain_install.log", "toolchain_build.log"],
    "official_references": [
        "https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio?view=vs-2022",
        "https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools?view=vs-2022"]}
save("toolchain_verification.json", toolchain)

bridge = {
    "document_version": "1.0", "stage": "1.2", "status": "SUCCESS",
    "generated_at": datetime.now(timezone(timedelta(hours=7))).isoformat(), "timezone": "Asia/Bangkok",
    "repository_root": str(REPO), "source_of_truth": "On-disk working tree including existing uncommitted changes; static audit, no production database rows copied",
    "read_only_verification": integrity,
    "architecture": {
        "top_level_entries": raw["top_level"], "file_counts_by_top_level": raw["directory_file_counts"],
        "components": [
            {"path": "backend/", "role": "Python stdlib ThreadingHTTPServer on port 8088; SQLite persistence; simulation and scheduled jobs"},
            {"path": "app/src/main/java/com/example/", "role": "Android Kotlin / Jetpack Compose management UI; OkHttp JSON clients"},
            {"path": "app/src/main/assets/lawatan/", "role": "Godot match renderer packaged as lawatan.pck"},
            {"path": "app/src/main/java/com/example/friendly/FriendlyMatchActivity.kt", "role": "GodotActivity and LawatanBridge plugin; cached JSON and clock handoff"},
            {"path": "art/", "role": "Character sources and supplemental football animation .blend/.glb"},
            {"path": "Mixar/", "role": "Existing full-match Blender scenes and animation scripts"},
            {"path": "build/nico-paz/", "role": "Intermediate character rigs and exports, inventoried separately from runtime assets"},
            {"path": "docs/lawatan/", "role": "Design documentation and historical test evidence"}],
        "service_initialization": ref("backend/server.py", 57),
        "simulation_modules": [
            {"source": ref("backend/friendly_simulation.py", 136), "role": "Seeded authoritative simulate(teams, seed, league_rules); actions, events, score, clips over 5400 match seconds"},
            {"source": ref("backend/friendly_engine.py", 277), "role": "Prepare/generate lawatan; freeze team snapshot and seed, gzip replay BLOB, READY lifecycle"},
            {"source": ref("backend/league_engine.py", 586), "role": "League round results/standings; consumes prepared replay score; stores replay BLOB"},
            {"source": ref("backend/league_replay.py", 8), "role": "Build frozen league replay and compress it outside the standings transaction"},
            {"source": ref("backend/broadcast.py", 56), "role": "Action-complete highlight selection, exactly 450 presentation seconds per half"},
            {"source": ref("backend/player_motion.py", 52), "role": "Bounded acceleration integration, Hermite player motion frames"},
            {"source": ref("backend/ball_trajectory.py", 13), "role": "Analytic ball trajectories"},
            {"source": ref("backend/action_phase.py", 7), "role": "Release/contact timing and dribble contacts"},
            {"source": ref("backend/attacking_play.py", 1), "role": "Attacking choices, pass lanes, heading/offside"},
            {"source": ref("backend/off_ball.py", 1), "role": "Off-ball planning and flight reaction"},
            {"source": ref("backend/match_rating.py", 1), "role": "Team rating calculation"},
            {"source": ref("backend/scenario_library.py", 1), "role": "Scenario selection metadata attached to preparation/snapshots"}],
        "legacy_fallback": {"source": ref("backend/league_engine.py", 299), "detail": "With _replayTeams, score comes from authoritative replay. Without management snapshots, legacy rating-based score generation remains."}
    },
    "database": {
        "path": str(REPO / "backend/football_manager.db"), "schema_inspection": "Static CREATE TABLE and ALTER TABLE declarations; database not opened",
        "schema_files": ["backend/database.py", "backend/club_engine.py", "backend/friendly_engine.py"],
        "tables": tables,
        "important_storage": {
            "players": {"table": "club_players", "payload_column": "body TEXT", "detail": "JSON player object contains id/name/number/role/anchor and attributes; not one SQL column per attribute", "source": ref("backend/club_engine.py", 78)},
            "tactics": {"table": "club_states", "columns": ["tactics TEXT", "lineup TEXT", "training TEXT"], "detail": "tactics object, ordered 11-player ID lineup, training configuration are JSON", "source": ref("backend/club_engine.py", 72)},
            "league_schedule": {"table": "club_matchdays", "source": ref("backend/club_engine.py", 116)},
            "frozen_lineups": {"table": "club_fixture_snapshots", "payload_column": "body TEXT", "detail": "Pair of home/away frozen team snapshots", "source": ref("backend/club_engine.py", 111)},
            "results": {"table": "fixtures", "columns": ["home_score", "away_score", "is_played", "played_at"], "source": ref("backend/database.py", 152)},
            "league_replays": {"table": "league_match_replays", "payload_column": "body BLOB", "encoding": "UTF-8 JSON -> gzip(level=3, mtime=0)", "source": ref("backend/league_replay.py", 18)},
            "lawatan": {"tables": ["friendly_wallets", "friendly_careers", "friendly_matches", "friendly_rewards"], "replay_column": "friendly_matches.package BLOB", "snapshot_column": "friendly_matches.snapshot TEXT", "source": ref("backend/friendly_engine.py", 34)},
            "career_slots": {"table": "career_slots", "slot_range": [1, 6], "migration": "career_generation TEXT NOT NULL DEFAULT '' added by database.py:269", "source": ref("backend/database.py", 135)},
            "season_history": {"table": "club_season_archives", "payload_column": "body TEXT"}},
        "league_clock": {"timezone": "Asia/Jakarta (UTC+07:00)", "lock_local": "19:55:00", "kickoff_local": "20:00:00", "timestamps": "Unix seconds", "scheduler_poll_seconds": 30,
                         "missed_lock_policy": "If lock cutoff is missed by more than 90 seconds, postpone to next eligible 20:00 instead of reconstructing old lineup",
                         "sources": [ref("backend/league_schedule.py", 9), ref("backend/league_schedule.py", 64), ref("backend/club_engine.py", 140)]}
    },
    "data_contract": {
        "source_constants": all_constants,
        "player_attributes_existing": list(flat["ATTRIBUTES"]) + ["heading"],
        "heading_note": "Heading is generated separately at friendly_simulation.py:55; not listed in ATTRIBUTES tuple",
        "team_snapshot_fields": ["id", "name", "rating", "formation", "style", "fitness", "chemistry", "synthetic", "players", "tactics", "clubRevision", "clubRulesVersion"],
        "team_snapshot_source": ref("backend/club_engine.py", 296),
        "lineup_order": "Two teams; players flattened home[0:11], away[11:22]; owner/receiver/player references are global zero-based indices, goalkeeper indices 0 and 11",
        "coordinates": {"units": "metres", "pitch_length": 105, "pitch_width": 68, "x": "pitch length", "y": "height", "z": "pitch width", "player_position": "[x,z]", "ball_position": "[x,y,z]", "ball_radius": flat["BALL_RADIUS"]},
        "http": {"authorization": "Bearer token", "response_success": {"success": "boolean true", "data": "match/agenda/club object"},
                 "response_failure": {"success": "boolean false", "code": "string", "error": "string"},
                 "content_type": "application/json; charset=utf-8", "compression": "HTTP gzip only if client Accept-Encoding includes gzip and serialized response is >4096 bytes; separate from database BLOB compression",
                 "source": ref("backend/server.py", 67)},
        "endpoints": [
            {"method": "GET", "path": "/api/v1/friendly/agenda?slotNumber={1..6}", "response": "agenda, activeMatch, completed history, fitness/rewards/tickets"},
            {"method": "POST", "path": "/api/v1/friendly/matches", "request": {"slotNumber": "int 1..6", "opponentId": "string", "requestKey": "string 8..80"}, "response": "match metadata; idempotent prepare"},
            {"method": "GET", "path": "/api/v1/friendly/matches/{id}", "response": "match metadata with replay object when package exists"},
            {"method": "POST", "path": "/api/v1/friendly/matches/{id}/start", "request": {}, "response": "official startedAt, endsAt, serverNow"},
            {"method": "POST", "path": "/api/v1/friendly/matches/{id}/settle", "request": {}, "response": "settlement/result metadata"},
            {"method": "GET", "path": "/api/v1/club/replay?slotNumber={slot}&fixtureId={id}", "response": {"id": "fixture ID", "slotNumber": "int", "mode": "league", "status": "COMPLETED", "replay": "object"}},
            {"method": "GET", "path": "/api/v1/club/state?slotNumber={slot}", "response": "players, lineup, tactics, schedule and club state"},
            {"method": "POST", "path": "/api/v1/club/action", "request": {"slotNumber": "int", "generation": "career generation", "revision": "int", "requestKey": "idempotency key", "action": "string", "payload": "object"}}],
        "endpoint_sources": [ref("backend/server.py", 684), ref("backend/server.py", 714)],
        "friendly_match_metadata_fields": ["id", "slotNumber", "generation", "status", "homeName", "awayName", "createdAt", "startedAt", "endsAt", "serverNow", "rewardApplied", "error", "duration", "synthetic", "score (after endsAt)", "replay (when requested and ready)"],
        "friendly_statuses": ["PREPARING", "READY", "RUNNING", "COMPLETED", "FAILED", "EXPIRED"],
        "replay": {
            "source": ref("backend/friendly_simulation.py", 460),
            "versions": {"version": flat["ENGINE_VERSION"], "rulesVersion": flat["RULES_VERSION"], "motionVersion": flat["MOTION_VERSION"], "phaseVersion": flat["PHASE_VERSION"], "behaviourVersion": flat["BEHAVIOUR_VERSION"], "ballVersion": flat["BALL_VERSION"], "attackVersion": flat["ATTACK_VERSION"], "broadcastVersion": flat["BROADCAST_VERSION"]},
            "top_level": {"seed": "string representation of integer", "mode": "friendly|league", "duration": 900, "halfDuration": 450, "matchDuration": 5400, "ballRadius": 0.11, "teams": "two team snapshots", "score": "[homeGoals,awayGoals]", "events": "event[]", "actions": "action[]", "clips": "clip[]", "limitations": "string[]"},
            "optional_additions": {"friendly": ["home_rating", "away_rating", "scenario"], "league": ["fixtureId", "competitionName", "matchday"]},
            "action": {"t": "match seconds", "duration": "seconds >0", "half": "1|2", "kind": "action enum", "outcome": "string (empty allowed)", "owner": "player index 0..21", "receiver": "player index 0..21", "intendedReceiver": "player index 0..21", "team": "0|1", "cut": "boolean", "playersFrom": "22 x [x,z]", "playersTo": "22 x [x,z]", "ball": "trajectory object", "motion": "motion object", "phase": "contact phase object", "intent": "off-ball intent object", "contacts?": "dribble phase[]", "chance?": "shot probability", "keeper?": "{player,start,impact,recover,target,outcome}"},
            "motion": {"step": "seconds, <=0.5", "frames": "N x 22 x [x,z,vx,vz]; (N-1)*step = action.duration", "interpolation": "Cubic Hermite using positions and velocities"},
            "ball": {"from": "[x,y,z]", "to": "[x,y,z]", "duration": "seconds", "mode": "air|roll", "curve": "number", "restitution": 0.45, "gravity": 9.81, "physicsVersion": "flight-2", "radius": 0.11, "launchAt": "action-relative seconds", "launchFrom": "contact point", "touches?": "[{t,p}] for dribble"},
            "phase": {"start": "seconds", "impact": "seconds", "release": "equals impact", "end": "seconds", "effector": "head|hands|left_foot|right_foot", "contact": "[x,y,z]"},
            "event": {"t": "match seconds at action end", "action": "action index", "kind": "outcome or kind", "team": "0|1", "player": "0..21", "score": "score snapshot"},
            "clip": {"half": "1|2", "playStart": "presentation offset seconds", "matchStart": "match offset seconds", "matchDuration": "content seconds", "leadIn": "transition seconds", "tailHold": "hold seconds", "breaks": "[{at,kind,duration,replayFrom?}]", "duration": "total presentation seconds including breaks/holds"},
            "invariants": ["Total clips.duration is 900; each half is 450", "Goal events sum to replay.score and are represented in highlight clips", "22 player positions per action", "Live outcomes/ball roots decided by backend; viewer does not resimulate", "League official score must equal cached replay.score"],
            "highlight_speed": "Normal highlight content plays at 1x; short Ulangan segments use 0.5x while display clock/score stays at committed event time",
            "sources": [ref("backend/friendly_simulation.py", 414), ref("backend/friendly_simulation.py", 526), ref("backend/player_motion.py", 52), ref("backend/broadcast.py", 32), ref("app/src/main/assets/lawatan/replay_timeline.gd", 35)]}
    },
    "android_and_viewer": {
        "flow": ["OkHttp requests JSON envelope; code consumes success/data", "FriendlyRepository.cacheReplay validates all replay version tags", "Atomic local save: filesDir/friendly/{id}.json using temp + rename", "Intent starts FriendlyMatchActivity with matchId, offsetSeconds, replay, startPending", "Godot loads lawatan.pck; LawatanBridge.getReplay returns cached JSON", "replay_timeline.gd maps presentation clock to match time and 0.5x repeat time", "match_view.gd searches action, interpolates 22 player roots, samples ball, applies cosmetic skeletal poses and camera"],
        "offline": "Supported after replay is cached; ClubRepository uses same viewer for completed league matches without friendly start/settle",
        "live_clock": "serverNow-startedAt + monotonic SystemClock.elapsedRealtime delta; live Godot clock syncs through bridge",
        "playback_clock": "Completed replay starts at zero after viewer readiness and advances locally",
        "sources": [ref("app/src/main/java/com/example/friendly/FriendlyRepository.kt", 88), ref("app/src/main/java/com/example/friendly/FriendlyMatchActivity.kt", 73), ref("app/src/main/java/com/example/data/club/ClubRepository.kt", 84), ref("app/src/main/assets/lawatan/match_view.gd", 1042)],
        "compatibility_issue": {"severity": "blocking new replay ingestion", "observation": "Kotlin validator rejects current backend output: friendly-1.6, broadcast-2, off-ball-2, attacking-2. It accepts at most friendly-1.5 with broadcast-1/off-ball-1/attacking-1.",
                                "effect": "cacheReplay require() fails before the viewer opens, for both friendly and league downloads", "evidence": [ref("app/src/main/java/com/example/friendly/FriendlyReplayFormat.kt", 4), ref("app/src/main/java/com/example/friendly/FriendlyRepository.kt", 90)], "fixed": False,
                                "verification": "Static code comparison; Android application was not run"}
    },
    "migration_to_stage_1_1_constants": {
        "status": "Mapping documentation only; no adapters or old-repo edits performed",
        "attributes": {"ball_control": "control", "heading": "heading (generated separately)", "other_attributes": "same key names"},
        "play_style": {"balanced": "Balanced", "possession": "Possession", "counter_attack": "Counter", "high_press": "Pressing", "wing_play": "Wide", "direct_pass": "Direct"},
        "mentality": {"defensive": "Defensive", "balanced": "Balanced", "attacking": "Attacking"},
        "pressing_and_tempo": "Existing schema uses integer 0..100; new schema uses three enums. No authoritative thresholds/conversion policy exists yet; do not silently discard precision.",
        "additional_tactics": "Existing width 0..100 and specialist backup IDs have no fields in stage 1.1 constants",
        "specialists": {"captain": "captain", "penalty_taker": "penalty", "free_kick_taker": "freeKick", "corner_left_taker": "cornerLeft", "corner_right_taker": "cornerRight"},
        "action_alias_candidates": {"one_two_pass": "one_two", "kick_off": "kickoff", "corner_kick": "corner", "penalty_kick": "penalty"},
        "action_mapping_note": "Alias candidates require adapter implementation. Keeper/off-ball actions often exist as pose/intent/keeper metadata rather than direct action.kind values. Header_pass and keeper_release are additional old kinds.",
        "formations": "All seven requested formation strings already match",
        "future_fsim": "No .fsim writer/reader or binary schema identified in audited replay path. Existing interchange is versioned JSON, gzip in SQLite, plain JSON in Android cache.",
        "sources": [ref("backend/friendly_simulation.py", 23), ref("backend/club_engine.py", 24), ref("backend/club_engine.py", 545), ref("backend/club_specialists.py", 2)]
    },
    "assets_3d": {
        "scope": "All .glb/.gltf/.fbx/.blend paths found by rg --files -uuu excluding .git; includes build intermediates",
        "counts": {"glb": 29, "blend": 7, "gltf": 0, "fbx": 0}, "blender_version": blend["blender_version"],
        "runtime_character": {"path": main_asset["path"], "rig": "Armature_NicoPaz", "joint_count": 25,
                              "joint_names": [j["name"] for j in main_asset["skins"][0]["joints"]], "animations": [a["name"] for a in main_asset["animations"]]},
        "supplemental_football_actions": {"path": extra_asset["path"], "animations": [a["name"] for a in extra_asset["animations"]],
                                          "runtime_resource": "app/src/main/assets/lawatan/football_actions.res", "source": ref("app/src/main/assets/lawatan/player_mesh_rig.gd", 49)},
        "rig_compatibility": "Main GLB uses mixamorig: bone names; Godot controller looks for mixamorig_ names after import. Old original export uses 19 Pelvis/Chest-style joints, build intermediates include 59-joint basemesh_male_rig. Retarget by explicit mapping, not identical indices.",
        "notes": ["Main runtime character has 18 animations; supplemental GLB has 9 more", "Several separate action GLBs contain many duplicate .001/.002/.003 tracks; exact names preserved per file", "Raw/refined character GLBs and several .blend sources have no armature/actions", "Mixar scenes contain per-player root movement and ball actions, distinct from reusable skeletal clips"],
        "files": asset_files
    },
    "comfyui": {
        "models_root": raw["comfyui_models_root"], "path_notation": "relative_path uses forward slashes; category_relative_windows_path preserves Windows subfolder separator for workflow selection strings. Confirm actual dropdown values with /object_info before submitting workflows.",
        "model_weight_count": len(weights), "all_file_count": len(model_files), "directories": raw["comfyui_model_directories"],
        "model_weights": weights, "all_files_including_configs_and_placeholders": model_files,
        "loras": {"model_count": 0, "entries": ["loras/put_loras_here"]},
        "key_workflow_mappings": [
            {"family": "SDXL", "relative_path": "checkpoints/sd_xl_base_1.0.safetensors", "category": "checkpoints", "filename": "sd_xl_base_1.0.safetensors"},
            {"family": "ControlNet OpenPose SDXL", "relative_path": "controlnet/controlnet-openpose-sdxl-1.0.safetensors", "category": "controlnet", "filename": "controlnet-openpose-sdxl-1.0.safetensors"},
            {"family": "Hunyuan3D shape", "relative_path": "diffusion_models/hy3dgen/hunyuan3d-dit-v2-0-fp16.safetensors", "node_class": "Hy3DModelLoader", "input_key": "model", "input_value_windows": "hy3dgen\\hunyuan3d-dit-v2-0-fp16.safetensors", "category": "diffusion_models"},
            {"family": "Hunyuan3D Paint 2.0 Turbo", "pipeline_directory": "diffusers/hunyuan3d-paint-v2-0-turbo", "note": "Diffusers directory with multiple component weights/configs, not a single checkpoint"},
            {"family": "Pixal3D", "relative_path": "diffusion_models/pixal3d_multiview_int8_convrot.safetensors", "category": "diffusion_models", "inference_tested": False},
            {"family": "TRELLIS.2", "relative_paths": ["vae/trellis_2_shape_vae_bf16.safetensors", "vae/trellis_2_texture_vae_bf16.safetensors"], "note": "Only these VAE weights identified; full TRELLIS.2 generation pipeline not established by file presence"}],
        "external_paintpbr_2_1": {"models_root": str(external_root), "inside_comfyui_models": False, "files": external_weights,
                                   "node_class": "HunyuanPaint21LocalD", "node_source": r"D:\ComfyUI_windows_portable\ComfyUI\custom_nodes\ComfyUI-HunyuanPaint21-LocalD\__init__.py",
                                   "worker_source": r"D:\AI\Hunyuan3D-Paint-2.1\hunyuan_worker.py", "model_path_evidence_line": 42,
                                   "input_keys": ["model_3d", "reference", "steps", "view_size", "texture_size", "seed", "reuse_result"],
                                   "note": "Custom node launches isolated venv worker using hardcoded local models; requires GLB with existing UVs; no model-filename dropdown"},
        "inference_or_server_started": False
    },
    "toolchain": toolchain,
    "audit_limitations": ["Static working-tree audit; Android/Godot playback and ComfyUI inference were not launched", "Database schema documented from source, not production rows", "Backend/client compatibility mismatch remains untouched as required", "Initial installation wrapper logging used an unsupported Tee-Object parameter combination after launch; installer completed successfully and separate logs/build prove installation"]
}
save("existing_system_bridge.json", bridge)
paths = ["Root: " + raw["comfyui_models_root"], "", "MODEL WEIGHTS (22):"]
paths += [f"{m['relative_path']}\t{m['bytes']} bytes" for m in weights]
paths += ["", "EXTERNAL PAINTPBR 2.1 ROOT: " + str(external_root)]
paths += [f"{m['relative_path']}\t{m['bytes']} bytes" for m in external_weights]
paths += ["", "LoRA: no model weights; only loras/put_loras_here", "", "ALL FILES UNDER COMFYUI models/ (including configs/placeholders):"]
paths += [m["relative_path"] for m in model_files]
(OUT / "comfyui_model_paths.txt").write_text("\n".join(paths) + "\n", encoding="utf-8")
print(json.dumps({"bridge": str(OUT / "existing_system_bridge.json"), "database_tables": len(tables),
                  "assets": len(asset_files), "model_weights": len(weights), "external_weights": len(external_weights),
                  "integrity": integrity["status"], "backend_versions": bridge["data_contract"]["replay"]["versions"]}, indent=2))
