from __future__ import annotations

import unreal


CATALOG_PATH = "/Game/CharacterCustomization/Blueprints/DA_CustomizationCatalog"
PROPERTIES = (
    "MaleBodyCatalog",
    "FemaleBodyCatalog",
    "MaleFaceSkinCatalog",
    "FemaleFaceSkinCatalog",
    "MaleEyeWhiteCatalog",
    "FemaleEyeWhiteCatalog",
    "MaleEyeIrisCatalog",
    "FemaleEyeIrisCatalog",
    "MaleEyeHighlightCatalog",
    "FemaleEyeHighlightCatalog",
    "MaleEyeExtraCatalog",
    "FemaleEyeExtraCatalog",
    "MaleBrowCatalog",
    "FemaleBrowCatalog",
    "MaleEyelashCatalog",
    "FemaleEyelashCatalog",
    "MaleEyelineCatalog",
    "FemaleEyelineCatalog",
    "MaleMouthCatalog",
    "FemaleMouthCatalog",
    "FaceSkinTextureCatalog",
    "FaceSkinNormalTextureCatalog",
    "EyeWhiteTextureCatalog",
    "EyeIrisTextureCatalog",
    "EyeHighlightTextureCatalog",
    "BrowTextureCatalog",
    "EyelashTextureCatalog",
    "EyelineTextureCatalog",
    "MouthTextureCatalog",
    "LipTextureCatalog",
    "MouthLineTextureCatalog",
    "MaleHairFrontCatalog",
    "MaleHairSideCatalog",
    "MaleHairBackCatalog",
    "MaleHairExtraCatalog",
    "FemaleHairFrontCatalog",
    "FemaleHairSideCatalog",
    "FemaleHairBackCatalog",
    "FemaleHairExtraCatalog",
    "MaleTopCatalog",
    "MaleBottomCatalog",
    "MaleOnepieceCatalog",
    "MaleShoesCatalog",
    "FemaleTopCatalog",
    "FemaleBottomCatalog",
    "FemaleOnepieceCatalog",
    "FemaleShoesCatalog",
    "MaleHeadAccessoryCatalog",
    "MaleFaceAccessoryCatalog",
    "MaleEarAccessoryCatalog",
    "MaleTailAccessoryCatalog",
    "MaleNeckAccessoryCatalog",
    "FemaleHeadAccessoryCatalog",
    "FemaleFaceAccessoryCatalog",
    "FemaleEarAccessoryCatalog",
    "FemaleTailAccessoryCatalog",
    "FemaleNeckAccessoryCatalog",
)


def main() -> None:
    catalog = unreal.load_asset(CATALOG_PATH)
    if catalog is None:
        raise RuntimeError(f"Missing catalog: {CATALOG_PATH}")
    for property_name in PROPERTIES:
        values = catalog.get_editor_property(property_name)
        valid = sum(value is not None for value in values)
        sample = values[1].get_path_name() if len(values) > 1 and values[1] else "None"
        unreal.log_warning(
            f"CATALOG_VALIDATE {property_name} count={len(values)} valid={valid} sample={sample}"
        )

    for material_path in (
        "/Game/CharacterCustomization/Assets/VRoid/Outfits/Female/Onepiece/Style_N00_001/M_Onepiece_Female_N00_001",
        "/Game/CharacterCustomization/Assets/VRoid/Outfits/Female/Onepiece/Style_N00_107/M_Onepiece_Female_N00_107",
        "/Game/CharacterCustomization/Assets/VRoid/Outfits/Male/Tops/Style_N00_Default/M_Top_Male_N00_Default",
    ):
        material = unreal.load_asset(material_path)
        if material is None:
            unreal.log_error(f"MATERIAL_VALIDATE missing={material_path}")
            continue
        texture_paths = []
        details = ""
        if isinstance(material, unreal.Material):
            textures = unreal.MaterialEditingLibrary.get_used_textures(material)
            texture_paths = [texture.get_path_name() for texture in textures]
        elif isinstance(material, unreal.MaterialInstanceConstant):
            parent = material.get_editor_property("parent")
            parameters = material.get_editor_property("texture_parameter_values")
            texture_paths = [
                value.parameter_value.get_path_name()
                for value in parameters
                if value.parameter_value is not None
            ]
            details = f" parent={parent.get_path_name() if parent else 'None'}"
        unreal.log_warning(
            f"MATERIAL_VALIDATE path={material_path} class={material.get_class().get_name()} "
            f"textures={texture_paths}{details}"
        )

    for mesh_path in (
        "/Game/CharacterCustomization/Assets/VRoid/Accessories/FaceAccessory/SK_FaceAccessory_GlassesHi/SK_FaceAccessory_GlassesHi",
        "/Game/CharacterCustomization/Assets/VRoid/Accessories/FaceAccessory/SK_FaceAccessory_GlassesLow/SK_FaceAccessory_GlassesLow",
        "/Game/CharacterCustomization/Assets/VRoid/Accessories/EarAccessory/SK_EarAccessory_CatEar/SK_EarAccessory_CatEar",
    ):
        mesh = unreal.load_asset(mesh_path)
        if not isinstance(mesh, unreal.SkeletalMesh):
            unreal.log_error(f"ACCESSORY_MATERIAL_VALIDATE missing={mesh_path}")
            continue
        material_details = []
        for slot in mesh.get_editor_property("materials"):
            interface = slot.get_editor_property("material_interface")
            if interface is None:
                material_details.append("None")
                continue
            parent = (
                interface.get_editor_property("parent")
                if isinstance(interface, unreal.MaterialInstanceConstant)
                else None
            )
            overrides = (
                interface.get_editor_property("base_property_overrides")
                if isinstance(interface, unreal.MaterialInstanceConstant)
                else None
            )
            override_two_sided = (
                overrides.get_editor_property("override_two_sided")
                if overrides is not None
                else False
            )
            two_sided = (
                overrides.get_editor_property("two_sided")
                if overrides is not None
                else False
            )
            scalars = (
                interface.get_editor_property("scalar_parameter_values")
                if isinstance(interface, unreal.MaterialInstanceConstant)
                else []
            )
            vectors = (
                interface.get_editor_property("vector_parameter_values")
                if isinstance(interface, unreal.MaterialInstanceConstant)
                else []
            )
            material_details.append(
                f"{interface.get_path_name()} class={interface.get_class().get_name()} "
                f"parent={parent.get_path_name() if parent else 'None'} "
                f"override_two_sided={override_two_sided} two_sided={two_sided} "
                f"scalars={scalars} vectors={vectors}"
            )
        unreal.log_warning(
            f"ACCESSORY_MATERIAL_VALIDATE path={mesh_path} "
            f"bounds={mesh.get_bounds()} materials={material_details}"
        )


if __name__ == "__main__":
    main()
