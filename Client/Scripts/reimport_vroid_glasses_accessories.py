from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = PROJECT_ROOT / "Intermediate" / "VRoidAccessoryCatalog" / "accessory_manifest.json"
DESTINATION_ROOT = "/Game/CharacterCustomization/Assets/VRoid/Accessories"
CATALOG_PATH = "/Game/CharacterCustomization/Blueprints/DA_CustomizationCatalog"
SKELETON_PATH = "/Game/VRoidGenerated/SK_BodySkin_Skeleton"
LABELS = ("GlassesHi", "GlassesLow")


def import_skeletal(path: Path, destination: str, skeleton: unreal.Skeleton) -> unreal.SkeletalMesh:
    asset_name = path.stem
    if unreal.EditorAssetLibrary.does_directory_exist(destination):
        unreal.EditorAssetLibrary.delete_directory(destination)

    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = destination
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
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

    object_path = f"{destination}/{asset_name}.{asset_name}"
    mesh = unreal.load_asset(object_path)
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError(f"Failed to import {path}")
    return mesh


def enable_two_sided(material: unreal.MaterialInstanceConstant) -> None:
    overrides = material.get_editor_property("base_property_overrides")
    overrides.set_editor_property("override_two_sided", True)
    overrides.set_editor_property("two_sided", True)
    material.set_editor_property("base_property_overrides", overrides)
    material.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)


def find_frame_material() -> unreal.MaterialInterface | None:
    for label in LABELS:
        path = (
            f"{DESTINATION_ROOT}/FaceAccessory/SK_FaceAccessory_{label}/"
            "M_FaceAccessory.M_FaceAccessory"
        )
        material = unreal.load_asset(path)
        if isinstance(material, unreal.MaterialInterface):
            return material
    return None


def main() -> None:
    skeleton = unreal.load_asset(SKELETON_PATH)
    catalog = unreal.load_asset(CATALOG_PATH)
    if not isinstance(skeleton, unreal.Skeleton) or catalog is None:
        raise RuntimeError("Customization skeleton or DataAsset is missing")

    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    items = [item for item in manifest["items"] if item["label"] in LABELS]
    if len(items) != len(LABELS):
        raise RuntimeError(f"Expected {len(LABELS)} glasses entries, got {len(items)}")

    imported_by_label: dict[str, unreal.SkeletalMesh] = {}
    for item in items:
        target = Path(item["output"])
        destination = f"{DESTINATION_ROOT}/{item['category']}/{target.stem}"
        mesh = import_skeletal(target, destination, skeleton)
        unreal.EditorAssetLibrary.set_metadata_tag(mesh, "VRoidCategory", item["category"])
        unreal.EditorAssetLibrary.set_metadata_tag(mesh, "VRoidLabel", item["label"])
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
        imported_by_label[item["label"]] = mesh

        for asset_path in unreal.EditorAssetLibrary.list_assets(destination, recursive=True, include_folder=False):
            asset = unreal.load_asset(asset_path)
            if isinstance(asset, unreal.MaterialInstanceConstant):
                enable_two_sided(asset)

        unreal.log_warning(f"VROID_GLASSES_REIMPORT {item['label']} {mesh.get_path_name()}")

    frame_material = find_frame_material()
    if frame_material is None:
        raise RuntimeError("Imported glasses frame material was not created")
    for mesh in imported_by_label.values():
        materials = list(mesh.get_editor_property("materials"))
        if not materials:
            raise RuntimeError(f"Imported glasses mesh has no material slots: {mesh.get_path_name()}")
        materials[0].set_editor_property("material_interface", frame_material)
        mesh.set_editor_property("materials", materials)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)

    face_catalog = [None] + [imported_by_label[label] for label in LABELS]
    catalog.set_editor_property("MaleFaceAccessoryCatalog", face_catalog)
    catalog.set_editor_property("FemaleFaceAccessoryCatalog", face_catalog)
    catalog.set_editor_property("FaceAccessoryForwardOffset", 6.5)
    catalog.set_editor_property("FaceAccessoryVerticalOffset", 17.5)
    catalog.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(catalog, only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_directory(f"{DESTINATION_ROOT}/FaceAccessory")
    unreal.log_warning(f"VROID_GLASSES_REIMPORT_COMPLETE items={len(items)}")


if __name__ == "__main__":
    main()
