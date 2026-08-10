from pathlib import Path

import unreal


OUTPUT = Path(unreal.Paths.project_saved_dir()) / "Screenshots" / "Customization" / "TextureQA"
OUTPUT.mkdir(parents=True, exist_ok=True)

ASSETS = {
    "HairFront_Male_2388.png": "/Game/VRoidCatalog/HairDetermined/Male/Style_2388/Hair_481f10_F00_000_Hair_00_87",
    "HairBase_Male_0.png": "/Game/VRoidCatalog/HairDetermined/BaseHairAligned/Male/Style_0/HairBase",
}

for filename, path in ASSETS.items():
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.Texture2D):
        raise RuntimeError(f"Missing texture: {path}")
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(OUTPUT / filename)
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError(f"Texture export failed: {path}")
    unreal.log(f"TEXTURE_QA exported={path} file={task.filename}")
