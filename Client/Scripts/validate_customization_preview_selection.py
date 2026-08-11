from __future__ import annotations

import unreal


MAP_PATH = "/Game/CharacterCustomization/Maps/L_CharacterCustomization"
PREVIEW_BLUEPRINT = "/Game/CharacterCustomization/Blueprints/BP_CustomizationPreviewActor"


def _load_preview_actor():
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_name().startswith("CustomizationPreview"):
            return actor
    preview_class = unreal.EditorAssetLibrary.load_blueprint_class(PREVIEW_BLUEPRINT)
    if preview_class is None:
        raise RuntimeError(f"Missing preview blueprint: {PREVIEW_BLUEPRINT}")
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        preview_class,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if actor is None:
        raise RuntimeError(f"Could not spawn preview actor from {PREVIEW_BLUEPRINT}")
    return actor


def _component(actor, name: str):
    for component in actor.get_components_by_class(unreal.SkeletalMeshComponent):
        if component.get_name() == name:
            return component
    raise RuntimeError(f"Missing component: {name}")


def _mesh_path(component) -> str:
    mesh = component.get_skeletal_mesh_asset()
    return mesh.get_path_name() if mesh else "None"


def _assert_visible_mesh(actor, name: str) -> None:
    component = _component(actor, name)
    if not component.is_visible():
        raise RuntimeError(f"{name} is hidden after option selection")
    if component.get_skeletal_mesh_asset() is None:
        raise RuntimeError(f"{name} lost its mesh after option selection")
    unreal.log(f"PREVIEW_VALIDATE {name} mesh={_mesh_path(component)} visible=True")


def _relative_location(component):
    return component.get_editor_property("relative_location")


def _is_nearly_zero(vector, tolerance: float = 0.01) -> bool:
    return (
        abs(vector.x) <= tolerance
        and abs(vector.y) <= tolerance
        and abs(vector.z) <= tolerance
    )


def _enum(name: str):
    enum_type = getattr(unreal, "UECustomizationPart", None)
    if enum_type is None:
        enum_type = getattr(unreal, "EUECustomizationPart", None)
    if enum_type is None:
        raise RuntimeError("UECustomizationPart enum is not exposed to Python")
    return getattr(enum_type, name)


def _last_index(actor, part_name: str) -> int:
    count = actor.get_option_count(_enum(part_name))
    if count <= 0:
        raise RuntimeError(f"{part_name} has no options")
    return count - 1


def _last_raw_index(actor, part_name: str) -> int:
    part = _enum(part_name)
    return actor.resolve_option_index(part, _last_index(actor, part_name))


def _assert_option_catalog_mapping(actor, catalog) -> None:
    hair_set = _enum("HAIR_SET")
    hair_set_count = actor.get_option_count(hair_set)
    if hair_set_count <= 0:
        raise RuntimeError("HairSet has no display options")
    raw_hair_set_count = len(catalog.get_editor_property("MaleHairFrontCatalog") or [])
    if raw_hair_set_count > 0 and hair_set_count >= raw_hair_set_count:
        raise RuntimeError(
            "HairSet shape duplicate entries were not collapsed: "
            f"display={hair_set_count} raw={raw_hair_set_count}"
        )
    first_hair_label = actor.get_option_label(hair_set, 0)
    if "Preset 87" in first_hair_label:
        raise RuntimeError(f"HairSet still uses the wrong fallback label: {first_hair_label}")

    hair_extra = _enum("HAIR_EXTRA")
    raw_hair_extra_count = len(catalog.get_editor_property("MaleHairExtraCatalog") or [])
    display_hair_extra_count = actor.get_option_count(hair_extra)
    if raw_hair_extra_count > 0 and display_hair_extra_count >= raw_hair_extra_count:
        raise RuntimeError(
            "HairExtra duplicate None entries were not collapsed: "
            f"display={display_hair_extra_count} raw={raw_hair_extra_count}"
        )
    unreal.log(
        "PREVIEW_VALIDATE option_mapping "
        f"hair_set_display={hair_set_count} hair_set_raw={raw_hair_set_count} "
        f"hair_set_label={first_hair_label} "
        f"hair_extra_display={display_hair_extra_count} hair_extra_raw={raw_hair_extra_count}"
    )


def main() -> None:
    actor = _load_preview_actor()
    actor.initialize_catalogs()
    catalog = unreal.load_asset("/Game/CharacterCustomization/Blueprints/DA_CustomizationCatalog")
    if catalog is None:
        raise RuntimeError("Missing customization catalog")
    _assert_option_catalog_mapping(actor, catalog)

    data = actor.get_appearance()
    data.face_style = _last_raw_index(actor, "FACE_SKIN")
    data.eye_white_style = _last_raw_index(actor, "EYE_WHITE")
    data.eye_iris_style = _last_raw_index(actor, "EYE_IRIS")
    data.eye_highlight_style = _last_raw_index(actor, "EYE_HIGHLIGHT")
    data.brow_style = _last_raw_index(actor, "BROW")
    data.eyelash_style = _last_raw_index(actor, "EYELASH")
    data.eyeline_style = _last_raw_index(actor, "EYELINE")
    data.mouth_style = _last_raw_index(actor, "MOUTH")
    data.lip_style = _last_raw_index(actor, "LIP")
    data.mouth_line_style = _last_raw_index(actor, "MOUTH_LINE")
    actor.apply_appearance(data)

    for name in ("FaceSkin", "EyeWhite", "EyeIris", "Mouth"):
        _assert_visible_mesh(actor, name)

    head_location = _relative_location(_component(actor, "HeadAccessory"))
    face_location = _relative_location(_component(actor, "FaceAccessory"))
    expected_head_z = catalog.get_editor_property("HeadAccessoryVerticalOffset")
    expected_face_y = catalog.get_editor_property("FaceAccessoryForwardOffset")
    expected_face_z = catalog.get_editor_property("FaceAccessoryVerticalOffset")
    if abs(head_location.z - expected_head_z) > 0.01 or abs(head_location.x) > 0.01 or abs(head_location.y) > 0.01:
        raise RuntimeError(f"HeadAccessory fit offset is wrong: {head_location}")
    if abs(face_location.y - expected_face_y) > 0.01 or abs(face_location.z - expected_face_z) > 0.01 or abs(face_location.x) > 0.01:
        raise RuntimeError(f"FaceAccessory fit offset is wrong: {face_location}")
    unreal.log(
        "PREVIEW_VALIDATE accessory_offsets "
        f"head=({head_location.x:.2f},{head_location.y:.2f},{head_location.z:.2f}) "
        f"face=({face_location.x:.2f},{face_location.y:.2f},{face_location.z:.2f})"
    )


if __name__ == "__main__":
    main()
