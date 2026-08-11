from __future__ import annotations

import unreal


CATALOG_PATH = "/Game/CharacterCustomization/Blueprints/DA_CustomizationCatalog"
SKELETON_PATH = "/Game/CharacterCustomization/Assets/VRoid/Skeletons/SK_VRoidCommon"
HAIR_ROOT = "/Game/VRoidCatalog/HairDetermined"
STYLE_IDS = list(range(2388, 2414))


def load_mesh(path: str, required: bool = True):
    mesh = unreal.load_asset(path)
    if isinstance(mesh, unreal.SkeletalMesh):
        return mesh
    if required:
        raise RuntimeError(f"Missing skeletal mesh: {path}")
    return None


def hair_mesh(gender: str, style_id: int, semantic: str, required: bool = True):
    path = (
        f"{HAIR_ROOT}/{gender}/Style_{style_id}/"
        f"SK_Hair{semantic}_{gender}_{style_id}"
    )
    return load_mesh(path, required=required)


def main() -> None:
    catalog = unreal.load_asset(CATALOG_PATH)
    if catalog is None:
        raise RuntimeError(f"Missing catalog: {CATALOG_PATH}")

    skeleton = unreal.load_asset(SKELETON_PATH)
    if not isinstance(skeleton, unreal.Skeleton):
        raise RuntimeError(f"Missing common VRoid skeleton: {SKELETON_PATH}")
    catalog.set_editor_property("CommonSkeleton", skeleton)

    catalog.set_editor_property("HeadAccessoryVerticalOffset", 9.0)
    catalog.set_editor_property("FaceAccessoryForwardOffset", 6.5)
    catalog.set_editor_property("FaceAccessoryVerticalOffset", 17.5)
    for gender in ("Male", "Female"):
        fronts = []
        sides = []
        backs = []
        extras = []
        for style_id in STYLE_IDS:
            fronts.append(hair_mesh(gender, style_id, "Front"))
            sides.append(hair_mesh(gender, style_id, "Side"))
            backs.append(hair_mesh(gender, style_id, "Back"))
            extras.append(hair_mesh(gender, style_id, "Extra", required=False))
        catalog.set_editor_property(f"{gender}HairFrontCatalog", fronts)
        catalog.set_editor_property(f"{gender}HairSideCatalog", sides)
        catalog.set_editor_property(f"{gender}HairBackCatalog", backs)
        catalog.set_editor_property(f"{gender}HairExtraCatalog", extras)
        catalog.set_editor_property(f"{gender}HairStyleIds", STYLE_IDS)
        unreal.log_warning(
            f"HAIR_CATALOG {gender} sets={len(fronts)} extras={sum(x is not None for x in extras)}"
        )
    catalog.modify()
    if not unreal.EditorAssetLibrary.save_loaded_asset(catalog, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save catalog: {CATALOG_PATH}")
    unreal.log_warning("HAIR_CATALOG_COMPLETE")


if __name__ == "__main__":
    main()
