"""이번 리타겟 묶음에서 추가한 13종만 사용하는 별도 야생 포켓몬 스포너를 만든다.

기존 /Game/Pokemon/spawner와 전체 종족 카탈로그는 수정하지 않는다.
새 스포너는 지정된 WildPokemonSpeciesPool만 사용하므로 기존 종이 섞이지 않는다.
"""

import json
from pathlib import Path

import unreal


SOURCE_SPAWNER = "/Game/Pokemon/spawner"
DESTINATION_DIRECTORY = "/Game/Pokemon"
DESTINATION_SPAWNER = f"{DESTINATION_DIRECTORY}/들판"
LEGACY_SPAWNERS = (
    "/Game/Pokemon/필드",
    "/Game/Pokemon/Spawner/\ubabd\ubbc02/BP_\ubabd\ubbc02_야생포켓몬스포너",
    "/Game/Pokemon/Spawner/\ubabd\ubb502/BP_\ubabd\ubb502_야생포켓몬스포너",
)
# 보고서는 각 팀원의 현재 언리얼 프로젝트 Saved 폴더에 저장한다.
REPORT_PATH = (
    Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))
    / "Codex"
    / "mongme2_spawner_report.json"
)

SPECIES_NAMES = (
    "치코리타",
    "나무지기",
    "나오하",
    "찌르꼬",
    "흔들풍손",
    "콘팡",
    "치릴리",
    "가디",
    "도치마론",
    "나몰빼미",
    "영구스",
    "암멍이",
    "브케인",
)

editor_assets = unreal.EditorAssetLibrary


def load_required(path):
    if not editor_assets.does_asset_exist(path):
        raise RuntimeError(f"필수 에셋을 찾을 수 없습니다: {path}")
    asset = editor_assets.load_asset(path)
    if not asset:
        raise RuntimeError(f"필수 에셋 로드 실패: {path}")
    return asset


def asset_path(asset):
    return asset.get_path_name().split(".", 1)[0]


def main():
    source_blueprint = load_required(SOURCE_SPAWNER)
    source_defaults = unreal.get_default_object(source_blueprint.generated_class())
    source_pool_before = tuple(
        asset_path(species)
        for species in source_defaults.get_editor_property("wild_pokemon_species_pool")
    )

    species_pool = [
        load_required(f"/Game/Pokemon/SpeciesData/{name}/DA_{name}")
        for name in SPECIES_NAMES
    ]

    editor_assets.make_directory(DESTINATION_DIRECTORY)
    # 이전에 만든 긴 이름의 에셋은 참조를 보존하며 /Game/Pokemon/들판으로 이동한다.
    for legacy_spawner in LEGACY_SPAWNERS:
        if not editor_assets.does_asset_exist(legacy_spawner):
            continue
        if not editor_assets.does_asset_exist(DESTINATION_SPAWNER):
            if not editor_assets.rename_asset(legacy_spawner, DESTINATION_SPAWNER):
                raise RuntimeError("기존 스포너 에셋을 /Game/Pokemon/들판으로 이동하지 못했습니다.")
        else:
            editor_assets.delete_asset(legacy_spawner)

    if editor_assets.does_asset_exist(DESTINATION_SPAWNER):
        blueprint = load_required(DESTINATION_SPAWNER)
    else:
        blueprint = editor_assets.duplicate_asset(SOURCE_SPAWNER, DESTINATION_SPAWNER)
    if not blueprint:
        raise RuntimeError(f"스포너 블루프린트 생성 실패: {DESTINATION_SPAWNER}")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    generated_class = blueprint.generated_class()
    if not generated_class:
        raise RuntimeError("스포너 블루프린트의 GeneratedClass가 없습니다.")

    defaults = unreal.get_default_object(generated_class)

    # 추가한 13종을 한 번씩 확인하기 좋도록 기본 소환 수도 13으로 맞춘다.
    defaults.set_editor_property("wild_pokemon_count", len(species_pool))
    defaults.set_editor_property("wild_pokemon_species_pool", species_pool)

    # 명시 풀이 비어 있을 때 기존 카탈로그로 돌아가지 않게 별도 참조는 비운다.
    defaults.set_editor_property("wild_pokemon_species_data", None)
    defaults.set_editor_property("pokemon_species_catalog", None)

    # 기존 스포너와 같은 레벨에 배치해도 RuntimePokemonId가 겹치지 않도록 대역을 분리한다.
    defaults.set_editor_property("first_runtime_pokemon_id", 200000)
    defaults.set_editor_property("spawn_on_begin_play", True)
    defaults.set_editor_property("allow_local_spawn_with_external_field_server", False)
    defaults.set_editor_property("use_fixed_spawn_seed", False)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    editor_assets.save_loaded_asset(blueprint, only_if_is_dirty=False)

    # 저장 후 자신과 기존 스포너를 모두 검증한다.
    saved_blueprint = load_required(DESTINATION_SPAWNER)
    saved_defaults = unreal.get_default_object(saved_blueprint.generated_class())
    saved_pool = tuple(
        asset_path(species)
        for species in saved_defaults.get_editor_property("wild_pokemon_species_pool")
    )
    expected_pool = tuple(asset_path(species) for species in species_pool)

    errors = []
    if saved_pool != expected_pool:
        errors.append("저장된 종족 풀이 13종 목록과 다릅니다.")
    if saved_defaults.get_editor_property("wild_pokemon_count") != len(species_pool):
        errors.append("기본 소환 수가 13이 아닙니다.")
    if saved_defaults.get_editor_property("pokemon_species_catalog") is not None:
        errors.append("별도 스포너에 기존 카탈로그가 연결되어 있습니다.")
    if saved_defaults.get_editor_property("first_runtime_pokemon_id") != 200000:
        errors.append("RuntimePokemonId 시작값이 200000이 아닙니다.")

    source_pool_after = tuple(
        asset_path(species)
        for species in source_defaults.get_editor_property("wild_pokemon_species_pool")
    )
    if source_pool_after != source_pool_before:
        errors.append("기존 /Game/Pokemon/spawner의 종족 풀이 변경됐습니다.")

    report = {
        "Spawner": saved_blueprint.get_path_name(),
        "SpawnCount": saved_defaults.get_editor_property("wild_pokemon_count"),
        "FirstRuntimePokemonId": saved_defaults.get_editor_property("first_runtime_pokemon_id"),
        "Species": list(SPECIES_NAMES),
        "SpeciesAssets": list(saved_pool),
        "ExistingSpawnerModified": source_pool_after != source_pool_before,
        "PlacedInLevel": False,
        "Errors": errors,
        "Passed": not errors,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(
        json.dumps(report, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    if errors:
        raise RuntimeError("; ".join(errors))
    unreal.log(f"[MONGME2_SPAWNER] PASS {saved_blueprint.get_path_name()}")


main()
