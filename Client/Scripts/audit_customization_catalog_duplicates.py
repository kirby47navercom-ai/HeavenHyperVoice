from __future__ import annotations

from collections import defaultdict
from pathlib import Path

import unreal


CATALOG_PATH = "/Game/CharacterCustomization/Blueprints/DA_CustomizationCatalog"
OUTPUT_PATH = Path(unreal.Paths.project_saved_dir()) / "Diagnostics" / "CustomizationCatalogDuplicates.txt"


MESH_FIELDS = (
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

TEXTURE_FIELDS = (
    "FaceSkinTextureCatalog", "FaceSkinNormalTextureCatalog", "EyeWhiteTextureCatalog",
    "EyeIrisTextureCatalog", "EyeHighlightTextureCatalog", "BrowTextureCatalog",
    "EyelashTextureCatalog", "EyelineTextureCatalog", "MouthTextureCatalog",
    "LipTextureCatalog", "MouthLineTextureCatalog",
)

DIFFUSE_PARAMS = ("DiffuseColorMap", "MainImage", "BaseColorMap", "BaseMap", "_MainTex")


def _path(asset) -> str:
    return asset.get_path_name() if asset else "None"


def _mesh_preview_texture(mesh) -> str:
    if mesh is None:
        return "None"
    for slot in mesh.get_editor_property("materials") or []:
        material = slot.get_editor_property("material_interface")
        if material is None:
            continue
        for param in DIFFUSE_PARAMS:
            texture = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(
                material, param
            )
            if texture:
                return texture.get_path_name()
    return "None"


def _mesh_shape_key(mesh) -> str:
    if mesh is None:
        return "None"
    bounds = mesh.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent
    return (
        f"origin=({origin.x:.2f},{origin.y:.2f},{origin.z:.2f})|"
        f"extent=({extent.x:.2f},{extent.y:.2f},{extent.z:.2f})|"
        f"radius={bounds.sphere_radius:.2f}|"
        f"texture={_mesh_preview_texture(mesh).rsplit('/', 1)[-1]}"
    )


def _summarize_duplicates(lines: list[str], field: str, values: list[str]) -> None:
    groups: dict[str, list[int]] = defaultdict(list)
    for index, value in enumerate(values):
        groups[value].append(index)
    duplicate_groups = {key: indices for key, indices in groups.items() if len(indices) > 1}
    duplicate_entries = sum(len(indices) - 1 for indices in duplicate_groups.values())
    lines.append(
        f"{field}: count={len(values)} unique={len(groups)} duplicate_entries={duplicate_entries}"
    )
    for key, indices in sorted(
        duplicate_groups.items(), key=lambda item: (-len(item[1]), item[1][0])
    )[:12]:
        lines.append(f"  DUP x{len(indices)} indices={indices[:20]} key={key}")


def main() -> None:
    catalog = unreal.load_asset(CATALOG_PATH)
    if catalog is None:
        raise RuntimeError(f"Missing catalog: {CATALOG_PATH}")

    lines: list[str] = []
    lines.append("Customization catalog duplicate audit")
    for field in MESH_FIELDS:
        meshes = list(catalog.get_editor_property(field) or [])
        _summarize_duplicates(lines, field, [_path(mesh) for mesh in meshes])
        if "Hair" in field:
            _summarize_duplicates(
                lines,
                f"{field}.preview_texture",
                [_mesh_preview_texture(mesh) for mesh in meshes],
            )
            _summarize_duplicates(
                lines,
                f"{field}.shape_key",
                [_mesh_shape_key(mesh) for mesh in meshes],
            )
    for field in TEXTURE_FIELDS:
        textures = list(catalog.get_editor_property(field) or [])
        _summarize_duplicates(lines, field, [_path(texture) for texture in textures])

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text("\n".join(lines), encoding="utf-8")
    unreal.log(f"CUSTOMIZATION_DUPLICATE_AUDIT {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
