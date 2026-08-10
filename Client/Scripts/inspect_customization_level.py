from __future__ import annotations

import unreal


MAP = "/Game/CharacterCustomization/Maps/L_CharacterCustomization"


unreal.EditorLoadingAndSavingUtils.load_map(MAP)
actors = unreal.EditorLevelLibrary.get_all_level_actors()
for actor in actors:
    class_name = actor.get_class().get_name()
    if "Customization" in class_name or "Customization" in actor.get_name():
        unreal.log(
            f"CUSTOMIZATION_ACTOR name={actor.get_name()} class={class_name} "
            f"location={actor.get_actor_location()}"
        )
unreal.log(f"LEVEL_ACTOR_COUNT {len(actors)}")
