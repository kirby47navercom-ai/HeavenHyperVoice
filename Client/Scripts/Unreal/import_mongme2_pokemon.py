"""푸케문 몽므2의 리타겟 포켓몬을 프로젝트 구조에 맞춰 임포트한다.

이 스크립트는 포켓몬 에셋과 종족 DataAsset만 만든다.
야생 스포너와 DA_PokemonSpeciesCatalog는 의도적으로 수정하지 않는다.
"""

import json
import os
import re
from pathlib import Path

import unreal


# 원본 FBX는 저장소 밖의 대용량 작업 파일이므로 환경 변수로 받는다.
# 예: HHV_MONGME2_SOURCE_ROOT=<Retargeted_FBX 폴더>
SOURCE_ROOT_ENV = "HHV_MONGME2_SOURCE_ROOT"
SOURCE_ROOT_VALUE = os.environ.get(SOURCE_ROOT_ENV, "").strip()
SOURCE_ROOT = Path(SOURCE_ROOT_VALUE).expanduser() if SOURCE_ROOT_VALUE else None

# 생성 중간물과 검증 보고서는 각 팀원의 현재 언리얼 프로젝트 Saved 폴더에 둔다.
PROJECT_SAVED_ROOT = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
)
PREPARED_ANIMATION_ROOT = PROJECT_SAVED_ROOT / "Codex" / "Mongme2Animations"
REPORT_PATH = PROJECT_SAVED_ROOT / "Codex" / "mongme2_import_report.json"

ASSET_ROOT = "/Game/Pokemon/Asset"
DATA_ROOT = "/Game/Pokemon/SpeciesData"
DATA_TEMPLATE = "/Game/Pokemon/SpeciesData/꼬부기/DA_꼬부기"
ABP_TEMPLATE = "/Game/Pokemon/SpeciesData/꼬부기/ABP_꼬부기"
BLEND_TEMPLATE = "/Game/Pokemon/SpeciesData/꼬부기/BS_꼬부기_Locomotion_1D"


# WalkSpeed와 RunSpeed는 BlendSpace의 샘플 좌표이자 AnimBP 재생 기준이다.
# 기존 포켓몬처럼 작은 종은 느리게, 다리가 긴 사족형은 빠르게 시작하되,
# DA와 자식 AnimBP에서 팀원이 언제든 직접 조정할 수 있다.
SPECIES = [
    {
        "Korean": "치코리타",
        "English": "Chikorita",
        "Dex": 152,
        "WalkSpeed": 32.0,
        "RunSpeed": 100.0,
        "Forms": [("Chikorita_pm0152_00_00", "")],
    },
    {
        "Korean": "나무지기",
        "English": "Treecko",
        "Dex": 252,
        "WalkSpeed": 22.0,
        "RunSpeed": 56.0,
        "Forms": [("Treecko_pm0252_00_00", "")],
    },
    {
        "Korean": "나오하",
        "English": "Sprigatito",
        "Dex": 906,
        "WalkSpeed": 43.0,
        "RunSpeed": 75.0,
        "Forms": [("Sprigatito_pm1010_00_00", "")],
    },
    {
        "Korean": "찌르꼬",
        "English": "Starly",
        "Dex": 396,
        "WalkSpeed": 23.0,
        "RunSpeed": 44.0,
        "Forms": [
            ("Starly_Form00_pm0396_00_00", "수컷"),
            ("Starly_Form01_pm0396_01_00", "암컷"),
        ],
    },
    {
        "Korean": "흔들풍손",
        "English": "Drifloon",
        "Dex": 425,
        "WalkSpeed": 30.0,
        "RunSpeed": 75.0,
        "Forms": [("Drifloon_pm0425_00_00", "")],
    },
    {
        "Korean": "콘팡",
        "English": "Venonat",
        "Dex": 48,
        "WalkSpeed": 15.0,
        "RunSpeed": 30.0,
        "Forms": [("Venonat_pm0048_00_00", "")],
    },
    {
        "Korean": "치릴리",
        "English": "Petilil",
        "Dex": 548,
        "WalkSpeed": 23.0,
        "RunSpeed": 52.0,
        "Forms": [("Petilil_pm0548_00_00", "")],
    },
    {
        "Korean": "가디",
        "English": "Growlithe",
        "Dex": 58,
        "WalkSpeed": 84.0,
        "RunSpeed": 220.0,
        "Forms": [("Growlithe_pm0058_00_00", "")],
    },
    {
        "Korean": "도치마론",
        "English": "Chespin",
        "Dex": 650,
        "WalkSpeed": 30.0,
        "RunSpeed": 85.0,
        "Forms": [("Chespin_pm0720_00_00", "")],
    },
    {
        "Korean": "나몰빼미",
        "English": "Rowlet",
        "Dex": 722,
        "WalkSpeed": 15.0,
        "RunSpeed": 42.0,
        "Forms": [("Rowlet_pm0841_00_00", "")],
    },
    {
        "Korean": "영구스",
        "English": "Yungoos",
        "Dex": 734,
        "WalkSpeed": 22.0,
        "RunSpeed": 71.0,
        "Forms": [("Yungoos_pm0826_00_00", "")],
    },
    {
        "Korean": "암멍이",
        "English": "Rockruff",
        "Dex": 744,
        "WalkSpeed": 60.0,
        "RunSpeed": 111.0,
        "Forms": [("Rockruff_pm0828_00_00", "")],
    },
    {
        "Korean": "브케인",
        "English": "Cyndaquil",
        "Dex": 155,
        "WalkSpeed": 33.0,
        "RunSpeed": 77.0,
        "Forms": [("Cyndaquil_pm0155_00_00", "")],
    },
]


# 파일명 뒤쪽의 행동 이름을 SpeciesData의 편집 가능한 애니메이션 변수에 연결한다.
# 같은 행동이 지상형(00000)과 비행형(20000)처럼 여러 개면 숫자가 작은 기본 세트를 쓴다.
ANIMATION_PROPERTIES = {
    "idle": "defaultwait01_loop",
    "battle_idle": "battlewait01_loop",
    "idle01": "defaultidle01",
    "idle02": "defaultidle02",
    "walk": "walk01_loop",
    "run": "run01_loop",
    "turn_left90": "turn_l090",
    "turn_right90": "turn_r090",
    "jump_start": "jumpup01_start",
    "jump_loop": "jumpup01_loop",
    "fall_start": "jumpdown01_start",
    "fall_loop": "jumpdown01_loop",
    "land": "land02",
    "attack01": "attack01",
    "attack02": "attack02",
    "range_attack01": "rangeattack01",
    "range_attack02_start": "rangeattack02_start",
    "range_attack02_loop": "rangeattack02_loop",
    "range_attack02_end": "rangeattack02_end",
    "damage01": "damage01",
    "damage02": "damage02",
    "stun_start": "stun01_start",
    "stun_loop": "stun01_loop",
    "stun_end": "stun01_end",
    "down_start": "down01_start",
    "down_loop": "down01_loop",
    "down_end": "down01_end",
    "eat01_start": "eat01_start",
    "eat01_loop": "eat01_loop",
    "eat01_end": "eat01_end",
    "eat02_start": "eat02_start",
    "eat02_loop": "eat02_loop",
    "eat02_end": "eat02_end",
    "sleep_start": "sleep01_start",
    "sleep_loop": "sleep01_loop",
    "sleep_end": "sleep01_end",
    "rest_start": "rest01_start",
    "rest_loop": "rest01_loop",
    "rest_end": "rest01_end",
    "notice": "notice01",
    "roar": "roar01",
    "glad": "glad01",
    "hate": "hate01",
    "refresh": "refresh01",
    "step_out_start": "stepout01_start",
    "step_out": "stepout01",
    "step_out_end": "stepout01_end",
    "eye": "eye01",
    "mouth": "mouth01",
}


asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
editor_assets = unreal.EditorAssetLibrary


def set_property(obj, name, value):
    """엔진 버전별로 없는 선택 속성은 건너뛰고, 필수 속성은 호출부에서 직접 설정한다."""
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception:
        return False


def load_required(path):
    asset = editor_assets.load_asset(path)
    if not asset:
        raise RuntimeError(f"필수 기준 에셋을 찾을 수 없습니다: {path}")
    return asset


def load_if_exists(path):
    """새 에셋을 처음 만드는 경우는 '없음'이 정상 상태이므로, 로드 오류를 남기지 않는다."""
    if not editor_assets.does_asset_exist(path):
        return None
    return editor_assets.load_asset(path)


def get_skeleton(asset):
    getter = getattr(asset, "get_skeleton", None)
    return getter() if getter else asset.get_editor_property("skeleton")


def import_task(filename, destination, options):
    task = unreal.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = destination
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = False
    task.options = options
    return task


def model_options():
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.import_as_skeletal = True
    options.import_mesh = True
    options.import_animations = False
    options.import_materials = True
    options.import_textures = True
    set_property(options, "create_physics_asset", False)
    set_property(options.skeletal_mesh_import_data, "import_mesh_lods", False)
    set_property(options.skeletal_mesh_import_data, "update_skeleton_reference_pose", False)
    set_property(options.skeletal_mesh_import_data, "use_t0_as_ref_pose", False)
    return options


def animation_options(skeleton):
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.import_as_skeletal = True
    options.import_mesh = False
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.skeleton = skeleton
    set_property(options.anim_sequence_import_data, "import_bone_tracks", True)
    set_property(options.anim_sequence_import_data, "remove_redundant_keys", False)
    set_property(options.anim_sequence_import_data, "import_custom_attribute", True)
    return options


def import_form(species_name, package_name, form_folder):
    source = SOURCE_ROOT / package_name
    if not source.is_dir():
        raise RuntimeError(f"리타겟 폴더를 찾을 수 없습니다: {source}")

    # 폼이 하나뿐이면 불필요한 중간 폴더를 만들지 않고, 실제로 두 폼인 찌르꼬만 분리한다.
    destination_root = f"{ASSET_ROOT}/{species_name}"
    if form_folder:
        destination_root += f"/{form_folder}"
    model_destination = f"{destination_root}/모델"
    animation_destination = f"{destination_root}/애니메이션"
    editor_assets.make_directory(model_destination)
    editor_assets.make_directory(animation_destination)

    model_files = sorted((source / "Models").glob("*_Rigged.fbx"))
    if len(model_files) != 1:
        raise RuntimeError(f"모델 FBX는 정확히 하나여야 합니다: {source} ({len(model_files)}개)")

    model_task = import_task(model_files[0], model_destination, model_options())
    asset_tools.import_asset_tasks([model_task])
    meshes = [obj for obj in model_task.get_objects() if isinstance(obj, unreal.SkeletalMesh)]
    if not meshes:
        expected_mesh = f"{model_destination}/{model_files[0].stem}"
        loaded = editor_assets.load_asset(expected_mesh)
        meshes = [loaded] if isinstance(loaded, unreal.SkeletalMesh) else []
    if len(meshes) != 1:
        raise RuntimeError(f"스켈레탈 메시 임포트 실패: {model_files[0]}")

    mesh = meshes[0]
    skeleton = get_skeleton(mesh)
    if not skeleton:
        raise RuntimeError(f"스켈레톤이 없는 메시입니다: {mesh.get_path_name()}")

    # .blend에서 메시를 제외해 분리한 FBX가 있으면 이 가벼운 파일을 사용한다.
    # 분리 파일이 없는 경우에만 원본 Animations 폴더로 돌아간다.
    prepared_directory = PREPARED_ANIMATION_ROOT / package_name
    animation_source = prepared_directory if prepared_directory.is_dir() else source / "Animations"
    animation_files = sorted(animation_source.glob("*.fbx"))
    tasks = [
        import_task(filename, animation_destination, animation_options(skeleton))
        for filename in animation_files
    ]
    asset_tools.import_asset_tasks(tasks)

    animations = {}
    failures = []
    for filename, task in zip(animation_files, tasks):
        sequences = [obj for obj in task.get_objects() if isinstance(obj, unreal.AnimSequence)]
        if not sequences:
            loaded = editor_assets.load_asset(f"{animation_destination}/{filename.stem}")
            sequences = [loaded] if isinstance(loaded, unreal.AnimSequence) else []
        if len(sequences) != 1:
            failures.append(filename.name)
            continue
        sequence = sequences[0]
        if get_skeleton(sequence) != skeleton:
            failures.append(filename.name + "(스켈레톤 불일치)")
            continue
        set_property(sequence, "enable_root_motion", False)
        set_property(sequence, "force_root_lock", True)
        animations[filename.stem] = sequence

    if failures:
        raise RuntimeError(
            f"{species_name} {form_folder or '기본폼'} 애니메이션 임포트 실패: "
            + ", ".join(failures)
        )

    return {
        "Package": package_name,
        "Form": form_folder or "기본폼",
        "Mesh": mesh,
        "Skeleton": skeleton,
        "Animations": animations,
        "SourceAnimationCount": len(animation_files),
    }


def animation_code(name):
    match = re.search(r"_(\d{5})_[^_].*$", name)
    return int(match.group(1)) if match else 99999


def find_animation(animations, suffix):
    candidates = [
        animation
        for name, animation in animations.items()
        if name.lower().endswith("_" + suffix.lower())
    ]
    if not candidates:
        return None
    return min(candidates, key=lambda item: animation_code(item.get_name()))


def create_blend_space(species, primary_form, blend_template):
    name = species["Korean"]
    data_directory = f"{DATA_ROOT}/{name}"
    asset_name = f"BS_{name}_Locomotion_1D"
    asset_path = f"{data_directory}/{asset_name}"
    editor_assets.make_directory(data_directory)

    blend = load_if_exists(asset_path)
    if blend and get_skeleton(blend) != primary_form["Skeleton"]:
        editor_assets.delete_asset(asset_path)
        blend = None
    if not blend:
        factory = unreal.BlendSpaceFactory1D()
        factory.set_editor_property("target_skeleton", primary_form["Skeleton"])
        set_property(factory, "preview_skeletal_mesh", primary_form["Mesh"])
        blend = asset_tools.create_asset(asset_name, data_directory, unreal.BlendSpace1D, factory)
    if not blend:
        raise RuntimeError(f"BlendSpace 생성 실패: {asset_path}")

    idle = find_animation(primary_form["Animations"], "defaultwait01_loop")
    walk = find_animation(primary_form["Animations"], "walk01_loop")
    run = find_animation(primary_form["Animations"], "run01_loop")
    if not idle or not walk or not run:
        raise RuntimeError(f"Idle/Walk/Run 애니메이션이 부족합니다: {name}")

    parameters = [parameter.copy() for parameter in blend_template.get_editor_property("blend_parameters")]
    parameters[0].set_editor_property("min", 0.0)
    parameters[0].set_editor_property("max", float(species["RunSpeed"]))
    blend.set_editor_property("blend_parameters", parameters)

    sample_values = (0.0, float(species["WalkSpeed"]), float(species["RunSpeed"]))
    samples = []
    for source_sample, animation, speed in zip(
        blend_template.get_editor_property("sample_data"),
        (idle, walk, run),
        sample_values,
    ):
        sample = source_sample.copy()
        sample.set_editor_property("animation", animation)
        sample.set_editor_property("sample_value", unreal.Vector(speed, 0.0, 0.0))
        samples.append(sample)
    blend.set_editor_property("sample_data", samples)
    editor_assets.save_loaded_asset(blend, only_if_is_dirty=False)
    return blend


def create_anim_blueprint(species, primary_form, blend_space):
    name = species["Korean"]
    data_directory = f"{DATA_ROOT}/{name}"
    asset_path = f"{data_directory}/ABP_{name}"
    blueprint = load_if_exists(asset_path)
    if not blueprint:
        blueprint = editor_assets.duplicate_asset(ABP_TEMPLATE, asset_path)
    if not blueprint:
        raise RuntimeError(f"AnimBlueprint 생성 실패: {asset_path}")

    blueprint.set_editor_property("target_skeleton", primary_form["Skeleton"])
    set_property(blueprint, "preview_skeletal_mesh", primary_form["Mesh"])

    # 공통 부모 ABP의 AnimGraph가 LocomotionBlendSpace 변수를 읽는 구조이므로,
    # 자식 ABP에서는 그래프를 복제하지 않고 Class Defaults만 종별 값으로 교체한다.
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    generated_class = blueprint.generated_class()
    if not generated_class:
        raise RuntimeError(f"AnimBlueprint 컴파일 실패: {asset_path}")

    # 부모 ABP가 읽는 기본값을 자식 Class Defaults에 저장한다.
    defaults = unreal.get_default_object(generated_class)
    defaults.set_editor_property("locomotion_blend_space", blend_space)
    defaults.set_editor_property("walk_animation_speed", float(species["WalkSpeed"]))
    defaults.set_editor_property("run_animation_speed", float(species["RunSpeed"]))
    defaults.set_editor_property("max_locomotion_play_rate", 1.5)
    editor_assets.save_loaded_asset(blueprint, only_if_is_dirty=False)
    return blueprint


def create_species_data(species, primary_form, anim_blueprint, data_template):
    name = species["Korean"]
    data_directory = f"{DATA_ROOT}/{name}"
    asset_name = f"DA_{name}"
    asset_path = f"{data_directory}/{asset_name}"
    data = load_if_exists(asset_path)
    if not data:
        data = asset_tools.create_asset(
            asset_name,
            data_directory,
            unreal.UEPokemonSpeciesData,
            unreal.DataAssetFactory(),
        )
    if not data:
        raise RuntimeError(f"SpeciesData 생성 실패: {asset_path}")

    data.set_editor_property("species_id", species["English"])
    data.set_editor_property("dex_number", int(species["Dex"]))
    data.set_editor_property("display_name", name)
    data.set_editor_property("skeletal_mesh", primary_form["Mesh"])
    data.set_editor_property("anim_instance_class", anim_blueprint.generated_class())

    # +X 정면 규칙과 현재 캐릭터 캡슐 기준은 기존 DA에서 그대로 복사한다.
    for property_name in (
        "mesh_relative_transform",
        "capsule_radius",
        "capsule_half_height",
        "max_step_height",
        "walkable_floor_angle_degrees",
        "max_hp",
        "base_attack_power",
        "base_defense",
        "cry_volume_multiplier",
        "cry_pitch_multiplier",
        "wild_cry_min_interval_seconds",
        "wild_cry_max_interval_seconds",
    ):
        data.set_editor_property(property_name, data_template.get_editor_property(property_name))

    data.set_editor_property("move_speed", float(species["RunSpeed"]))
    for property_name, suffix in ANIMATION_PROPERTIES.items():
        data.set_editor_property(
            property_name,
            find_animation(primary_form["Animations"], suffix),
        )

    editor_assets.save_loaded_asset(data, only_if_is_dirty=False)
    return data


def verify_species(species, forms, blend, blueprint, data):
    errors = []
    name = species["Korean"]
    primary = forms[0]
    if data.get_editor_property("skeletal_mesh") != primary["Mesh"]:
        errors.append("DA 메시 불일치")
    if data.get_editor_property("anim_instance_class") != blueprint.generated_class():
        errors.append("DA AnimBP 불일치")
    for required in ("idle", "walk", "run"):
        if not data.get_editor_property(required):
            errors.append(f"DA {required} 비어 있음")
    if get_skeleton(blend) != primary["Skeleton"]:
        errors.append("BlendSpace 스켈레톤 불일치")
    if len(blend.get_editor_property("sample_data")) != 3:
        errors.append("BlendSpace 샘플 개수 불일치")
    if blueprint.get_editor_property("target_skeleton") != primary["Skeleton"]:
        errors.append("AnimBP 스켈레톤 불일치")
    for form in forms:
        if len(form["Animations"]) != form["SourceAnimationCount"]:
            errors.append(f"{form['Form']} 애니메이션 개수 불일치")
    return {
        "Name": name,
        "Dex": species["Dex"],
        "Forms": [
            {
                "Form": form["Form"],
                "Mesh": form["Mesh"].get_path_name(),
                "Skeleton": form["Skeleton"].get_path_name(),
                "Animations": len(form["Animations"]),
            }
            for form in forms
        ],
        "DataAsset": data.get_path_name(),
        "AnimBlueprint": blueprint.get_path_name(),
        "BlendSpace": blend.get_path_name(),
        "Errors": errors,
    }


def run():
    if SOURCE_ROOT is None or not SOURCE_ROOT.is_dir():
        raise RuntimeError(
            f"환경 변수 {SOURCE_ROOT_ENV}에 Retargeted_FBX 폴더를 지정해야 합니다: "
            f"{SOURCE_ROOT_VALUE or '(미설정)'}"
        )

    data_template = load_required(DATA_TEMPLATE)
    load_required(ABP_TEMPLATE)
    blend_template = load_required(BLEND_TEMPLATE)
    results = []

    for species in SPECIES:
        unreal.log(f"[MONGME2] {species['Korean']} 임포트 시작")
        forms = [
            import_form(species["Korean"], package_name, form_folder)
            for package_name, form_folder in species["Forms"]
        ]
        blend = create_blend_space(species, forms[0], blend_template)
        blueprint = create_anim_blueprint(species, forms[0], blend)
        data = create_species_data(species, forms[0], blueprint, data_template)
        result = verify_species(species, forms, blend, blueprint, data)
        results.append(result)
        unreal.log(
            f"[MONGME2] {species['Korean']} 완료 "
            f"폼={len(forms)} 애니메이션={sum(len(form['Animations']) for form in forms)}"
        )
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    report = {
        "Source": str(SOURCE_ROOT),
        "SpeciesCount": len(results),
        "FormCount": sum(len(item["Forms"]) for item in results),
        "AnimationCount": sum(
            form["Animations"] for item in results for form in item["Forms"]
        ),
        "SpawnerModified": False,
        "CatalogModified": False,
        "Species": results,
    }
    report["Errors"] = [
        f"{item['Name']}: {error}"
        for item in results
        for error in item["Errors"]
    ]
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(
        json.dumps(report, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    unreal.log("MONGME2_IMPORT_RESULT=" + json.dumps(report, ensure_ascii=False))
    if report["Errors"]:
        raise RuntimeError("; ".join(report["Errors"]))


run()
