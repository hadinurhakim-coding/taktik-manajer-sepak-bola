"""Create a verified, reversible archive before replacing the old character.

This script copies only. It never deletes source files or touches the legacy repo.
"""
import hashlib
import json
from pathlib import Path
import zipfile

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'04_Exported_GameAssets/Saved/Legacy_NicoPaz_20261015'
SOURCES=['04_Exported_GameAssets/characters_lod','04_Exported_GameAssets/animations_locomotion',
         '04_Exported_GameAssets/animations_onball','04_Exported_GameAssets/animations_goalkeeper',
         '04_Exported_GameAssets/animations_setpieces','04_Exported_GameAssets/kits_textures',
         '04_Exported_GameAssets/asset_manifest.json','05_UE5_MatchViewer_3D/Content/Football',
         '05_UE5_MatchViewer_3D/Content/Viewer/M_PlayerKit.uasset']

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    archive=OUT/'legacy_assets.zip'
    if archive.exists(): raise RuntimeError('Archive already exists; preserve it rather than overwrite.')
    records=[]
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=3) as z:
        for name in SOURCES:
            path=(ROOT/name).resolve()
            if not path.is_relative_to(ROOT): raise ValueError('Source escapes workspace')
            for file in sorted(path.rglob('*') if path.is_dir() else [path]):
                if not file.is_file(): continue
                data=file.read_bytes(); relative=file.relative_to(ROOT).as_posix()
                records.append({'path':relative,'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
                z.writestr(relative,data)
    with zipfile.ZipFile(archive) as z:
        for entry in records:
            if hashlib.sha256(z.read(entry['path'])).hexdigest()!=entry['sha256']: raise RuntimeError('Archive verification failed')
    report={'archive':str(archive),'verified':True,'operation':'copy_only','files':records}
    (ROOT/'00_Specs_and_Logs/character_rebuild_legacy_archive.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('VERIFIED_ARCHIVE',len(records),'files',archive)

if __name__=='__main__': main()
