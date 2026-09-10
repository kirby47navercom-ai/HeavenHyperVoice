"""Author missing Rotom form DA/ABP/BlendSpace assets and connect all six draws.

Run inside Unreal Editor, without PIE. Reuses imported meshes, animations,
the base Rotom defaults and the existing species/portrait authoring helpers.
"""

import json
import sys
from pathlib import Path

import unreal

SCRIPT_ROOT = Path(__file__).resolve().parent
if str(SCRIPT_ROOT) not in sys.path:
    sys.path.insert(0, str(SCRIPT_ROOT))
import import_mongme2_pokemon as species_tools
from import_round_pokemon_portraits import import_species_portraits

FORMS = (
    ("히트로토무", "Heat", "12"),
    ("워시로토무", "Wash", "13"),
    ("프로스트로토무", "Frost", "14"),
    ("스핀로토무", "Fan", "15"),
    ("커트로토무", "Mow", "16"),
)
BASE = "/Game/Pokemon/SpeciesData/로토무"
UI_PATH = "/Game/Gacha/UI/WBP_GachaStudio"


def add_pool_scroll():
    """Keep the saved Designer layout usable with ten electric candidates."""
    bp = unreal.load_asset(UI_PATH)
    tree_path = UI_PATH + ".WBP_GachaStudio:WidgetTree"
    text = unreal.load_object(None, tree_path + ".PoolText")
    if isinstance(text.get_parent(), unreal.ScrollBox):
        return
    bp.modify()
    tree = unreal.load_object(None, tree_path)
    canvas = text.get_parent()
    position, size = text.slot.get_position(), text.slot.get_size()
    scroll = unreal.ScrollBox(outer=tree, name="PoolScroll")
    canvas_slot = canvas.add_child_to_canvas(scroll)
    canvas_slot.set_position(position)
    canvas_slot.set_size(size)
    canvas_slot.set_z_order(2)
    text.remove_from_parent()
    scroll.add_child(text)
    # UE 5.8은 변수가 아닌 디자이너 위젯도 GUID로 추적하므로 새 스크롤을 등록한다.
    guids = dict(bp.get_editor_property("widget_variable_name_to_guid_map"))
    guids[unreal.Name("PoolScroll")] = unreal.GuidLibrary.new_guid()
    bp.set_editor_property("widget_variable_name_to_guid_map", guids)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)


def main():
    library = unreal.EditorAssetLibrary
    base = species_tools.load_required(BASE + "/DA_로토무")
    base_blend = species_tools.load_required(BASE + "/BS_로토무_Locomotion_1D")
    base_abp = species_tools.load_required(BASE + "/ABP_로토무")
    defaults = unreal.get_default_object(base_abp.generated_class())
    all_species = [base]
    created = []
    previous_template = species_tools.ABP_TEMPLATE
    species_tools.ABP_TEMPLATE = BASE + "/ABP_로토무"
    try:
        for name, english, form_id in FORMS:
            path = f"/Game/Pokemon/SpeciesData/{name}/DA_{name}"
            data = species_tools.load_if_exists(path)
            if data:
                all_species.append(data)
                continue
            asset_root = f"/Game/Pokemon/Asset/로토무/{name}"
            mesh = species_tools.load_required(
                asset_root + f"/모델/Rotom_{english}_pm0479_{form_id}_00_Rigged")
            animations = {}
            for animation_path in library.list_assets(asset_root + "/애니메이션"):
                animation = library.load_asset(animation_path)
                if isinstance(animation, unreal.AnimSequence):
                    animations[animation.get_name()] = animation
            form = {"Mesh": mesh, "Skeleton": species_tools.get_skeleton(mesh),
                    "Animations": animations}
            species = {"Korean": name, "English": "Rotom" + english, "Dex": 479,
                       "WalkSpeed": defaults.get_editor_property("walk_animation_speed"),
                       "RunSpeed": defaults.get_editor_property("run_animation_speed")}
            blend = species_tools.create_blend_space(species, form, base_blend)
            abp = species_tools.create_anim_blueprint(species, form, blend)
            # 로토무의 공용 능력치·오디오·변환값을 복사한 다음, 모든 애니메이션
            # 슬롯을 이 폼의 스켈레톤에 맞는 시퀀스로 교체한다.
            data = library.duplicate_asset(BASE + "/DA_로토무", path)
            if data is None:
                raise RuntimeError(f"Could not create {path}")
            data.set_editor_property("profile_icon", None)
            data.set_editor_property("spawn_montage", None)
            data.set_editor_property("despawn_montage", None)
            data.set_editor_property("extra_animations", {})
            data = species_tools.create_species_data(species, form, abp, base)
            all_species.append(data)
            created.append(name)
    finally:
        species_tools.ABP_TEMPLATE = previous_template

    missing_icons = [data.get_name().removeprefix("DA_") for data in all_species
                     if data.get_editor_property("profile_icon") is None]
    import_species_portraits(missing_icons)

    pool = species_tools.load_required("/Game/Gacha/Data/DA_Gacha_Electric")
    entries = list(pool.get_editor_property("entries"))
    rotom_entries = [entry for entry in entries if entry.get_editor_property("dex_number") == 479]
    if not rotom_entries:
        raise RuntimeError("No existing Rotom draw weight to distribute")
    existing = {entry.get_editor_property("species") for entry in rotom_entries}
    if not all(data in existing for data in all_species):
        # 기존 로토무 전체 확률은 유지하고, 추가된 폼 사이에서만 가중치를 나눈다.
        total_weight = sum(entry.get_editor_property("weight") for entry in rotom_entries)
        template = rotom_entries[0]
        replacements = []
        for data in all_species:
            entry = template.copy()
            entry.set_editor_property("species", data)
            entry.set_editor_property("display_name", data.get_editor_property("display_name"))
            entry.set_editor_property("weight", total_weight / len(all_species))
            replacements.append(entry)
        first = next(i for i, entry in enumerate(entries) if entry.get_editor_property("dex_number") == 479)
        entries = [entry for entry in entries if entry.get_editor_property("dex_number") != 479]
        entries[first:first] = replacements
        pool.set_editor_property("entries", entries)
        library.save_loaded_asset(pool)

    add_pool_scroll()
    report = {"created_species": created, "created_icons": missing_icons,
              "forms": [{"data": data.get_path_name(),
                         "mesh": data.get_editor_property("skeletal_mesh").get_path_name(),
                         "anim_bp": data.get_editor_property("anim_instance_class").get_path_name(),
                         "icon": data.get_editor_property("profile_icon").get_path_name()}
                        for data in all_species],
              "electric_entries": len(entries),
              "rotom_weights": [entry.get_editor_property("weight") for entry in entries
                                if entry.get_editor_property("dex_number") == 479]}
    output = Path(unreal.Paths.project_saved_dir()) / "Codex/Gacha/rotom_forms_connected.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[ROTOM FORMS] Saved six connected forms and a scrollable candidate list")


if __name__ == "__main__":
    main()
