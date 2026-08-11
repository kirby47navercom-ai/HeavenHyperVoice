from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "Intermediate" / "VRoidOutfitCatalog"
MANIFEST_PATH = SOURCE_ROOT / "outfit_manifest.json"
DESTINATION_ROOT = "/Game/CharacterCustomization/Assets/VRoid/Outfits"
CATALOG_PATH = "/Game/CharacterCustomization/Blueprints/DA_CustomizationCatalog"
SKELETON_PATH = "/Game/CharacterCustomization/Assets/VRoid/Skeletons/SK_VRoidCommon"
PROPERTY_NAMES = {
    ("Male", "Tops"): "MaleTopCatalog",
    ("Male", "Bottoms"): "MaleBottomCatalog",
    ("Male", "Onepiece"): "MaleOnepieceCatalog",
    ("Male", "Shoes"): "MaleShoesCatalog",
    ("Female", "Tops"): "FemaleTopCatalog",
    ("Female", "Bottoms"): "FemaleBottomCatalog",
    ("Female", "Onepiece"): "FemaleOnepieceCatalog",
    ("Female", "Shoes"): "FemaleShoesCatalog",
}


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
    options.import_textures = True
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
    if not MANIFEST_PATH.exists():
        raise FileNotFoundError(MANIFEST_PATH)
    skeleton = unreal.load_asset(SKELETON_PATH)
    catalog = unreal.load_asset(CATALOG_PATH)
    if not isinstance(skeleton, unreal.Skeleton):
        raise RuntimeError(f"Missing skeleton: {SKELETON_PATH}")
    if catalog is None:
        raise RuntimeError(f"Missing catalog: {CATALOG_PATH}")
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    catalogs = {key: [None] for key in PROPERTY_NAMES}

    for number, item in enumerate(manifest.get("items", []), start=1):
        key = (item["gender"], item["category"])
        target = Path(item["output"])
        if not target.exists():
            raise FileNotFoundError(target)
        destination = (
            f"{DESTINATION_ROOT}/{item['gender']}/{item['category']}/"
            f"Style_{item['style']}"
        )
        mesh = import_skeletal(target, destination, skeleton)
        unreal.EditorAssetLibrary.set_metadata_tag(mesh, "VRoidCategory", item["category"])
        unreal.EditorAssetLibrary.set_metadata_tag(mesh, "VRoidStyleId", item["style"])
        unreal.EditorAssetLibrary.set_metadata_tag(mesh, "VRoidLabel", item["label"])
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
        catalogs[key].append(mesh)
        unreal.log_warning(
            f"OUTFIT_IMPORT {number}/{len(manifest['items'])} "
            f"{item['gender']} {item['category']} {item['style']}"
        )

    for key, property_name in PROPERTY_NAMES.items():
        catalog.set_editor_property(property_name, catalogs[key])
    catalog.modify()
    if not unreal.EditorAssetLibrary.save_loaded_asset(catalog, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save catalog: {CATALOG_PATH}")
    unreal.EditorAssetLibrary.save_directory(DESTINATION_ROOT)
    unreal.log_warning(
        f"OUTFIT_IMPORT_COMPLETE items={len(manifest.get('items', []))}"
    )


if __name__ == "__main__":
    main()
