"""Compile movement Blueprints after replacing the native movement components."""

import json
import os

import unreal


ASSETS = [
    "/Game/Character/Player/BP_PlayerCharacter",
    "/Game/Pokemon/BP_Pokemon",
    "/Game/Pokemon/SpeciesData/Common/ABP_PokemonBase",
    "/Game/Data/Animation/HeavenHyperVoice/Player/ABP_UEAnimInstance",
    "/Game/Data/Animation/HeavenHyperVoice/Player/ABP_UEAnimInstance_Male",
]


def main():
    results = []
    for path in ASSETS:
        blueprint = unreal.load_asset(path)
        if not blueprint:
            raise RuntimeError(f"Missing movement Blueprint: {path}")

        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        if path in ASSETS[:2]:
            defaults = unreal.get_default_object(blueprint.generated_class())
            core_class = unreal.load_class(None, "/Script/HeavenHyperVoice.UECoreMovementComponent")
            if not defaults.get_component_by_class(core_class):
                raise RuntimeError(f"Blueprint has no shared movement component: {path}")
            if defaults.get_component_by_class(unreal.CharacterMovementComponent):
                raise RuntimeError(f"Blueprint still has CharacterMovement: {path}")

        if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save compiled Blueprint: {path}")

        results.append({"asset": path, "compiled_and_saved": True})
        unreal.log(f"SHARED_MOVEMENT_BLUEPRINT: {path}")

    report = os.path.join(unreal.Paths.project_saved_dir(), "HHVQA", "shared_movement.json")
    os.makedirs(os.path.dirname(report), exist_ok=True)
    with open(report, "w", encoding="utf-8") as output:
        json.dump(results, output, indent=2, ensure_ascii=False)


main()
