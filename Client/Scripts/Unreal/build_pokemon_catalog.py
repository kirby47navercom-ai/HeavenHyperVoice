# 팀원이 만든 종족 DA 를 카탈로그에 종족 순서대로 꽂는다.
#
# 팀원은 Content/Pokemon/SpeciesData/<종족>/DA_<종족> 에 종족 데이터를 만들어
# 뒀다. 메시뿐 아니라 ABP_<종족>(애니메이션)까지 물려 있어, 이걸 그대로 쓰면
# 야생·동행이 걷기 애니메이션까지 나온다. 서버가 보낸 종족 번호(1~20)로
# 이 카탈로그를 조회한다.
#
# 실행:
#   UnrealEditor-Cmd.exe HeavenHyperVoice.uproject -run=pythonscript
#       -script="Client/Scripts/Unreal/build_pokemon_catalog.py" -unattended -nosplash
#
# 다시 돌려도 안전하다. 카탈로그를 매번 새로 채운다.

import unreal

# 종족 id(1~20) -> 팀원 DA 가 있는 종족 폴더명. 순서는 Server/Protocol/
# PokemonSpecies.h 의 kSpecies 와 같다. 전설 4종(기라티나·디아루가·아르세우스·
# 펄기아)은 아직 DA 가 없어 비운다 — 그 칸은 런타임에서 색 큐브로 폴백된다.
NAME_BY_ID = {
    1:  "귀뚤뚜기",
    3:  "꼬링크",
    4:  "꼬부기",
    5:  "꽁어름",
    7:  "랄토스",
    8:  "모부기",
    9:  "벼리짱",
    10: "불꽃숭이",
    12: "이브이",
    13: "이상해씨",
    14: "자망칼",
    15: "터검니",
    16: "파이리",
    17: "파치리스",
    18: "팽도리",
    20: "피카츄",
}

CATALOG_DIR = "/Game/Pokemon"
CATALOG_NAME = "DA_PokemonSpeciesCatalog"  # UEPokemonSpeciesCatalog::CatalogAssetPath 와 일치

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
editor_asset = unreal.EditorAssetLibrary


def team_da_path(name):
    return "/Game/Pokemon/SpeciesData/{n}/DA_{n}.DA_{n}".format(n=name)


def make_or_load(package_path, name, cls):
    full = "{}/{}".format(package_path, name)
    if editor_asset.does_asset_exist(full):
        return editor_asset.load_asset(full)
    return asset_tools.create_asset(name, package_path, cls, unreal.DataAssetFactory())


def run():
    entries = []
    linked, empty = 0, []
    for species_id in range(1, 21):
        name = NAME_BY_ID.get(species_id)
        if not name:
            entries.append(None)
            empty.append(species_id)
            continue
        da = unreal.load_asset(team_da_path(name))
        if da is None:
            unreal.log_warning("[CATALOG] DA not found: {}".format(team_da_path(name)))
            entries.append(None)
            empty.append(species_id)
            continue
        entries.append(da)
        linked += 1

    catalog = make_or_load(CATALOG_DIR, CATALOG_NAME, unreal.UEPokemonSpeciesCatalog)
    catalog.set_editor_property("species", entries)
    editor_asset.save_loaded_asset(catalog)

    # 예전에 내가 임시로 만든 메시만 든 DA 를 지운다. 팀원 DA(애니메이션 포함)로
    # 대체됐으므로 남겨두면 헷갈린다.
    removed = 0
    for i in range(1, 21):
        stale = "/Game/Pokemon/Data/DA_Species_{:02d}".format(i)
        if editor_asset.does_asset_exist(stale):
            editor_asset.delete_asset(stale)
            removed += 1
    if editor_asset.does_directory_exist("/Game/Pokemon/Data"):
        if not editor_asset.list_assets("/Game/Pokemon/Data", recursive=True):
            editor_asset.delete_directory("/Game/Pokemon/Data")

    unreal.log("[CATALOG] linked team DA: {}  empty(legendary): {}".format(linked, empty))
    unreal.log("[CATALOG] removed stale DA_Species: {}".format(removed))
    unreal.log("[CATALOG] done -> {}/{}".format(CATALOG_DIR, CATALOG_NAME))


run()
