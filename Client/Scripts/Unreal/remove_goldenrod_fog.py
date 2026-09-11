"""Remove Goldenrod fog rendering and fog-wall asset bindings. No PIE."""
import json
from pathlib import Path
import unreal


def remove_walls(city):
    walls = city.get_editor_property("boundary_walls")
    walls.clear_instances()
    walls.set_static_mesh(None)
    walls.set_material(0, None)
    walls.set_visibility(False)
    walls.set_hidden_in_game(True)


def main():
    assets = unreal.EditorAssetLibrary
    level = "/Game/Environments/Goldenrod_R03/Maps/L_Goldenrod"
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name().split(".")[0] != level:
        raise RuntimeError("Open Goldenrod before removing its fog")
    bp = assets.load_asset("/Game/Environments/Goldenrod_R03/Blueprints/BP_GoldenrodCity")
    remove_walls(unreal.get_default_object(bp.generated_class()))
    if not assets.save_loaded_asset(bp, only_if_is_dirty=False):
        raise RuntimeError("Could not save city defaults")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cities = 0
    removed = []
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.UEGoldenrodCity):
            remove_walls(actor)
            cities += 1
        elif isinstance(actor, (unreal.LocalFogVolume, unreal.ExponentialHeightFog)):
            removed.append(actor.get_actor_label())
            actors.destroy_actor(actor)
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError("Could not save Goldenrod")
    report = {"cities_without_fog_walls": cities, "removed_fog_actors": removed}
    output = Path(unreal.Paths.project_saved_dir()) / "Codex/CityPolish/fog_removed.json"
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[FOG REMOVED] " + str(report))


if __name__ == "__main__":
    main()
