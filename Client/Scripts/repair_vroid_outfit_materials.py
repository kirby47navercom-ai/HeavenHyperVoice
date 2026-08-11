from __future__ import annotations

import json
import re
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = PROJECT_ROOT / "Intermediate" / "VRoidOutfitCatalog" / "outfit_manifest.json"
DESTINATION_ROOT = "/Game/CharacterCustomization/Assets/VRoid/Outfits"
TEMPLATE_MATERIAL_PATH = "/Game/CharacterCustomization/Materials/M_UEOutfitTextured.M_UEOutfitTextured"

DIFFUSE_PARAMS = ("DiffuseColorMap", "MainImage", "BaseColorMap", "BaseMap", "_MainTex")
OPACITY_PARAMS = ("OpacityMaskMap", "AlphaMap", "MaskMap", "_AlphaTex")
NORMAL_PARAMS = ("NormalMap", "NormalMapImage", "BumpMap", "_BumpMap")
COLOR_PARAMS = ("DiffuseColor", "BaseColor", "Base Color", "Color", "TintColor")


def sanitize_asset_name(name: str) -> str:
    value = re.sub(r"[^0-9A-Za-z_]+", "_", name)
    if not value or value[0].isdigit():
        value = f"T_{value}"
    return value


def first_material_values(material_path: Path) -> dict:
    data = json.loads(material_path.read_text(encoding="utf-8"))
    return next(iter(data.values())) if data else {}


def import_texture(source: Path, destination: str, is_normal: bool = False) -> unreal.Texture | None:
    if not source.exists():
        return None

    asset_name = sanitize_asset_name(source.stem)
    object_path = f"{destination}/{asset_name}.{asset_name}"
    texture = unreal.load_asset(object_path)
    if texture is None:
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = destination
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = True
        task.save = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(object_path)

    if isinstance(texture, unreal.Texture):
        texture.set_editor_property("srgb", not is_normal)
        if is_normal:
            texture.set_editor_property(
                "compression_settings",
                unreal.TextureCompressionSettings.TC_NORMALMAP,
            )
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
        return texture
    return None


def set_texture(material: unreal.MaterialInstanceConstant, parameter_names: tuple[str, ...], texture: unreal.Texture) -> None:
    for parameter_name in parameter_names:
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
            material,
            parameter_name,
            texture,
        )


def set_scalar(material: unreal.MaterialInstanceConstant, parameter_name: str, value: float) -> None:
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        material,
        parameter_name,
        value,
    )


def set_color(material: unreal.MaterialInstanceConstant, parameter_names: tuple[str, ...], values: list[float]) -> None:
    color = unreal.LinearColor(
        float(values[0]) if len(values) > 0 else 1.0,
        float(values[1]) if len(values) > 1 else 1.0,
        float(values[2]) if len(values) > 2 else 1.0,
        float(values[3]) if len(values) > 3 else 1.0,
    )
    for parameter_name in parameter_names:
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            material,
            parameter_name,
            color,
        )


def material_instances(destination: str) -> list[unreal.MaterialInstanceConstant]:
    materials: list[unreal.MaterialInstanceConstant] = []
    for asset_path in unreal.EditorAssetLibrary.list_assets(destination, recursive=False, include_folder=False):
        asset = unreal.load_asset(asset_path)
        if isinstance(asset, unreal.MaterialInstanceConstant):
            materials.append(asset)
    return materials


def repair_item(item: dict) -> tuple[int, int]:
    material_path = Path(item["materials"])
    values = first_material_values(material_path)
    destination = f"{DESTINATION_ROOT}/{item['gender']}/{item['category']}/Style_{item['style']}"
    source_root = material_path.parent
    main_name = values.get("main_texture", "")
    normal_name = values.get("normal_texture", "")

    main_texture = import_texture(source_root / main_name, destination, False) if main_name else None
    normal_texture = import_texture(source_root / normal_name, destination, True) if normal_name else None
    template_material = unreal.load_asset(TEMPLATE_MATERIAL_PATH)

    repaired = 0
    missing = 0
    for material in material_instances(destination):
        if template_material:
            material.set_editor_property("parent", template_material)
        set_color(material, COLOR_PARAMS, values.get("color", [1.0, 1.0, 1.0, 1.0]))
        if main_texture:
            set_texture(material, DIFFUSE_PARAMS, main_texture)
            set_texture(material, OPACITY_PARAMS, main_texture)
            set_scalar(material, "DiffuseColorMapWeight", 1.0)
            set_scalar(material, "OpacityMaskMapWeight", 1.0)
            unreal.EditorAssetLibrary.set_metadata_tag(material, "VRoidMainTexture", main_name)
            unreal.EditorAssetLibrary.remove_metadata_tag(material, "VRoidMissingMainTexture")
        else:
            set_scalar(material, "DiffuseColorMapWeight", 0.0)
            set_scalar(material, "OpacityMaskMapWeight", 0.0)
            unreal.EditorAssetLibrary.set_metadata_tag(material, "VRoidMissingMainTexture", "true")
            missing += 1
        if normal_texture:
            set_texture(material, NORMAL_PARAMS, normal_texture)
            set_scalar(material, "NormalMapWeight", float(values.get("normal_strength", 1.0)))
            unreal.EditorAssetLibrary.set_metadata_tag(material, "VRoidNormalTexture", normal_name)
        else:
            set_scalar(material, "NormalMapWeight", 0.0)
            unreal.EditorAssetLibrary.remove_metadata_tag(material, "VRoidNormalTexture")
        unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
        repaired += 1

    return repaired, missing


def main() -> None:
    if not MANIFEST_PATH.exists():
        raise FileNotFoundError(MANIFEST_PATH)
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    total_repaired = 0
    total_missing = 0
    for index, item in enumerate(manifest.get("items", []), start=1):
        repaired, missing = repair_item(item)
        total_repaired += repaired
        total_missing += missing
        unreal.log_warning(
            f"OUTFIT_MATERIAL_REPAIR {index}/{len(manifest.get('items', []))} "
            f"{item['gender']} {item['category']} {item['style']} repaired={repaired} missing_main={missing}"
        )

    unreal.EditorAssetLibrary.save_directory(DESTINATION_ROOT, only_if_is_dirty=False, recursive=True)
    unreal.log_warning(
        f"OUTFIT_MATERIAL_REPAIR_COMPLETE repaired={total_repaired} missing_main={total_missing}"
    )


if __name__ == "__main__":
    main()
