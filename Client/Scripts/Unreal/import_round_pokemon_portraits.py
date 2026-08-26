"""원형 포켓몬 초상화를 언리얼에 임포트하고 종족 데이터에 연결한다."""

from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve().parent
SOURCE_ROOT = PROJECT_ROOT / "Client" / "SourceArt" / "Pokemon" / "Portraits"

SPECIES_NAMES = (
    "귀뚤뚜기", "기라티나", "꼬링크", "꼬부기", "꽁어름",
    "디아루가", "랄토스", "모부기", "벼리짱", "불꽃숭이",
    "아르세우스", "이브이", "이상해씨", "자망칼", "터검니",
    "파이리", "파치리스", "팽도리", "펄기아", "피카츄",
)


def import_portrait(species_name: str):
    source_path = SOURCE_ROOT / species_name / f"T_{species_name}_Portrait.png"
    destination_path = f"/Game/UI/PokemonParty/Portraits/{species_name}"
    destination_name = f"T_{species_name}_Portrait"
    if not source_path.is_file():
        raise FileNotFoundError(f"원형 초상화 PNG를 찾지 못함: {source_path}")

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_path))
    task.set_editor_property("destination_path", destination_path)
    task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture_path = f"{destination_path}/{destination_name}"
    texture = unreal.EditorAssetLibrary.load_asset(texture_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Texture2D 임포트 실패: {texture_path}")

    # 원형 가장자리의 반투명 픽셀이 정확히 보이도록 UI 압축과 sRGB를 사용한다.
    texture.set_editor_property("srgb", True)
    texture.set_editor_property(
        "compression_settings",
        unreal.TextureCompressionSettings.TC_EDITOR_ICON,
    )
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property(
        "mip_gen_settings",
        unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
    )
    texture.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


for species_name in SPECIES_NAMES:
    portrait_texture = import_portrait(species_name)
    data_path = f"/Game/Pokemon/SpeciesData/{species_name}/DA_{species_name}"
    species_data = unreal.EditorAssetLibrary.load_asset(data_path)
    if species_data is None:
        raise RuntimeError(f"종족 데이터 에셋을 찾지 못함: {data_path}")

    # 런타임 경로 검색을 쓰지 않고 DataAsset 변수가 Texture2D를 직접 소유한다.
    species_data.set_editor_property("profile_icon", portrait_texture)
    unreal.EditorAssetLibrary.save_loaded_asset(species_data, only_if_is_dirty=False)

unreal.log("[ROUND PORTRAIT] 원형 포켓몬 초상화 20종 임포트 및 연결 완료")
