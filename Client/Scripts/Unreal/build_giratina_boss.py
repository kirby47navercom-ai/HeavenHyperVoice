import unreal


BOSS_ROOT = "/Game/Pokemon/Boss/기라티나"
DATA_FOLDER = f"{BOSS_ROOT}/Data"
BLUEPRINT_FOLDER = f"{BOSS_ROOT}/Blueprints"
DATA_ASSET_PATH = f"{DATA_FOLDER}/DA_기라티나_Boss"
BLUEPRINT_PATH = f"{BLUEPRINT_FOLDER}/BP_기라티나_Boss"

ANOTHER_ROOT = "/Game/Pokemon/Asset/기라티나/어나더폼"
ORIGIN_ROOT = "/Game/Pokemon/Asset/기라티나/오리진폼"

# 보스전에서 의미가 있는 동작만 전체 라이브러리에 넣는다.
# eat, sleep, rest는 일반 필드 생활 동작이므로 보스 데이터에서 의도적으로 제외한다.
BOSS_ANIMATION_TOKENS = (
    "defaultwait",
    "defaultidle",
    "battlewait",
    "walk",
    "run",
    "turn_",
    "stepout",
    "jumpup",
    "jumpdown",
    "land",
    "notice",
    "roar",
    "hate",
    "refresh",
    "glad",
    "attack",
    "damage",
    "stun",
    "down",
    "eye",
    "mouth",
)


def require_asset(asset_path):
    asset = unreal.load_asset(asset_path)
    if asset is None:
        raise RuntimeError(f"필수 에셋을 찾지 못했습니다: {asset_path}")
    return asset


def list_animation_paths(animation_folder):
    paths = unreal.EditorAssetLibrary.list_assets(animation_folder, recursive=False, include_folder=False)
    return sorted(path for path in paths if "AnimSequence" in str(type(unreal.load_asset(path))))


def get_animation_code(asset_path):
    # pm0487_11_00_00560_notice01에서 00560 부분을 가져온다.
    asset_name = asset_path.rsplit("/", 1)[-1].split(".", 1)[0]
    name_parts = asset_name.split("_")
    return name_parts[3] if len(name_parts) > 4 else ""


def choose_animation(animation_paths, suffix, preferred_group):
    # 어나더폼은 0xxxx, 오리진폼은 2xxxx 계열을 대표 동작으로 우선 선택한다.
    candidates = [path for path in animation_paths if path.split(".", 1)[0].endswith(suffix)]
    candidates.sort(key=lambda path: (not get_animation_code(path).startswith(preferred_group), path))
    return unreal.load_asset(candidates[0]) if candidates else None


def collect_boss_library(animation_paths):
    result = []
    for path in animation_paths:
        asset_name = path.rsplit("/", 1)[-1].lower()
        if any(token in asset_name for token in BOSS_ANIMATION_TOKENS):
            animation = unreal.load_asset(path)
            if animation is not None:
                result.append(animation)
    return result


def make_animation_set(animation_paths, preferred_group):
    animations = unreal.UEPokemonBossAnimationSet()

    # 등장과 전투 개시 동작
    animations.set_editor_property("entrance_start", choose_animation(animation_paths, "_stepout01_start", preferred_group))
    animations.set_editor_property("entrance_loop", choose_animation(animation_paths, "_stepout01", preferred_group))
    animations.set_editor_property("entrance_end", choose_animation(animation_paths, "_stepout01_end", preferred_group))
    animations.set_editor_property("notice", choose_animation(animation_paths, "_notice01", preferred_group))
    animations.set_editor_property("roar", choose_animation(animation_paths, "_roar01", preferred_group))
    animations.set_editor_property("aggro", choose_animation(animation_paths, "_hate01", preferred_group))
    animations.set_editor_property("phase_transition", choose_animation(animation_paths, "_refresh01", preferred_group))
    animations.set_editor_property("victory", choose_animation(animation_paths, "_glad01", preferred_group))

    # 정지와 이동 동작
    animations.set_editor_property("idle", choose_animation(animation_paths, "_defaultwait01_loop", preferred_group))
    animations.set_editor_property("idle01", choose_animation(animation_paths, "_defaultidle01", preferred_group))
    animations.set_editor_property("idle02", choose_animation(animation_paths, "_defaultidle02", preferred_group))
    battle_idle = choose_animation(animation_paths, "_battlewait01_loop", preferred_group)
    if battle_idle is None:
        # 어나더폼 원본에는 battlewait01_loop가 없으므로 기본 반복 대기를 안전한 대체값으로 쓴다.
        battle_idle = choose_animation(animation_paths, "_defaultwait01_loop", preferred_group)
    animations.set_editor_property("battle_idle", battle_idle)
    animations.set_editor_property("walk", choose_animation(animation_paths, "_walk01_loop", preferred_group))
    animations.set_editor_property("run", choose_animation(animation_paths, "_run01_loop", preferred_group))
    animations.set_editor_property("turn_left90", choose_animation(animation_paths, "_turn_l090", preferred_group))
    animations.set_editor_property("turn_right90", choose_animation(animation_paths, "_turn_r090", preferred_group))

    # 공중 동작
    animations.set_editor_property("jump_start", choose_animation(animation_paths, "_jumpup01_start", preferred_group))
    animations.set_editor_property("jump_loop", choose_animation(animation_paths, "_jumpup01_loop", preferred_group))
    animations.set_editor_property("fall_start", choose_animation(animation_paths, "_jumpdown01_start", preferred_group))
    animations.set_editor_property("fall_loop", choose_animation(animation_paths, "_jumpdown01_loop", preferred_group))
    animations.set_editor_property("land", choose_animation(animation_paths, "_land02", preferred_group))

    # 공격 동작
    animations.set_editor_property("attack01", choose_animation(animation_paths, "_attack01", preferred_group))
    animations.set_editor_property("attack02", choose_animation(animation_paths, "_attack02", preferred_group))
    animations.set_editor_property("range_attack01", choose_animation(animation_paths, "_rangeattack01", preferred_group))
    animations.set_editor_property("range_attack02_start", choose_animation(animation_paths, "_rangeattack02_start", preferred_group))
    animations.set_editor_property("range_attack02_loop", choose_animation(animation_paths, "_rangeattack02_loop", preferred_group))
    animations.set_editor_property("range_attack02_end", choose_animation(animation_paths, "_rangeattack02_end", preferred_group))

    # 피격, 그로기, 격파 동작
    animations.set_editor_property("damage01", choose_animation(animation_paths, "_damage01", preferred_group))
    animations.set_editor_property("damage02", choose_animation(animation_paths, "_damage02", preferred_group))
    animations.set_editor_property("stun_start", choose_animation(animation_paths, "_stun01_start", preferred_group))
    animations.set_editor_property("stun_loop", choose_animation(animation_paths, "_stun01_loop", preferred_group))
    animations.set_editor_property("stun_end", choose_animation(animation_paths, "_stun01_end", preferred_group))
    animations.set_editor_property("defeat_start", choose_animation(animation_paths, "_down01_start", preferred_group))
    animations.set_editor_property("defeat_loop", choose_animation(animation_paths, "_down01_loop", preferred_group))
    animations.set_editor_property("defeat_end", choose_animation(animation_paths, "_down01_end", preferred_group))

    # 얼굴 보조 동작과 번호가 다른 모든 보스 동작을 함께 보관한다.
    animations.set_editor_property("eye", choose_animation(animation_paths, "_eye01", preferred_group))
    animations.set_editor_property("mouth", choose_animation(animation_paths, "_mouth01", preferred_group))
    animations.set_editor_property("boss_animation_library", collect_boss_library(animation_paths))
    return animations


def make_form(form_id, display_name, asset_root, mesh_name, preferred_group, anim_class=None):
    animation_paths = list_animation_paths(f"{asset_root}/애니메이션")
    if not animation_paths:
        raise RuntimeError(f"애니메이션이 비어 있습니다: {asset_root}/애니메이션")

    form = unreal.UEPokemonBossFormData()
    form.set_editor_property("form_id", form_id)
    form.set_editor_property("display_name", display_name)
    form.set_editor_property("skeletal_mesh", require_asset(f"{asset_root}/모델/{mesh_name}"))
    form.set_editor_property("anim_instance_class", anim_class)

    # 기존 기라티나 필드 데이터와 같은 높이를 유지해 메시가 바닥에서 뜨지 않게 한다.
    form.set_editor_property(
        "mesh_relative_transform",
        unreal.Transform(location=unreal.Vector(0.0, 0.0, -90.0)),
    )
    form.set_editor_property("animations", make_animation_set(animation_paths, preferred_group))
    return form


def create_or_load_boss_data(asset_tools):
    boss_data = unreal.load_asset(DATA_ASSET_PATH)
    if boss_data is not None:
        return boss_data

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.UEPokemonBossData)
    return asset_tools.create_asset(
        "DA_기라티나_Boss",
        DATA_FOLDER,
        unreal.UEPokemonBossData,
        factory,
    )


def create_or_load_boss_blueprint(asset_tools):
    boss_blueprint = unreal.load_asset(BLUEPRINT_PATH)
    if boss_blueprint is not None:
        return boss_blueprint

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.UEPokemonBossCharacter)
    return asset_tools.create_asset(
        "BP_기라티나_Boss",
        BLUEPRINT_FOLDER,
        unreal.Blueprint,
        factory,
    )


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    another_anim_class = unreal.EditorAssetLibrary.load_blueprint_class(
        "/Game/Pokemon/SpeciesData/기라티나/ABP_기라티나"
    )
    if another_anim_class is None:
        raise RuntimeError("어나더폼용 ABP_기라티나 클래스를 찾지 못했습니다.")

    another_form = make_form(
        "AnotherForm",
        "어나더폼",
        ANOTHER_ROOT,
        "Giratina_Form11_pm0487_11_00_pm0487_11_00_Rigged",
        "0",
        another_anim_class,
    )
    origin_form = make_form(
        "OriginForm",
        "오리진폼",
        ORIGIN_ROOT,
        "Giratina_Form12_pm0487_12_00_pm0487_12_00_Rigged",
        "2",
        None,
    )

    boss_data = create_or_load_boss_data(asset_tools)
    if boss_data is None:
        raise RuntimeError("기라티나 보스 데이터 에셋 생성에 실패했습니다.")

    boss_data.set_editor_property("boss_id", "Giratina")
    boss_data.set_editor_property("display_name", "기라티나")
    boss_data.set_editor_property("default_form_id", "AnotherForm")
    boss_data.set_editor_property("forms", [another_form, origin_form])
    boss_data.set_editor_property("max_hp", 5000.0)
    boss_data.set_editor_property("capsule_radius", 120.0)
    boss_data.set_editor_property("capsule_half_height", 160.0)
    boss_data.set_editor_property("move_speed", 180.0)
    if not unreal.EditorAssetLibrary.save_loaded_asset(boss_data, False):
        raise RuntimeError("기라티나 보스 데이터 에셋 저장에 실패했습니다.")

    boss_blueprint = create_or_load_boss_blueprint(asset_tools)
    if boss_blueprint is None:
        raise RuntimeError("기라티나 보스 블루프린트 생성에 실패했습니다.")

    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(BLUEPRINT_PATH)
    if generated_class is None:
        raise RuntimeError("BP_기라티나_Boss 생성 클래스를 불러오지 못했습니다.")

    boss_default = unreal.get_default_object(generated_class)
    boss_default.set_editor_property("boss_data", boss_data)
    boss_default.set_editor_property("initial_form_id", "AnotherForm")
    boss_default.apply_boss_data()
    boss_blueprint.modify()
    if not unreal.EditorAssetLibrary.save_loaded_asset(boss_blueprint, False):
        raise RuntimeError("기라티나 보스 블루프린트 저장에 실패했습니다.")

    forms = boss_data.get_editor_property("forms")
    library_counts = [
        len(form.get_editor_property("animations").get_editor_property("boss_animation_library"))
        for form in forms
    ]
    print(
        "GIRATINA_BOSS_BUILD_OK="
        + str(
            {
                "data_asset": boss_data.get_path_name(),
                "blueprint": boss_blueprint.get_path_name(),
                "forms": [str(form.form_id) for form in forms],
                "library_counts": library_counts,
                "ai_controller": str(boss_default.get_editor_property("ai_controller_class")),
                "auto_possess_ai": str(boss_default.get_editor_property("auto_possess_ai")),
            }
        )
    )


if __name__ == "__main__":
    main()
