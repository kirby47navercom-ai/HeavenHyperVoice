# 뽑기 후보 25종을 종족 카탈로그에 채운다.
#
# 파티 화면은 카탈로그를 훑어 목록을 만든다. 카탈로그는 build_pokemon_catalog.py
# 가 옛 20종 기준으로 만든 것이라, 팀원이 뒤에 추가한 종족과 서버 표에 나중에
# 붙인 12종이 빠져 있다. 그래서 뽑기로 얻을 수 있는데 파티 화면에는 안 보인다.
#
# 도감번호와 속성은 Server/FieldServer/gacha/pools.txt 및 GachaBuild.cpp 의
# 표와 같은 값이다. 세 곳이 어긋나면 뽑기는 되는데 화면에 안 나오거나,
# 화면에는 있는데 서버가 거절하는 상태가 된다.
#
# 실행:
#   UnrealEditor-Cmd.exe HeavenHyperVoice.uproject -run=pythonscript
#       -script="Client/Scripts/Unreal/add_gacha_species_to_catalog.py"
#       -unattended -nosplash
#
# 다시 돌려도 안전하다. 카탈로그를 새로 채우지 않고 **빠진 것만 뒤에 붙인다** --
# 배열 위치를 쓰는 기존 경로가 있어서 순서를 흔들면 안 된다.
# 이미 채워진 도감번호와 속성은 건드리지 않는다.

import unreal

CATALOG_PATH = "/Game/Pokemon/DA_PokemonSpeciesCatalog.DA_PokemonSpeciesCatalog"

# (폴더명, 도감번호, 속성). 속성 이름은 EUEPokemonType 과 같다.
GACHA_SPECIES = [
    ("불꽃숭이", 390, "FIRE"),
    ("영치코", 256, "FIRE"),
    ("폭타", 323, "FIRE"),
    ("파이어로", 663, "FIRE"),
    ("윈디", 59, "FIRE"),

    ("팽도리", 393, "WATER"),
    ("개굴반장", 657, "WATER"),
    ("블로스터", 693, "WATER"),
    ("누오", 195, "WATER"),
    ("갸라도스", 130, "WATER"),

    ("모부기", 387, "GRASS"),
    ("나무돌이", 253, "GRASS"),
    ("버섯모", 286, "GRASS"),
    ("눈설왕", 460, "GRASS"),
    ("드레디어", 549, "GRASS"),

    ("나옹", 52, "NORMAL"),
    ("노고치", 206, "NORMAL"),
    ("게을킹", 289, "NORMAL"),
    ("잠만보", 143, "NORMAL"),
    ("붉은달다투곰", 901, "NORMAL"),

    ("꼬링크", 403, "ELECTRIC"),
    ("데덴네", 702, "ELECTRIC"),
    ("파치리스", 417, "ELECTRIC"),
    ("전룡", 181, "ELECTRIC"),
    ("로토무", 479, "ELECTRIC"),
]

editor_asset = unreal.EditorAssetLibrary


def type_enum(name):
    # 언리얼 파이썬은 UENUM 의 E 접두사를 떼기도 하고 안 떼기도 한다.
    for holder in ("EUEPokemonType", "UEPokemonType"):
        enum = getattr(unreal, holder, None)
        if enum is not None:
            return getattr(enum, name)
    raise RuntimeError("EUEPokemonType is not exposed to Python")


def da_path(folder):
    return "/Game/Pokemon/SpeciesData/{n}/DA_{n}.DA_{n}".format(n=folder)


def run():
    catalog = unreal.load_asset(CATALOG_PATH)
    if catalog is None:
        unreal.log_error("[GACHA] catalog not found: {}".format(CATALOG_PATH))
        return

    species = list(catalog.get_editor_property("species"))
    present = set(s.get_name() for s in species if s)

    added, fixed, missing = [], [], []

    for folder, dex, type_name in GACHA_SPECIES:
        data = unreal.load_asset(da_path(folder))
        if data is None:
            missing.append(folder)
            continue

        # 도감번호와 속성이 비어 있으면 채운다. 이미 값이 있으면 그대로 둔다 --
        # 팀원이 손으로 정한 값을 덮지 않는다.
        changed = False
        if int(data.get_editor_property("dex_number")) != dex:
            if int(data.get_editor_property("dex_number")) == 0:
                data.set_editor_property("dex_number", dex)
                changed = True
            else:
                unreal.log_warning(
                    "[GACHA] {} has dex {} but the gacha table says {}; left alone".format(
                        folder, data.get_editor_property("dex_number"), dex))

        if data.get_editor_property("pokemon_type") == type_enum("NONE"):
            data.set_editor_property("pokemon_type", type_enum(type_name))
            changed = True

        if changed:
            editor_asset.save_loaded_asset(data)
            fixed.append(folder)

        if data.get_name() not in present:
            # 뒤에만 붙인다. 배열 위치를 쓰는 경로가 남아 있어서 중간에 끼우면
            # 이미 저장된 것이 다른 종족을 가리킨다.
            species.append(data)
            present.add(data.get_name())
            added.append(folder)

    if added:
        catalog.set_editor_property("species", species)
        editor_asset.save_loaded_asset(catalog)

    unreal.log("[GACHA] catalog entries: {}".format(len(species)))
    unreal.log("[GACHA] added: {}".format(", ".join(added) if added else "(none)"))
    unreal.log("[GACHA] dex/type filled: {}".format(", ".join(fixed) if fixed else "(none)"))
    if missing:
        unreal.log_warning("[GACHA] DA not found: {}".format(", ".join(missing)))


run()
