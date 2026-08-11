from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = PROJECT_ROOT / "Intermediate" / "VRoidAccessoryCatalog" / "accessory_manifest.json"
DESTINATION_ROOT = "/Game/CharacterCustomization/Assets/VRoid/Accessories"
CATALOG_PATH = "/Game/CharacterCustomization/Blueprints/DA_CustomizationCatalog"
SKELETON_PATH = "/Game/CharacterCustomization/Assets/VRoid/Skeletons/SK_VRoidCommon"
CATEGORIES = (
    "HeadAccessory",
    "FaceAccessory",
    "EarAccessory",
    "TailAccessory",
    "NeckAccessory",
)


def import_skeletal(path: Path, destination: str, skeleton: unreal.Skeleton):
    asset_name = path.stem
    object_path = f"{destination}/{asset_name}.{asset_name}"
    existing = unreal.load_asset(object_path)
    if isinstance(existing, unreal.SkeletalMesh):
        return existing
    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = destination
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = False
    task.save = True
    options = unreal.FbxImportUI()
    options.import_as_skeletal = True
    options.import_mesh = True
    options.import_materials = True
    options.import_textures = False
    options.import_animations = False
    options.skeleton = skeleton
    options.skeletal_mesh_import_data.normal_import_method = (
        unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    )
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = unreal.load_asset(object_path)
    if not isinstance(imported, unreal.SkeletalMesh):
        raise RuntimeError(f"Failed to import {path}")
    return imported


def main() -> None:
    skeleton = unreal.load_asset(SKELETON_PATH)
    catalog = unreal.load_asset(CATALOG_PATH)
    if not isinstance(skeleton, unreal.Skeleton) or catalog is None:
        raise RuntimeError("Customization skeleton or DataAsset is missing")
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    catalogs = {category: [None] for category in CATEGORIES}
    for number, item in enumerate(manifest["items"], start=1):
        target = Path(item["output"])
        destination = f"{DESTINATION_ROOT}/{item['category']}/{target.stem}"
        mesh = import_skeletal(target, destination, skeleton)
        unreal.EditorAssetLibrary.set_metadata_tag(mesh, "VRoidCategory", item["category"])
        unreal.EditorAssetLibrary.set_metadata_tag(mesh, "VRoidLabel", item["label"])
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
        catalogs[item["category"]].append(mesh)
        unreal.log_warning(f"ACCESSORY_IMPORT {number}/{len(manifest['items'])} {item['label']}")

    for gender in ("Male", "Female"):
        for category, values in catalogs.items():
            catalog.set_editor_property(f"{gender}{category}Catalog", values)
    catalog.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(catalog, only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_directory(DESTINATION_ROOT)
    unreal.log_warning(f"ACCESSORY_IMPORT_COMPLETE items={len(manifest['items'])}")


if __name__ == "__main__":
    main()
