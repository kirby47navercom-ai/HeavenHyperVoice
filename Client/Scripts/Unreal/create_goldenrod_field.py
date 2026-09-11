"""Create the Goldenrod field from the existing gameplay map, then wire travel.

Editor authoring only; no PIE. Existing field is retained as a template.
Native collision comes from AUEGoldenrodCity, never the visual StaticMesh.
"""
import json
from pathlib import Path
import unreal

ROOT = "/Game/Environments/Goldenrod_R03"
LEVEL = ROOT + "/Maps/L_Goldenrod"
BLUEPRINT = ROOT + "/Blueprints/BP_GoldenrodCity"


def main():
    assets = unreal.EditorAssetLibrary
    if assets.does_asset_exist(LEVEL):
        raise RuntimeError("Goldenrod field already exists; preserve manual edits")
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.UEGoldenrodCity)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "BP_GoldenrodCity", ROOT + "/Blueprints", unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    mesh = assets.load_asset(ROOT + "/Meshes/SM_Goldenrod_City_R03")
    defaults = unreal.get_default_object(bp.generated_class())
    defaults.get_editor_property("city_mesh").set_static_mesh(mesh)
    assets.save_loaded_asset(bp, only_if_is_dirty=False)

    if not unreal.EditorLevelLibrary.new_level_from_template(LEVEL, "/Game/Level/PlayerTestLevel"):
        raise RuntimeError("Could not create Goldenrod gameplay level")
    # Template creation leaves the active world in /Temp; load its saved package before editing.
    if not unreal.EditorLevelLibrary.load_level(LEVEL):
        raise RuntimeError("Could not load saved Goldenrod level")
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    portals, starts = [], []
    for actor in subsystem.get_all_level_actors():
        if isinstance(actor, unreal.PlayerStart):
            starts.append(actor)
        elif isinstance(actor, unreal.UEInstancePortal):
            portals.append(actor)
        elif isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label() != "SM_SkySphere":
            subsystem.destroy_actor(actor)
    city = subsystem.spawn_actor_from_class(bp.generated_class(), unreal.Vector(0, 0, 0))
    city.set_actor_label("Goldenrod City — Native Collision")
    if not starts:
        starts.append(subsystem.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector()))
    for i, start in enumerate(starts):
        start.set_actor_location(unreal.Vector(0, i * 180, 90), False, False)
        start.set_actor_rotation(unreal.Rotator(pitch=0, yaw=-90, roll=0), False)
    for i, portal in enumerate(portals):
        portal.set_actor_location(unreal.Vector(0, -700 - i * 550, 0), False, False)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    selection = assets.load_asset("/Game/CharacterSelection/Blueprints/WBP_CharacterSelection")
    selection_defaults = unreal.get_default_object(selection.generated_class())
    world.get_world_settings().set_editor_property(
        "default_game_mode", selection_defaults.get_editor_property("gameplay_game_mode_class"))
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(
        unreal.Vector(0, 1100, 550), unreal.Rotator(pitch=-24, yaw=-90, roll=0))
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError("Could not save Goldenrod level")
    selection_defaults.set_editor_property("gameplay_level", world)
    assets.save_loaded_asset(selection, only_if_is_dirty=False)
    output = Path(unreal.Paths.project_saved_dir()) / "Codex/Goldenrod/field_created.json"
    output.write_text(json.dumps({"level": LEVEL, "city": city.get_path_name(),
        "native_boxes": len(city.get_components_by_class(unreal.BoxComponent)),
        "portals": len(portals), "player_starts": len(starts)}, indent=2), encoding="utf-8")
    unreal.log("[GOLDENROD FIELD] Level, collision actor, spawn, portals and login travel saved")


if __name__ == "__main__":
    main()
