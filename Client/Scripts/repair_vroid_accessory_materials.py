from __future__ import annotations

import unreal


ACCESSORY_ROOT = "/Game/CharacterCustomization/Assets/VRoid/Accessories"


def enable_two_sided(material: unreal.MaterialInstanceConstant) -> bool:
    overrides = material.get_editor_property("base_property_overrides")
    overrides.set_editor_property("override_two_sided", True)
    overrides.set_editor_property("two_sided", True)
    material.set_editor_property("base_property_overrides", overrides)
    material.modify()
    return unreal.EditorAssetLibrary.save_loaded_asset(
        material, only_if_is_dirty=False
    )


def main() -> None:
    materials = []
    for asset_path in unreal.EditorAssetLibrary.list_assets(
        ACCESSORY_ROOT, recursive=True, include_folder=False
    ):
        asset = unreal.load_asset(asset_path)
        if isinstance(asset, unreal.MaterialInstanceConstant):
            materials.append(asset)

    failed = []
    for material in materials:
        if not enable_two_sided(material):
            failed.append(material.get_path_name())
    if failed:
        raise RuntimeError(f"Failed to save accessory materials: {failed}")
    unreal.log_warning(
        f"ACCESSORY_MATERIAL_REPAIR_COMPLETE materials={len(materials)} two_sided=1"
    )


if __name__ == "__main__":
    main()
