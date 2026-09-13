# 파티 화면 위젯들의 가시성·활성 상태를 찍는다.
#
# 클릭이 칸 버튼까지 도달하지 않는 원인을 찾으려고 만들었다. 실행 로그상
# 버튼은 enabled=1, visibility=Visible 인데도 누름이 창 배경까지 흘러내린다 --
# 그 사이 어딘가가 히트 테스트를 막고 있다는 뜻이다. 그 값은 코드가 아니라
# 에셋에 있어서 여기서 본다.
#
# WidgetTree 는 파이썬에 노출되지 않아 트리를 타고 내려갈 수 없다. 대신 위젯을
# 이름으로 하나씩 연다 -- 에셋 안에서 <에셋경로>:WidgetTree.<이름> 이다.
#
# 실행:
#   UnrealEditor-Cmd.exe HeavenHyperVoice.uproject -run=pythonscript
#       -script="Client/Scripts/Unreal/dump_party_widget_tree.py" -unattended -nosplash

import unreal

FOLDER = "/Game/UI/PokemonParty"

TARGETS = [
    ("WBP_FieldParty", [
        "ScreenRoot", "ResponsiveScale", "PanelSize", "PanelSurface", "PartyPanel",
        "TitleText", "GuideText", "PokemonScroll", "PokemonList", "StatusText",
        "ResetButton", "ResetButtonLabel", "CloseButton", "CloseButtonLabel",
        "ConfirmButton", "ConfirmButtonLabel",
    ]),
    ("WBP_FieldPartyEntry", [
        "EntrySize", "SelectionBorder", "EntryOverlay", "SelectButton", "EntryContent",
        "IconImage", "LabelText", "BadgeSize", "SlotBadge", "SlotBadgeText",
    ]),
]


def prop(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return "?"


def run():
    for asset, names in TARGETS:
        path = "{}/{}.{}".format(FOLDER, asset, asset)
        unreal.log("[TREE] ===== {} =====".format(asset))

        generated = unreal.load_object(None, path + "_C")
        if generated:
            # 위젯 자신의 가시성은 트리가 아니라 클래스 기본값에 있다. 여기가
            # HitTestInvisible 이면 안쪽 버튼이 켜져 있어도 클릭을 못 받는다.
            cdo = unreal.get_default_object(generated)
            unreal.log("[TREE]   <self>  vis={}  enabled={}  opacity={}".format(
                prop(cdo, "visibility"), prop(cdo, "is_enabled"), prop(cdo, "render_opacity")))

        for name in names:
            widget = unreal.load_object(None, "{}:WidgetTree.{}".format(path, name))
            if widget is None:
                unreal.log("[TREE]   {}: (없음)".format(name))
                continue
            unreal.log("[TREE]   {}  {}  vis={}  enabled={}  opacity={}".format(
                name, widget.get_class().get_name(),
                prop(widget, "visibility"), prop(widget, "is_enabled"),
                prop(widget, "render_opacity")))


run()
