# 파티 화면 배경이 목록을 덮고 있는 것을 내린다.
#
# PanelSurface 캔버스에서 배경(PartyPanel)은 zOrder 1, 목록(PokemonScroll)은
# zOrder 0 이다. 캔버스는 zOrder 가 큰 쪽을 위에 그리고 먼저 히트 테스트한다.
# 배경은 가시성이 Visible 이라 클릭을 먹어 버리고, Border 는 누름을 처리하지
# 않으므로 클릭이 그대로 창 배경까지 흘러간다 -- 목록이 회색으로 죽어 보이고
# 아무리 눌러도 파티에 안 들어가던 원인이다.
#
# 배경을 -1 로 내린다. 가시성은 그대로 둔다 -- 창 안쪽 빈 곳 클릭이 월드로
# 새지 않게 막는 역할은 남겨 둔다.
#
# 실행:
#   UnrealEditor-Cmd.exe HeavenHyperVoice.uproject -run=pythonscript
#       -script="Client/Scripts/Unreal/fix_field_party_panel_zorder.py"
#       -unattended -nosplash
#
# 다시 돌려도 안전하다. 이미 -1 이면 아무것도 하지 않는다.

import unreal

ASSET = "/Game/UI/PokemonParty/WBP_FieldParty"
SURFACE_CHILDREN = [
    "PartyPanel", "TitleText", "GuideText", "PokemonScroll", "StatusText",
    "ResetButton", "CloseButton", "ConfirmButton",
]
BACKGROUND = "PartyPanel"
BACKGROUND_Z = -1


def widget(name):
    return unreal.load_object(None, "{}.{}:WidgetTree.{}".format(ASSET, ASSET.rsplit("/", 1)[1], name))


def report(stage):
    for name in SURFACE_CHILDREN:
        item = widget(name)
        if item is None:
            continue
        slot = item.get_editor_property("slot")
        z = slot.get_editor_property("z_order") if slot else "?"
        unreal.log("[ZFIX] {} {}: z={} vis={}".format(
            stage, name, z, item.get_editor_property("visibility")))


def run():
    blueprint = unreal.load_asset(ASSET)
    if blueprint is None:
        unreal.log_error("[ZFIX] not found: {}".format(ASSET))
        return

    report("before")

    background = widget(BACKGROUND)
    if background is None:
        unreal.log_error("[ZFIX] {} not found".format(BACKGROUND))
        return
    slot = background.get_editor_property("slot")
    if slot is None:
        unreal.log_error("[ZFIX] {} has no canvas slot".format(BACKGROUND))
        return

    if int(slot.get_editor_property("z_order")) == BACKGROUND_Z:
        unreal.log("[ZFIX] already fixed")
        return

    slot.set_editor_property("z_order", BACKGROUND_Z)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)

    report("after")


run()
