"""현재 프로젝트의 20종 포켓몬 울음을 상황별로 SpeciesData에 연결한다.

원본은 Client/SourceArt/Pokemon/Audio/Cries 아래의 모노 WAV를 사용한다.
런타임 코드에는 파일 경로를 넣지 않고, 임포트된 에셋 참조만 DataAsset에 저장한다.
이미 /Game/Pokemon/Audio/Cries 아래에 있는 상황별 울음은 이름에 맞춰 소환·감정·전투·환경 배열로 나눈다.
"""

from pathlib import Path

import unreal


SPECIES = {
    "귀뚤뚜기": "KRICKETOT",
    "기라티나": "GIRATINA",
    "꼬링크": "SHINX",
    "꼬부기": "SQUIRTLE",
    "꽁어름": "BERGMITE",
    "디아루가": "DIALGA",
    "랄토스": "RALTS",
    "모부기": "TURTWIG",
    "벼리짱": "TINKATUFF",
    "불꽃숭이": "CHIMCHAR",
    "아르세우스": "ARCEUS",
    "이브이": "EEVEE",
    "이상해씨": "BULBASAUR",
    "자망칼": "PAWNIARD",
    "터검니": "AXEW",
    "파이리": "CHARMANDER",
    "파치리스": "PACHIRISU",
    "팽도리": "PIPLUP",
    "펄기아": "PALKIA",
    "피카츄": "PIKACHU",
}

CONTENT_ROOT = "/Game/Pokemon/Audio"
SHARED_ROOT = f"{CONTENT_ROOT}/Common"
SOURCE_ROOT = Path(unreal.Paths.project_dir()) / "SourceArt" / "Pokemon" / "Audio" / "Cries"

# 언리얼에 이미 임포트된 상황별 울음 이름이다. 번호가 아니라 의미를 키로 사용해 잘못된 칸에 섞이는 것을 막는다.
SITUATIONAL_CRY_NAMES = {
    "base": "울음_01_기본",
    "happy": ["울음_02_기쁨1", "울음_03_기쁨2", "울음_04_기쁨3"],
    "angry": ["울음_05_분노"],
    "sad": ["울음_06_슬픔"],
    "physical_attack": ["울음_07_물리공격"],
    "special_attack": ["울음_08_특수공격"],
    "special": ["울음_09_특수음성1", "울음_10_특수음성2", "울음_11_특수음성3", "울음_12_특수음성4"],
    "ambient": ["울음_13_환경"],
}


def get_or_create_asset(asset_path, asset_class, factory_class):
    existing = unreal.load_asset(asset_path)
    if existing:
        return existing

    package_path, asset_name = asset_path.rsplit("/", 1)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        package_path,
        asset_class,
        factory_class(),
    )


def build_shared_audio_settings():
    attenuation = get_or_create_asset(
        f"{SHARED_ROOT}/ATT_PokemonCry_3D",
        unreal.SoundAttenuation,
        unreal.SoundAttenuationFactory,
    )
    attenuation_settings = attenuation.get_editor_property("attenuation")
    attenuation_settings.set_editor_property("attenuate", True)
    attenuation_settings.set_editor_property("spatialize", True)
    attenuation_settings.set_editor_property("attenuation_shape_extents", unreal.Vector(250.0, 0.0, 0.0))
    attenuation_settings.set_editor_property("falloff_distance", 3000.0)
    attenuation_settings.set_editor_property("attenuate_with_lpf", False)
    attenuation_settings.set_editor_property("enable_occlusion", False)
    attenuation_settings.set_editor_property("enable_reverb_send", False)
    attenuation_settings.set_editor_property("apply_normalization_to_stereo_sounds", True)
    attenuation_settings.set_editor_property("non_spatialized_radius_start", 0.0)
    attenuation_settings.set_editor_property("non_spatialized_radius_end", 0.0)
    attenuation_settings.set_editor_property("stereo_spread", 0.0)
    attenuation.set_editor_property("attenuation", attenuation_settings)

    concurrency = get_or_create_asset(
        f"{SHARED_ROOT}/SC_PokemonCry",
        unreal.SoundConcurrency,
        unreal.SoundConcurrencyFactory,
    )
    concurrency_settings = concurrency.get_editor_property("concurrency")
    concurrency_settings.set_editor_property("max_count", 8)
    concurrency_settings.set_editor_property("retrigger_time", 0.05)
    # 가까운 울음을 보존하고, 한도를 넘긴 원거리의 오래된 울음부터 정리한다.
    concurrency_settings.set_editor_property(
        "resolution_rule",
        unreal.MaxConcurrentResolutionRule.STOP_FARTHEST_THEN_OLDEST,
    )
    concurrency.set_editor_property("concurrency", concurrency_settings)

    unreal.EditorAssetLibrary.save_loaded_assets([attenuation, concurrency], only_if_is_dirty=False)
    return attenuation, concurrency


def import_sound(source_file, destination_path, destination_name):
    object_path = f"{destination_path}/{destination_name}.{destination_name}"
    existing = unreal.load_asset(object_path)
    if existing:
        return existing

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_file))
    task.set_editor_property("destination_path", destination_path)
    task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    imported_paths = task.get_editor_property("imported_object_paths")
    if not imported_paths:
        raise RuntimeError(f"오디오 임포트 실패: {source_file}")

    sound = unreal.load_asset(imported_paths[0])
    sound.set_editor_property("looping", False)
    sound.set_editor_property("compression_quality", 90)
    unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False)
    return sound


def load_situational_sound(korean_name, asset_name):
    """상황별 울음은 프로젝트 에셋을 직접 참조하며, 누락을 조용히 숨기지 않고 즉시 알려준다."""
    asset_path = f"{CONTENT_ROOT}/Cries/{korean_name}/{asset_name}"
    sound = unreal.load_asset(asset_path)
    if not sound:
        raise RuntimeError(f"상황별 울음 에셋을 찾을 수 없습니다: {asset_path}")
    return sound


def load_situational_group(korean_name, group_name):
    """한 상황에 여러 후보가 있으면 등록된 순서를 유지해 DataAsset 배열로 만든다."""
    return [
        load_situational_sound(korean_name, asset_name)
        for asset_name in SITUATIONAL_CRY_NAMES[group_name]
    ]


def main():
    if not SOURCE_ROOT.is_dir():
        raise RuntimeError(f"울음 원본 폴더가 없습니다: {SOURCE_ROOT}")

    attenuation, concurrency = build_shared_audio_settings()
    assigned = 0
    faint_assigned = 0

    for korean_name, english_name in SPECIES.items():
        normal_source = SOURCE_ROOT / f"{english_name}_Cry.wav"
        if not normal_source.is_file():
            raise RuntimeError(f"일반 울음 원본이 없습니다: {normal_source}")

        destination = f"{CONTENT_ROOT}/Cries/{korean_name}"
        normal_sound = import_sound(normal_source, destination, f"SW_{english_name}_Cry")

        faint_source = SOURCE_ROOT / f"{english_name}_Faint.wav"
        faint_sound = None
        if faint_source.is_file():
            faint_sound = import_sound(faint_source, destination, f"SW_{english_name}_Faint")

        data_asset_path = f"/Game/Pokemon/SpeciesData/{korean_name}/DA_{korean_name}"
        data_asset = unreal.load_asset(data_asset_path)
        if not data_asset:
            raise RuntimeError(f"SpeciesData를 찾을 수 없습니다: {data_asset_path}")

        base_sound = load_situational_sound(korean_name, SITUATIONAL_CRY_NAMES["base"])
        happy_sounds = load_situational_group(korean_name, "happy")
        angry_sounds = load_situational_group(korean_name, "angry")
        sad_sounds = load_situational_group(korean_name, "sad")
        physical_attack_sounds = load_situational_group(korean_name, "physical_attack")
        special_attack_sounds = load_situational_group(korean_name, "special_attack")
        special_sounds = load_situational_group(korean_name, "special")
        ambient_sounds = load_situational_group(korean_name, "ambient")

        # R 소환은 대표 울음·기본 울음·특수음성만 사용한다. 감정이나 공격 소리가 소환 중 튀어나오지 않게 분리한다.
        data_asset.set_editor_property("summon_cry", normal_sound)
        data_asset.set_editor_property("summon_cries", [normal_sound, base_sound] + special_sounds)

        # 야생 랜덤 울음에는 평온하게 들리는 기본·기쁨·특수·환경 후보만 넣는다.
        # 분노·슬픔·공격 후보는 해당 행동이 실제로 일어날 때만 별도 함수로 재생한다.
        data_asset.set_editor_property(
            "wild_cries",
            [base_sound] + happy_sounds + special_sounds + ambient_sounds,
        )
        data_asset.set_editor_property("happy_cries", happy_sounds)
        data_asset.set_editor_property("angry_cries", angry_sounds)
        data_asset.set_editor_property("sad_cries", sad_sounds)
        data_asset.set_editor_property("physical_attack_cries", physical_attack_sounds)
        data_asset.set_editor_property("special_attack_cries", special_attack_sounds)
        data_asset.set_editor_property("special_cries", special_sounds)
        data_asset.set_editor_property("ambient_cries", ambient_sounds)

        # 피카츄처럼 전용 기절음이 있으면 그것만 쓰고, 없는 종은 슬픔 음성을 기절 후보로 사용한다.
        resolved_faint_sounds = [faint_sound] if faint_sound else sad_sounds
        data_asset.set_editor_property("faint_cry", resolved_faint_sounds[0])
        data_asset.set_editor_property("faint_cries", resolved_faint_sounds)
        data_asset.set_editor_property("cry_attenuation", attenuation)
        data_asset.set_editor_property("cry_concurrency", concurrency)
        data_asset.set_editor_property("cry_volume_multiplier", 1.0)
        data_asset.set_editor_property("cry_pitch_multiplier", 1.0)
        unreal.EditorAssetLibrary.save_loaded_asset(data_asset, only_if_is_dirty=False)

        assigned += 1
        faint_assigned += int(bool(resolved_faint_sounds))

    unreal.log_warning(
        f"POKEMON_CRY_IMPORT_OK species={assigned} faint={faint_assigned} situational_per_species=13 "
        f"attenuation={attenuation.get_path_name()} concurrency={concurrency.get_path_name()}"
    )


if __name__ == "__main__":
    main()
