from __future__ import annotations

import unreal


CATALOG_PATH = "/Game/CharacterCustomization/Blueprints/DA_CustomizationCatalog"


CATALOG_FIELDS = (
    "MaleBodyCatalog", "FemaleBodyCatalog",
    "MaleFaceSkinCatalog", "FemaleFaceSkinCatalog",
    "MaleEyeWhiteCatalog", "FemaleEyeWhiteCatalog",
    "MaleEyeIrisCatalog", "FemaleEyeIrisCatalog",
    "MaleEyeHighlightCatalog", "FemaleEyeHighlightCatalog",
    "MaleEyeExtraCatalog", "FemaleEyeExtraCatalog",
    "MaleBrowCatalog", "FemaleBrowCatalog",
    "MaleEyelashCatalog", "FemaleEyelashCatalog",
    "MaleEyelineCatalog", "FemaleEyelineCatalog",
    "MaleMouthCatalog", "FemaleMouthCatalog",
    "MaleHairBaseCatalog", "FemaleHairBaseCatalog",
    "MaleHairFrontCatalog", "FemaleHairFrontCatalog",
    "MaleHairSideCatalog", "FemaleHairSideCatalog",
    "MaleHairBackCatalog", "FemaleHairBackCatalog",
    "MaleHairExtraCatalog", "FemaleHairExtraCatalog",
    "MaleTopCatalog", "FemaleTopCatalog",
    "MaleBottomCatalog", "FemaleBottomCatalog",
    "MaleOnepieceCatalog", "FemaleOnepieceCatalog",
    "MaleShoesCatalog", "FemaleShoesCatalog",
    "MaleHeadAccessoryCatalog", "FemaleHeadAccessoryCatalog",
    "MaleFaceAccessoryCatalog", "FemaleFaceAccessoryCatalog",
    "MaleEarAccessoryCatalog", "FemaleEarAccessoryCatalog",
    "MaleTailAccessoryCatalog", "FemaleTailAccessoryCatalog",
    "MaleNeckAccessoryCatalog", "FemaleNeckAccessoryCatalog",
)


def main() -> None:
    catalog = unreal.load_asset(CATALOG_PATH)
    if catalog is None:
        raise RuntimeError(f"Missing catalog: {CATALOG_PATH}")

    common = catalog.get_editor_property("CommonSkeleton")
    if common is None:
        raise RuntimeError("Catalog CommonSkeleton is empty")
    unreal.log(f"COMMON_SKELETON {common.get_path_name()}")

    total_meshes = 0
    total_missing = 0
    for field in CATALOG_FIELDS:
        meshes = catalog.get_editor_property(field) or []
        missing = sum(mesh is None for mesh in meshes)
        total_meshes += len(meshes)
        total_missing += missing
        skeletons = sorted({mesh.get_editor_property("skeleton").get_path_name() for mesh in meshes if mesh and mesh.get_editor_property("skeleton")})
        material_slots = sorted({len(mesh.get_editor_property("materials")) for mesh in meshes if mesh})
        unreal.log(
            f"CATALOG {field} count={len(meshes)} missing={missing} "
            f"skeletons={skeletons} material_slots={material_slots}"
        )

    texture_fields = (
        "FaceSkinTextureCatalog", "FaceSkinNormalTextureCatalog", "EyeWhiteTextureCatalog",
        "EyeIrisTextureCatalog", "EyeHighlightTextureCatalog", "BrowTextureCatalog",
        "EyelashTextureCatalog", "EyelineTextureCatalog", "MouthTextureCatalog",
        "LipTextureCatalog", "MouthLineTextureCatalog",
    )
    for field in texture_fields:
        textures = catalog.get_editor_property(field) or []
        unreal.log(f"TEXTURE_CATALOG {field} count={len(textures)} missing={sum(x is None for x in textures)}")

    unreal.log(f"TOTAL_MESH_ENTRIES {total_meshes} TOTAL_MISSING_ENTRIES {total_missing}")


if __name__ == "__main__":
    main()
