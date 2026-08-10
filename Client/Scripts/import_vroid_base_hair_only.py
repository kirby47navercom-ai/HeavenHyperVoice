from pathlib import Path
import sys

import unreal


SCRIPT_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_ROOT))

from import_vroid_hair_catalog import (  # noqa: E402
    DESTINATION_ROOT,
    SKELETON_PATH,
    SOURCE_ROOT,
    import_skeletal,
    repair_masked_materials,
)


skeleton = unreal.load_asset(SKELETON_PATH)
if not isinstance(skeleton, unreal.Skeleton):
    raise RuntimeError(f"Missing common VRoid skeleton: {SKELETON_PATH}")

imported = 0
for gender in ("Female", "Male"):
    for style_root in sorted((SOURCE_ROOT / "BaseHair" / gender).glob("Style_*")):
        style_id = style_root.name.split("_", 1)[1]
        destination = f"{DESTINATION_ROOT}/BaseHairAligned/{gender}/Style_{style_id}"
        texture_tasks = []
        for source, asset_name in (
            (style_root / "HairBase.png", "HairBase"),
            (style_root / "HairBase_Normal.png", "HairBase_Normal"),
        ):
            task = unreal.AssetImportTask()
            task.filename = str(source)
            task.destination_path = destination
            task.destination_name = asset_name
            task.automated = True
            task.replace_existing = True
            task.save = True
            texture_tasks.append(task)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(texture_tasks)
        normal = unreal.load_asset(f"{destination}/HairBase_Normal")
        if isinstance(normal, unreal.Texture2D):
            normal.set_editor_property("srgb", False)
            normal.set_editor_property(
                "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP
            )
            unreal.EditorAssetLibrary.save_loaded_asset(normal, only_if_is_dirty=False)
        fbx = style_root / "FBX" / f"SK_HairBase_{gender}_{style_id}.fbx"
        if not fbx.exists():
            continue
        if import_skeletal(fbx, destination, skeleton, replace_existing=True) is None:
            raise RuntimeError(f"Failed to import {fbx}")
        repair_masked_materials(destination)
        imported += 1

unreal.EditorAssetLibrary.save_directory(
    f"{DESTINATION_ROOT}/BaseHairAligned"
)
unreal.log(f"VRoid Base Hair ready: {imported} gender-specific meshes")
