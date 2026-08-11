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


def main() -> None:
    actor = _load_preview_actor()
    actor.initialize_catalogs()

    data = actor.get_appearance()
    data.face_style = _last_index(actor, "FACE_SKIN")
    data.eye_white_style = _last_index(actor, "EYE_WHITE")
    data.eye_iris_style = _last_index(actor, "EYE_IRIS")
    data.eye_highlight_style = _last_index(actor, "EYE_HIGHLIGHT")
    data.brow_style = _last_index(actor, "BROW")
    data.eyelash_style = _last_index(actor, "EYELASH")
    data.eyeline_style = _last_index(actor, "EYELINE")
    data.mouth_style = _last_index(actor, "MOUTH")
    data.lip_style = _last_index(actor, "LIP")
    data.mouth_line_style = _last_index(actor, "MOUTH_LINE")
    actor.apply_appearance(data)

    for name in ("FaceSkin", "EyeWhite", "EyeIris", "Mouth"):
        _assert_visible_mesh(actor, name)

    head_location = _relative_location(_component(actor, "HeadAccessory"))
    face_location = _relative_location(_component(actor, "FaceAccessory"))
    if abs(head_location.z - 9.0) > 0.01 or abs(head_location.x) > 0.01 or abs(head_location.y) > 0.01:
        raise RuntimeError(f"HeadAccessory fit offset is wrong: {head_location}")
    if abs(face_location.y - 6.5) > 0.01 or abs(face_location.z - 12.5) > 0.01 or abs(face_location.x) > 0.01:
        raise RuntimeError(f"FaceAccessory fit offset is wrong: {face_location}")
    unreal.log(
        "PREVIEW_VALIDATE accessory_offsets "
        f"head=({head_location.x:.2f},{head_location.y:.2f},{head_location.z:.2f}) "
        f"face=({face_location.x:.2f},{face_location.y:.2f},{face_location.z:.2f})"
    )


if __name__ == "__main__":
    main()
