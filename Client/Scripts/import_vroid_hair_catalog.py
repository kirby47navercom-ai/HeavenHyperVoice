from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "Intermediate" / "VRoidHairCatalog"
DESTINATION_ROOT = "/Game/VRoidCatalog/HairDetermined"
SKELETON_PATH = "/Game/VRoidGenerated/SK_BodySkin_Skeleton"
SEMANTICS = ("HairFront", "HairSide", "HairBack", "HairExtra")


def import_skeletal(
    path: Path,
    destination: str,
    skeleton: unreal.Skeleton,
    replace_existing: bool = False,
):
    asset_name = path.stem
    object_path = f"{destination}/{asset_name}.{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(object_path) and not replace_existing:
        return unreal.load_asset(object_path)
    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = destination
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = replace_existing
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
    return unreal.load_asset(object_path)


def repair_masked_materials(destination: str) -> None:
    mask_parent = unreal.load_asset(
        "/InterchangeAssets/Materials/PhongSurfaceMaskMaterial"
    )
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for asset_data in registry.get_assets_by_path(destination, recursive=True):
        material = asset_data.get_asset()
        if not isinstance(material, unreal.MaterialInstanceConstant):
            continue
        diffuse = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(
            material, "DiffuseColorMap"
        )
        if diffuse is None:
            continue
        material.set_editor_property("parent", mask_parent)
        overrides = material.get_editor_property("base_property_overrides")
        overrides.set_editor_property("override_two_sided", True)
        overrides.set_editor_property("two_sided", True)
        material.set_editor_property("base_property_overrides", overrides)
        applied_overrides = material.get_editor_property("base_property_overrides")
        if not (
            applied_overrides.get_editor_property("override_two_sided")
            and applied_overrides.get_editor_property("two_sided")
        ):
            raise RuntimeError(f"Two-sided hair material repair failed: {material.get_path_name()}")
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
            material, "OpacityMaskMap", diffuse
        )
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            material, "OpacityMask", 1.0
        )
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            material, "OpacityMaskMapWeight", 1.0
        )
        unreal.EditorAssetLibrary.save_loaded_asset(material)


def run() -> None:
    skeleton = unreal.load_asset(SKELETON_PATH)
    if not isinstance(skeleton, unreal.Skeleton):
        raise RuntimeError(f"Missing common VRoid skeleton: {SKELETON_PATH}")
    imported = 0
    for gender in ("Male", "Female"):
        for style_root in sorted((SOURCE_ROOT / gender).glob("Style_*")):
            mesh_id = style_root.name.split("_", 1)[1]
            destination = f"{DESTINATION_ROOT}/{gender}/Style_{mesh_id}"
            for semantic in SEMANTICS:
                fbx = style_root / "FBX" / f"SK_{semantic}_{gender}_{mesh_id}.fbx"
                if not fbx.exists():
                    continue
                if import_skeletal(fbx, destination, skeleton, replace_existing=True) is None:
                    raise RuntimeError(f"Failed to import {fbx}")
                imported += 1
            repair_masked_materials(destination)
    for gender in ("Female", "Male"):
        for style_root in sorted((SOURCE_ROOT / "BaseHair" / gender).glob("Style_*")):
            style_id = style_root.name.split("_", 1)[1]
            destination = f"{DESTINATION_ROOT}/BaseHairAligned/{gender}/Style_{style_id}"
            fbx = style_root / "FBX" / f"SK_HairBase_{gender}_{style_id}.fbx"
            if not fbx.exists():
                continue
            if import_skeletal(fbx, destination, skeleton, replace_existing=True) is None:
                raise RuntimeError(f"Failed to import {fbx}")
            imported += 1
            repair_masked_materials(destination)
    unreal.EditorAssetLibrary.save_directory(DESTINATION_ROOT)
    unreal.log(f"VRoid hair catalog ready: {imported} modular meshes")


if __name__ == "__main__":
    run()
