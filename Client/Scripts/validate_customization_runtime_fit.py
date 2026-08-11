from __future__ import annotations

import math

import unreal


MAP_PATH = "/Game/CharacterCustomization/Maps/L_CharacterCustomization"
PREVIEW_BLUEPRINT = "/Game/CharacterCustomization/Blueprints/BP_CustomizationPreviewActor"
CATALOG_PATH = "/Game/CharacterCustomization/Blueprints/DA_CustomizationCatalog"


def _enum(name: str):
    enum_type = getattr(unreal, "UECustomizationPart", None)
    if enum_type is None:
        enum_type = getattr(unreal, "EUECustomizationPart", None)
    if enum_type is None:
        raise RuntimeError("UECustomizationPart enum is not exposed to Python")
    return getattr(enum_type, name)


def _gender(name: str):
    enum_type = getattr(unreal, "UECharacterGender", None)
    if enum_type is None:
        enum_type = getattr(unreal, "EUECharacterGender", None)
    if enum_type is None:
        raise RuntimeError("UECharacterGender enum is not exposed to Python")
    return getattr(enum_type, name)


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


def _scale_x(component) -> float:
    return float(component.get_editor_property("relative_scale3d").x)


def _nearly_equal(actual: float, expected: float, tolerance: float = 0.015) -> bool:
    return math.isclose(actual, expected, abs_tol=tolerance)


def _assert_scale(component, expected: float, label: str) -> None:
    actual = _scale_x(component)
    if not _nearly_equal(actual, expected):
        raise RuntimeError(f"{label} scale={actual:.4f}, expected {expected:.4f}")


def _assert_leader(component, body_component, label: str) -> None:
    try:
        leader = component.get_editor_property("leader_pose_component")
    except Exception:
        unreal.log_warning(f"FIT_VALIDATE {label} leader pose property not exposed")
        return
    if leader != body_component:
        raise RuntimeError(f"{label} is not following BodyMesh leader pose")


def main() -> None:
    actor = _load_preview_actor()
    actor.initialize_catalogs()
    catalog = unreal.load_asset(CATALOG_PATH)
    if catalog is None:
        raise RuntimeError("Missing customization catalog")

    data = actor.get_appearance()
    data.gender = _gender("FEMALE")
    data.hair_base_style = 1
    data.hair_front_style = actor.resolve_option_index(_enum("HAIR_FRONT"), 0)
    data.hair_side_style = actor.resolve_option_index(_enum("HAIR_SIDE"), 0)
    data.hair_back_style = actor.resolve_option_index(_enum("HAIR_BACK"), 0)
    data.hair_extra_style = actor.resolve_option_index(_enum("HAIR_EXTRA"), 0)
    data.top_style = 1
    data.bottom_style = 1
    data.shoes_style = 1
    data.onepiece_style = 0
    data.head_accessory_style = 1
    data.face_accessory_style = 1
    data.ear_accessory_style = 1

    hair_radial_scale = float(catalog.get_editor_property("HairRadialScale"))
    hair_scalp_inset = float(catalog.get_editor_property("HairScalpInsetScale"))

    for head_size, expected_head_scale in ((0.0, 0.92), (0.5, 1.0), (1.0, 1.08)):
        data.head_size = head_size
        actor.apply_appearance(data)

        body = _component(actor, "BodySkin")
        face = _component(actor, "FaceSkin")
        eye = _component(actor, "EyeIris")
        mouth = _component(actor, "Mouth")
        hair_scalp = _component(actor, "HairScalp")
        hair_front = _component(actor, "HairFront")
        head_accessory = _component(actor, "HeadAccessory")
        face_accessory = _component(actor, "FaceAccessory")

        expected_hair_scale = expected_head_scale * hair_radial_scale
        expected_underlay_scale = expected_hair_scale * hair_scalp_inset
        _assert_scale(face, expected_head_scale, f"FaceSkin head_size={head_size}")
        _assert_scale(eye, expected_head_scale, f"EyeIris head_size={head_size}")
        _assert_scale(mouth, expected_head_scale, f"Mouth head_size={head_size}")
        _assert_scale(head_accessory, expected_head_scale, f"HeadAccessory head_size={head_size}")
        _assert_scale(face_accessory, expected_head_scale, f"FaceAccessory head_size={head_size}")
        _assert_scale(hair_front, expected_hair_scale, f"HairFront head_size={head_size}")
        _assert_scale(hair_scalp, expected_underlay_scale, f"HairScalp head_size={head_size}")
        if not hair_scalp.is_visible() or hair_scalp.get_skeletal_mesh_asset() is None:
            raise RuntimeError("HairScalp underlay is not visible with a mesh")

        for name in (
            "FaceSkin",
            "EyeIris",
            "Mouth",
            "HairScalp",
            "HairFront",
            "HairSide",
            "HairBack",
            "Top",
            "Bottom",
            "Shoes",
            "HeadAccessory",
            "FaceAccessory",
        ):
            _assert_leader(_component(actor, name), body, name)

        unreal.log(
            "FIT_VALIDATE "
            f"head_size={head_size:.2f} "
            f"head_scale={expected_head_scale:.3f} "
            f"hair_scale={expected_hair_scale:.3f} "
            f"underlay_scale={expected_underlay_scale:.3f}"
        )


if __name__ == "__main__":
    main()
