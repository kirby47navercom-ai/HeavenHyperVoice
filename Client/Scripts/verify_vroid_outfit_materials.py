from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
MANIFEST_PATH = PROJECT_ROOT / "Intermediate" / "VRoidOutfitCatalog" / "outfit_manifest.json"
OUTPUT_PATH = PROJECT_ROOT / "Saved" / "VRoidOutfitMaterialVerification.txt"
DESTINATION_ROOT = "/Game/CharacterCustomization/Assets/VRoid/Outfits"
TEMPLATE_MATERIAL_PATH = "/Game/CharacterCustomization/Materials/M_UEOutfitTextured.M_UEOutfitTextured"

DIFFUSE_PARAMS = ("DiffuseColorMap", "MainImage", "BaseColorMap", "BaseMap", "_MainTex")
OPACITY_PARAMS = ("OpacityMaskMap", "AlphaMap", "MaskMap", "_AlphaTex")
COLOR_PARAMS = ("DiffuseColor", "BaseColor", "Base Color", "Color", "TintColor")


def first_material_values(material_path: Path) -> dict:
    data = json.loads(material_path.read_text(encoding="utf-8"))
    return next(iter(data.values())) if data else {}


def parameter_name(value) -> str:
    info = value.get_editor_property("parameter_info")
    return str(info.get_editor_property("name"))


def vector_parameters(material: unreal.MaterialInstanceConstant) -> dict[str, unreal.LinearColor]:
    return {
        parameter_name(value): value.get_editor_property("parameter_value")
        for value in material.get_editor_property("vector_parameter_values")
    }


def scalar_parameters(material: unreal.MaterialInstanceConstant) -> dict[str, float]:
    return {
        parameter_name(value): float(value.get_editor_property("parameter_value"))
        for value in material.get_editor_property("scalar_parameter_values")
    }


def texture_parameters(material: unreal.MaterialInstanceConstant) -> dict[str, str]:
    result: dict[str, str] = {}
    for value in material.get_editor_property("texture_parameter_values"):
        texture = value.get_editor_property("parameter_value")
        result[parameter_name(value)] = texture.get_path_name() if texture else ""
    return result


def material_instances(destination: str) -> list[unreal.MaterialInstanceConstant]:
    materials: list[unreal.MaterialInstanceConstant] = []
    for asset_path in unreal.EditorAssetLibrary.list_assets(destination, recursive=False, include_folder=False):
        asset = unreal.load_asset(asset_path)
        if isinstance(asset, unreal.MaterialInstanceConstant):
            materials.append(asset)
    return materials


def colors_match(actual: unreal.LinearColor, expected: list[float], tolerance: float = 0.004) -> bool:
    values = [
        float(expected[0]) if len(expected) > 0 else 1.0,
        float(expected[1]) if len(expected) > 1 else 1.0,
        float(expected[2]) if len(expected) > 2 else 1.0,
        float(expected[3]) if len(expected) > 3 else 1.0,
    ]
    return (
        abs(float(actual.r) - values[0]) <= tolerance
        and abs(float(actual.g) - values[1]) <= tolerance
        and abs(float(actual.b) - values[2]) <= tolerance
        and abs(float(actual.a) - values[3]) <= tolerance
    )


def verify() -> int:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    failures: list[str] = []
    checked_items = 0
    checked_materials = 0
    source_textured_items = 0
    source_textured_materials = 0
    missing_main_items = 0

    for item in manifest.get("items", []):
        checked_items += 1
        values = first_material_values(Path(item["materials"]))
        main_name = values.get("main_texture", "")
        expected_color = values.get("color", [1.0, 1.0, 1.0, 1.0])
        destination = f"{DESTINATION_ROOT}/{item['gender']}/{item['category']}/Style_{item['style']}"
        materials = material_instances(destination)
        label = f"{item['gender']} {item['category']} {item['style']}"

        if main_name:
            source_textured_items += 1
        else:
            missing_main_items += 1

        if not materials:
            failures.append(f"{label}: no material instances in {destination}")
            continue

        for material in materials:
            checked_materials += 1
            parent = material.get_editor_property("parent")
            if not parent or parent.get_path_name() != TEMPLATE_MATERIAL_PATH:
                failures.append(
                    f"{label}: {material.get_name()} parent={parent.get_path_name() if parent else 'None'}"
                )

            vectors = vector_parameters(material)
            scalars = scalar_parameters(material)
            textures = texture_parameters(material)

            for color_param in COLOR_PARAMS:
                actual = vectors.get(color_param)
                if actual and not colors_match(actual, expected_color):
                    failures.append(f"{label}: {material.get_name()} {color_param}={actual} expected={expected_color}")

            diffuse_weight = scalars.get("DiffuseColorMapWeight")
            opacity_weight = scalars.get("OpacityMaskMapWeight")
            if main_name:
                source_textured_materials += 1
                expected_stem = Path(main_name).stem
                diffuse_texture_paths = [textures.get(param, "") for param in DIFFUSE_PARAMS]
                opacity_texture_paths = [textures.get(param, "") for param in OPACITY_PARAMS]
                if not any(expected_stem in texture_path for texture_path in diffuse_texture_paths):
                    failures.append(f"{label}: {material.get_name()} missing texture {main_name}")
                if not any(expected_stem in texture_path for texture_path in opacity_texture_paths):
                    failures.append(f"{label}: {material.get_name()} missing opacity texture {main_name}")
                if diffuse_weight is None or diffuse_weight < 0.99:
                    failures.append(f"{label}: {material.get_name()} DiffuseColorMapWeight={diffuse_weight}")
                if opacity_weight is None or opacity_weight < 0.99:
                    failures.append(f"{label}: {material.get_name()} OpacityMaskMapWeight={opacity_weight}")
            else:
                if diffuse_weight is None or abs(diffuse_weight) > 0.004:
                    failures.append(f"{label}: {material.get_name()} missing-source DiffuseColorMapWeight={diffuse_weight}")
                if opacity_weight is None or abs(opacity_weight) > 0.004:
                    failures.append(f"{label}: {material.get_name()} missing-source OpacityMaskMapWeight={opacity_weight}")

    lines = [
        "VRoid outfit material verification",
        f"items={checked_items}",
        f"materials={checked_materials}",
        f"source_textured_items={source_textured_items}",
        f"source_textured_materials={source_textured_materials}",
        f"missing_main_texture_items={missing_main_items}",
        f"failures={len(failures)}",
    ]
    if failures:
        lines.append("")
        lines.extend(failures[:200])
    OUTPUT_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")
    unreal.log_warning("VRoid outfit material verification: " + " ".join(lines[1:]))
    return len(failures)


if __name__ == "__main__":
    raise SystemExit(verify())
