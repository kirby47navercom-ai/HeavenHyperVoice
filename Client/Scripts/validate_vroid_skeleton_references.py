from __future__ import annotations

import unreal


CATALOG_PATH = "/Game/CharacterCustomization/Blueprints/DA_CustomizationCatalog"
SKELETON_PATH = "/Game/CharacterCustomization/Assets/VRoid/Skeletons/SK_VRoidCommon"
MESH_ROOTS = (
    "/Game/CharacterCustomization/Assets/VRoid",
    "/Game/VRoidCatalog",
    "/Game/VRoidGenerated",
)


def main() -> None:
    expected = unreal.load_asset(SKELETON_PATH)
    if not isinstance(expected, unreal.Skeleton):
        raise RuntimeError(f"Missing common VRoid skeleton: {SKELETON_PATH}")

    catalog = unreal.load_asset(CATALOG_PATH)
    if catalog is None:
        raise RuntimeError(f"Missing catalog: {CATALOG_PATH}")
    catalog_skeleton = catalog.get_editor_property("CommonSkeleton")
    if catalog_skeleton != expected:
        raise RuntimeError("DA_CustomizationCatalog.CommonSkeleton is not SK_VRoidCommon")

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    checked = 0
    mismatches = []
    for root in MESH_ROOTS:
        for asset_data in registry.get_assets_by_path(root, recursive=True):
            class_name = str(asset_data.asset_class_path.asset_name)
            if class_name != "SkeletalMesh":
                continue
            mesh = asset_data.get_asset()
            if not isinstance(mesh, unreal.SkeletalMesh):
                continue
            checked += 1
            skeleton = mesh.get_editor_property("skeleton")
            if skeleton != expected:
                mismatches.append(asset_data.object_path)

    if mismatches:
        for path in mismatches:
            unreal.log_error(f"VRoid mesh uses a different skeleton: {path}")
        raise RuntimeError(f"{len(mismatches)} skeletal mesh reference mismatch(es)")

    unreal.log(f"VRoid skeleton validation passed: {checked} skeletal meshes use {SKELETON_PATH}")


if __name__ == "__main__":
    main()
