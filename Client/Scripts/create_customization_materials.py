from __future__ import annotations

import unreal


PACKAGE_PATH = "/Game/CharacterCustomization/Materials"


def _create_or_load_material(name: str) -> unreal.Material:
    path = f"{PACKAGE_PATH}/{name}"
    existing = unreal.load_asset(path)
    if existing:
        return existing

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFactoryNew()
    material = asset_tools.create_asset(name, PACKAGE_PATH, unreal.Material, factory)
    if material is None:
        raise RuntimeError(f"Could not create {path}")
    return material


def _prepare_skeletal_material(material: unreal.Material) -> None:
    material.set_editor_property("two_sided", True)
    material.set_editor_property("use_material_attributes", False)
    material.set_editor_property("used_with_skeletal_mesh", True)


def _make_face_accessory_material() -> None:
    material = _create_or_load_material("M_UEFaceAccessoryTranslucent")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    _prepare_skeletal_material(material)

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    diffuse = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -900, -220
    )
    diffuse.set_editor_property("parameter_name", "DiffuseColorMap")
    diffuse.set_editor_property(
        "texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")
    )

    opacity_map = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -900, 60
    )
    opacity_map.set_editor_property("parameter_name", "OpacityMaskMap")
    opacity_map.set_editor_property(
        "texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")
    )

    original_tint = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -570, -240
    )
    original_tint.set_editor_property("parameter_name", "OriginalTint")
    original_tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -570, -60
    )
    color.set_editor_property("parameter_name", "AccessoryColor")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    original_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -290, -240
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(diffuse, "RGB", original_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(original_tint, "", original_color, "B")

    base_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -80, -160
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(original_color, "", base_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(color, "", base_color, "B")

    opacity = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -570, 160
    )
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.42)

    final_opacity = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -290, 110
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(opacity_map, "A", final_opacity, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(opacity, "", final_opacity, "B")

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -570, 340
    )
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.84)

    specular = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -570, 500
    )
    specular.set_editor_property("parameter_name", "Specular")
    specular.set_editor_property("default_value", 0.08)

    unreal.MaterialEditingLibrary.connect_material_property(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(base_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(final_opacity, "", unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)


def _make_eye_white_material() -> None:
    material = _create_or_load_material("M_UEEyeWhiteSolid")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("opacity_mask_clip_value", 0.03)
    _prepare_skeletal_material(material)

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    diffuse = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -760, -180
    )
    diffuse.set_editor_property("parameter_name", "DiffuseColorMap")
    diffuse.set_editor_property(
        "texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")
    )

    tint = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -760, 80
    )
    tint.set_editor_property("parameter_name", "EyeWhiteTint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    original_tint = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -760, -360
    )
    original_tint.set_editor_property("parameter_name", "OriginalTint")
    original_tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    original_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -420, -260
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(diffuse, "RGB", original_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(original_tint, "", original_color, "B")

    base_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -180, -80
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(original_color, "", base_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tint, "", base_color, "B")

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -760, 260
    )
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.9)

    specular = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -760, 420
    )
    specular.set_editor_property("parameter_name", "Specular")
    specular.set_editor_property("default_value", 0.04)

    unreal.MaterialEditingLibrary.connect_material_property(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(base_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(diffuse, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)


def _make_iris_material() -> None:
    material = _create_or_load_material("M_UEIrisTintMasked")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("opacity_mask_clip_value", 0.03)
    _prepare_skeletal_material(material)

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    texture = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -700, -120
    )
    texture.set_editor_property("parameter_name", "DiffuseColorMap")

    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -700, 140
    )
    color.set_editor_property("parameter_name", "IrisColor")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    original_tint = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -700, -360
    )
    original_tint.set_editor_property("parameter_name", "OriginalTint")
    original_tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    original_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -520, -240
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(texture, "RGB", original_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(original_tint, "", original_color, "B")

    multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -330, -20
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(original_color, "", multiply, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(color, "", multiply, "B")

    iris_fill = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -330, 140
    )
    iris_fill.set_editor_property("parameter_name", "IrisFill")
    iris_fill.set_editor_property("default_value", 0.0)

    fill = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -110, 60
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(color, "", fill, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(iris_fill, "", fill, "B")

    pupil_threshold = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -330, 600
    )
    pupil_threshold.set_editor_property("r", 0.08)

    iris_brightness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSubtract, -110, 310
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(texture, "R", iris_brightness, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(pupil_threshold, "", iris_brightness, "B")

    fill_scale = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -110, 500
    )
    fill_scale.set_editor_property("r", 4.0)

    fill_mask_raw = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 90, 310
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(iris_brightness, "", fill_mask_raw, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(fill_scale, "", fill_mask_raw, "B")

    fill_mask = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSaturate, 300, 310
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(fill_mask_raw, "", fill_mask, "")

    masked_fill = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 300, 60
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(fill, "", masked_fill, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(fill_mask, "", masked_fill, "B")

    base_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, 520, -20
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(multiply, "", base_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(masked_fill, "", base_color, "B")

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -330, 260
    )
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.72)

    specular = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -330, 420
    )
    specular.set_editor_property("parameter_name", "Specular")
    specular.set_editor_property("default_value", 0.08)

    unreal.MaterialEditingLibrary.connect_material_property(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(base_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(texture, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)


def _make_outfit_material() -> None:
    material = _create_or_load_material("M_UEOutfitTextured")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("opacity_mask_clip_value", 0.03)
    _prepare_skeletal_material(material)

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    diffuse = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -900, -240
    )
    diffuse.set_editor_property("parameter_name", "DiffuseColorMap")
    diffuse.set_editor_property(
        "texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")
    )

    opacity_map = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -900, 40
    )
    opacity_map.set_editor_property("parameter_name", "OpacityMaskMap")
    opacity_map.set_editor_property(
        "texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")
    )

    normal = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -900, 320
    )
    normal.set_editor_property("parameter_name", "NormalMap")
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    normal.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"))

    tint = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -570, -120
    )
    tint.set_editor_property("parameter_name", "CustomTint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    original_tint = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -570, -360
    )
    original_tint.set_editor_property("parameter_name", "OriginalTint")
    original_tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    original_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -300, -300
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(diffuse, "RGB", original_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(original_tint, "", original_color, "B")

    base_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -80, -200
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(original_color, "", base_color, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tint, "", base_color, "B")

    emission_weight = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -300, -20
    )
    emission_weight.set_editor_property("parameter_name", "TextureEmissionWeight")
    emission_weight.set_editor_property("default_value", 0.08)

    emission = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -80, -100
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(base_color, "", emission, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(emission_weight, "", emission, "B")

    opacity = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -570, 180
    )
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 1.0)

    opacity_mask = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -300, 100
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(opacity_map, "A", opacity_mask, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(opacity, "", opacity_mask, "B")

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -570, 520
    )
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.78)

    specular = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -570, 680
    )
    specular.set_editor_property("parameter_name", "Specular")
    specular.set_editor_property("default_value", 0.05)

    unreal.MaterialEditingLibrary.connect_material_property(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(emission, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(opacity_mask, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    unreal.MaterialEditingLibrary.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)


def main() -> None:
    _make_face_accessory_material()
    _make_eye_white_material()
    _make_iris_material()
    _make_outfit_material()


if __name__ == "__main__":
    main()
