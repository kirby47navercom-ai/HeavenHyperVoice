from __future__ import annotations

import unreal


PACKAGE_PATH = "/Game/CharacterCustomization/Materials"
MATERIAL_NAME = "M_UEFaceAccessoryTranslucent"
MATERIAL_PATH = f"{PACKAGE_PATH}/{MATERIAL_NAME}"


def _create_or_load_material() -> unreal.Material:
    existing = unreal.load_asset(MATERIAL_PATH)
    if existing:
        return existing

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFactoryNew()
    material = asset_tools.create_asset(MATERIAL_NAME, PACKAGE_PATH, unreal.Material, factory)
    if material is None:
        raise RuntimeError(f"Could not create {MATERIAL_PATH}")
    return material


def main() -> None:
    material = _create_or_load_material()
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("use_material_attributes", False)

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -500, -140
    )
    color.set_editor_property("parameter_name", "AccessoryColor")
    color.set_editor_property("default_value", unreal.LinearColor(0.12, 0.16, 0.13, 1.0))

    opacity = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -500, 80
    )
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.12)

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -500, 250
    )
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.84)

    specular = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -500, 420
    )
    specular.set_editor_property("parameter_name", "Specular")
    specular.set_editor_property("default_value", 0.08)

    unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)


if __name__ == "__main__":
    main()
