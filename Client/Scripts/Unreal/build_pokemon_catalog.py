# 종족 에셋을 UEPokemonSpeciesData 로 감싸고 카탈로그에 꽂는다.
#
# 팀원이 Content/Pokemon/Asset/<종족>/모델/*_Rigged 로 넣은 스켈레탈 메시를
# 종족 id 로 찾을 수 있게 DA 를 하나씩 만들고, DA_PokemonSpeciesCatalog 에
# 20칸을 채운다. 서버가 보낸 종족 번호(1~20)로 이 카탈로그를 조회한다.
#
# 실행:
#   UnrealEditor-Cmd.exe HeavenHyperVoice.uproject -run=pythonscript
#       -script="Client/Scripts/Unreal/build_pokemon_catalog.py" -unattended -nosplash
#
# 다시 돌려도 안전하다. 이미 있는 DA 는 메시만 다시 물리고 덮어쓴다.

import unreal

# (종족 id, 스켈레탈 메시의 오브젝트 경로). 순서는 Server/Protocol/PokemonSpecies.h
# 의 kSpecies 와 같다. 폼/성별이 나뉜 종은 기본형(수컷/일반폼/어나더폼)을 골랐다.
MESH_BY_ID = [
    (1,  "/Game/Pokemon/Asset/귀뚤뚜기/수컷/모델/Kricketot_pm0401_00_00_Rigged"),
    (2,  "/Game/Pokemon/Asset/기라티나/어나더폼/모델/Giratina_Form11_pm0487_11_00_pm0487_11_00_Rigged"),
    (3,  "/Game/Pokemon/Asset/꼬링크/수컷/모델/Shinx_pm0403_00_00_Rigged"),
    (4,  "/Game/Pokemon/Asset/꼬부기/모델/Squirtle_pm0007_00_00_Rigged"),
    (5,  "/Game/Pokemon/Asset/꽁어름/모델/Bergmite_pm0749_00_00_Rigged"),
    (6,  "/Game/Pokemon/Asset/디아루가/일반폼/모델/Dialga_Form11_pm0483_11_00_pm0483_11_00_Rigged"),
    (7,  "/Game/Pokemon/Asset/랄토스/모델/Ralts_pm0280_00_00_Rigged"),
    (8,  "/Game/Pokemon/Asset/모부기/모델/Turtwig_pm0387_00_00_Rigged"),
    (9,  "/Game/Pokemon/Asset/벼리짱/모델/Tinkatuff_pm1105_00_00_Rigged"),
    (10, "/Game/Pokemon/Asset/불꽃숭이/모델/Chimchar_pm0390_00_00_Rigged"),
    (11, "/Game/Pokemon/Asset/아르세우스/모델/Arceus_Form11_pm0493_11_00_pm0493_11_00_Rigged"),
    (12, "/Game/Pokemon/Asset/이브이/모델/Eevee_pm0133_00_00_Rigged"),
    (13, "/Game/Pokemon/Asset/이상해씨/모델/Bulbasaur_pm0001_00_00_Rigged"),
    (14, "/Game/Pokemon/Asset/자망칼/모델/Pawniard_pm0624_00_00_Rigged"),
    (15, "/Game/Pokemon/Asset/터검니/모델/Axew_pm0610_00_00_Rigged"),
    (16, "/Game/Pokemon/Asset/파이리/모델/Charmander_pm0004_00_00_Rigged"),
    (17, "/Game/Pokemon/Asset/파치리스/수컷/모델/Pachirisu_pm0417_00_00_Rigged"),
    (18, "/Game/Pokemon/Asset/팽도리/모델/Piplup_pm0393_00_00_Rigged"),
    (19, "/Game/Pokemon/Asset/펄기아/일반폼/모델/Palkia_Form11_pm0484_11_00_pm0484_11_00_Rigged"),
    (20, "/Game/Pokemon/Asset/피카츄/모델/Pikachu_Default_pm0025_00_00_pm0025_00_00_Rigged"),
]

DATA_DIR = "/Game/Pokemon/Data"
CATALOG_DIR = "/Game/Pokemon"
CATALOG_NAME = "DA_PokemonSpeciesCatalog"  # UEPokemonSpeciesCatalog::CatalogAssetPath 와 일치

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
editor_asset = unreal.EditorAssetLibrary


def object_path(package_path):
    # 스켈레탈 메시는 파일명과 오브젝트명이 같으므로 Package.Object 로 만든다.
    name = package_path.rsplit("/", 1)[-1]
    return "{}.{}".format(package_path, name)


def make_or_load(package_path, name, cls):
    full = "{}/{}".format(package_path, name)
    if editor_asset.does_asset_exist(full):
        return editor_asset.load_asset(full)
    factory = unreal.DataAssetFactory()
    return asset_tools.create_asset(name, package_path, cls, factory)


def run():
    catalog_entries = []
    made, missing = 0, []

    for species_id, mesh_pkg in MESH_BY_ID:
        mesh = unreal.load_asset(object_path(mesh_pkg))
        if mesh is None:
            unreal.log_warning("[CATALOG] mesh not found for id {}: {}".format(species_id, mesh_pkg))
            missing.append(species_id)
            catalog_entries.append(None)
            continue

        da_name = "DA_Species_{:02d}".format(species_id)
        data = make_or_load(DATA_DIR, da_name, unreal.UEPokemonSpeciesData)
        # 서버는 종족을 번호로만 보낸다. SpeciesId 를 그 번호로 박아 두면
        # SetWildSpecies 가 넣는 이름(번호 문자열)과 어긋나지 않는다.
        data.set_editor_property("species_id", unreal.Name(str(species_id)))
        data.set_editor_property("skeletal_mesh", mesh)
        editor_asset.save_loaded_asset(data)
        catalog_entries.append(data)
        made += 1

    catalog = make_or_load(CATALOG_DIR, CATALOG_NAME, unreal.UEPokemonSpeciesCatalog)
    catalog.set_editor_property("species", catalog_entries)
    editor_asset.save_loaded_asset(catalog)

    unreal.log("[CATALOG] species data made/updated: {}".format(made))
    unreal.log("[CATALOG] catalog entries: {}".format(len(catalog_entries)))
    if missing:
        unreal.log_warning("[CATALOG] missing meshes for ids: {}".format(missing))
    unreal.log("[CATALOG] done -> {}/{}".format(CATALOG_DIR, CATALOG_NAME))


run()
