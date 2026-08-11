from __future__ import annotations

import unreal


OUTPUT_PATH = unreal.Paths.project_saved_dir() + "/CustomizationMaterialParams.txt"
LINES = []


MATERIALS = (
    "/Game/VRoidCatalog/FaceGeometryDetermined/Skin",
    "/Game/VRoidCatalog/FaceGeometryDetermined/Brow",
    "/Game/VRoidCatalog/FaceGeometryDetermined/EyeIris",
    "/Game/VRoidCatalog/FaceGeometryDetermined/EyeWhite",
    "/Game/VRoidCatalog/FaceGeometryDetermined/Mouth",
    "/Game/VRoidGenerated/Female/Skin",
    "/Game/VRoidGenerated/Skin",
    "/Game/CharacterCustomization/Assets/VRoid/Accessories/FaceAccessory/SK_FaceAccessory_GlassesLow/M_FaceAccessory.M_FaceAccessory",
    "/Game/CharacterCustomization/Assets/VRoid/Accessories/HeadAccessory/SK_HeadAccessory_WitchHat/M_HeadAccessory.M_HeadAccessory",
    "/Game/CharacterCustomization/Assets/VRoid/Accessories/EarAccessory/SK_EarAccessory_CatEar/M_EarAccessory.M_EarAccessory",
    "/Game/CharacterCustomization/Assets/VRoid/Outfits/Female/Onepiece/Style_N00_005/M_Onepiece_Female_N00_005.M_Onepiece_Female_N00_005",
    "/Game/CharacterCustomization/Assets/VRoid/Outfits/Male/Tops/Style_N00_157/M_Top_Male_N00_157.M_Top_Male_N00_157",
    "/Game/CharacterCustomization/Materials/M_UEFaceAccessoryTranslucent.M_UEFaceAccessoryTranslucent",
    "/InterchangeAssets/Materials/FBXLegacyPhongSurfaceMaterial",
)


def _param_name(value) -> str:
    info = value.get_editor_property("parameter_info")
    return str(info.get_editor_property("name"))


for path in MATERIALS:
    material = unreal.load_asset(path)
    if material is None:
        LINES.append(f"MATERIAL_PARAM missing={path}")
        continue
    parent = None
    vectors = []
    scalars = []
    textures = []
    if isinstance(material, unreal.MaterialInstanceConstant):
        parent = material.get_editor_property("parent")
        vectors = [_param_name(value) for value in material.get_editor_property("vector_parameter_values")]
        scalars = [_param_name(value) for value in material.get_editor_property("scalar_parameter_values")]
        textures = [_param_name(value) for value in material.get_editor_property("texture_parameter_values")]
    try:
        blend_mode = material.get_editor_property("blend_mode")
    except Exception:
        blend_mode = "inherited"
    LINES.append(
        "MATERIAL_PARAM "
        f"path={path} class={material.get_class().get_name()} "
        f"blend={blend_mode} "
        f"parent={parent.get_path_name() if parent else 'None'} "
        f"vectors={vectors} scalars={scalars} textures={textures}"
    )

for prefix in (
    "/Game/CharacterCustomization/Assets/VRoid/Accessories/FaceAccessory",
    "/Game/CharacterCustomization/Assets/VRoid/Accessories/HeadAccessory",
):
    LINES.append(f"MATERIAL_PARAM list_assets {prefix}")
    for asset_path in unreal.EditorAssetLibrary.list_assets(prefix, recursive=True, include_folder=False):
        LINES.append(f"MATERIAL_PARAM asset={asset_path}")

for mesh_path in (
    "/Game/CharacterCustomization/Assets/VRoid/Accessories/FaceAccessory/SK_FaceAccessory_GlassesHi/SK_FaceAccessory_GlassesHi.SK_FaceAccessory_GlassesHi",
    "/Game/CharacterCustomization/Assets/VRoid/Accessories/FaceAccessory/SK_FaceAccessory_GlassesLow/SK_FaceAccessory_GlassesLow.SK_FaceAccessory_GlassesLow",
):
    mesh = unreal.load_asset(mesh_path)
    LINES.append(f"MATERIAL_PARAM mesh={mesh_path} class={mesh.get_class().get_name() if mesh else 'None'}")
    if not mesh:
        continue
    for index, material in enumerate(mesh.get_editor_property("materials")):
        slot_name = material.get_editor_property("material_slot_name")
        material_interface = material.get_editor_property("material_interface")
        LINES.append(
            "MATERIAL_PARAM mesh_slot "
            f"mesh={mesh_path} index={index} slot={slot_name} "
            f"material={material_interface.get_path_name() if material_interface else 'None'}"
        )

with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
    output.write("\n".join(LINES))
