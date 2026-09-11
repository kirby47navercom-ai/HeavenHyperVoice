"""Author the options menu as editable UMG assets. Run inside the UE editor.

This creates new assets only; it never replaces an existing menu or modifies a map.
The resulting WBP assets work without this authoring script at runtime.
"""

import asyncio
import json
from pathlib import Path

import unreal
from toolset_registry._registry_interface import execute_tool

FOLDER = "/Game/UI/Options"
UMG = "UMGToolSet.UMGToolSet"
OBJECTS = "editor_toolset.toolsets.object.ObjectTools"
ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = ROOT / "Saved/Codex/Options/build_report.json"
FONT = "/Engine/EngineFonts/Roboto.Roboto"
ART = FOLDER + "/Art/T_OptionsJourney.T_OptionsJourney"


def color(r, g, b, a=1.0):
    return dict(r=r, g=g, b=b, a=a)


IVORY = color(.91, .89, .83)
PAPER = color(.975, .965, .925)
INK = color(.033, .073, .080)
MUTED = color(.20, .28, .28)
TEAL = color(.055, .27, .245)
GOLD = color(.57, .40, .19)
WHITE = color(1, 1, 1)
CLEAR = color(0, 0, 0, 0)


def ref(path):
    return {"refPath": path or ""}


def path_of(value):
    return value.get("refPath", "") if isinstance(value, dict) else ""


def slate(c):
    return {"specifiedColor": c, "colorUseRule": "UseColor_Specified"}


def margin(l=0, t=0, r=0, b=0):
    return dict(left=float(l), top=float(t), right=float(r), bottom=float(b))


def brush(c, radius=12, outline=CLEAR, width=0):
    return {
        "drawAs": "RoundedBox", "tintColor": slate(c), "imageType": "NoImage",
        "outlineSettings": {
            "cornerRadii": dict(x=radius, y=radius, z=radius, w=radius),
            "roundingType": "FixedRadius", "color": slate(outline),
            "width": width, "bUseBrushTransparency": False,
        },
    }


async def call(toolset, name, args):
    result = await execute_tool(toolset, name, args)
    if result.get("error"):
        raise RuntimeError(f"{name}: {result['error']}")
    return result.get("returnValue", result)


async def props(obj, values):
    await call(OBJECTS, "set_properties", {"instance": ref(obj), "values": json.dumps(values, ensure_ascii=False)})


async def add(bp, cls, name, parent=None, values=None, rect=None, z=0):
    item = await call(UMG, "AddWidget", {
        "widgetBlueprint": ref(bp), "widgetClass": ref(cls if cls.startswith("/") else "/Script/UMG." + cls),
        "widgetDisplayName": name, "parentWidget": ref(path_of(parent["widget"]) if parent else ""),
        "childIndex": -1,
    })
    if not path_of(item.get("widget")):
        raise RuntimeError(f"Could not create {name}: {item}")
    if values:
        await props(path_of(item["widget"]), values)
    if rect:
        await canvas(item, rect, z=z)
    return item


async def canvas(item, rect, anchors=(0, 0, 0, 0), alignment=(0, 0), z=0):
    await props(path_of(item["slot"]), {
        "layoutData": {"offsets": margin(*rect),
                       "anchors": {"minimum": dict(x=anchors[0], y=anchors[1]), "maximum": dict(x=anchors[2], y=anchors[3])},
                       "alignment": dict(x=alignment[0], y=alignment[1])},
        "bAutoSize": False, "zOrder": z,
    })


async def text(bp, parent, name, label, rect, size=18, tint=INK, bold=False, align="Left"):
    return await add(bp, "TextBlock", name, parent, {
        "text": label, "colorAndOpacity": slate(tint),
        "font": {"fontObject": ref(FONT), "size": float(size),
                 "typefaceFontName": "Bold" if bold else "Regular"},
        "justification": align, "visibility": "HitTestInvisible",
    }, rect, z=5)


def icon_path(name):
    return f"{FOLDER}/Icons/T_Options{name}.T_Options{name}"


async def icon(bp, parent, name, resource, rect=None, tint=PAPER):
    return await add(bp, "Image", name, parent, {
        "brush": {"resourceObject": ref(icon_path(resource)), "drawAs": "Image",
                  "imageSize": dict(x=128., y=128.)},
        "colorAndOpacity": tint, "visibility": "HitTestInvisible",
    }, rect, z=5)


async def panel(bp, parent, name, rect, tint=PAPER, radius=12, outline=CLEAR, width=0, z=1):
    return await add(bp, "Border", name, parent, {
        "background": brush(tint, radius, outline, width), "brushColor": WHITE,
        "padding": margin(), "visibility": "HitTestInvisible",
    }, rect, z)


def button_style():
    return {
        "normal": brush(TEAL, 7, color(.68, .53, .29), 1),
        "hovered": brush(color(.085, .36, .31), 7, color(.85, .66, .34), 2),
        "pressed": brush(color(.025, .17, .15), 7, GOLD, 2),
        "disabled": brush(color(.27, .33, .29), 7, color(.45, .47, .39), 1),
        "normalPadding": margin(), "pressedPadding": margin(0, 2, 0, 0),
    }


async def create(name, parent_class):
    asset = FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(asset):
        raise RuntimeError(f"Asset already exists; edit it in UMG instead of rebuilding: {asset}")
    result = await call(UMG, "CreateWidgetBlueprint", {
        "folderPath": FOLDER, "assetName": name, "parentClass": ref(parent_class),
    })
    if not path_of(result):
        raise RuntimeError(f"CreateWidgetBlueprint failed: {result}")
    return path_of(result)


async def finish(bp):
    if not await call(UMG, "CompileWidgetBlueprint", {"widgetBlueprint": ref(bp)}):
        raise RuntimeError(f"Widget compile failed: {bp}")
    asset = unreal.load_asset(bp)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Widget save failed: {bp}")


def import_asset(filename, destination, name):
    task = unreal.AssetImportTask()
    task.filename = str(ROOT / "SourceArt/UI/Options" / filename)
    task.destination_path = FOLDER + destination
    task.destination_name = name
    task.automated = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = list(task.imported_object_paths)
    if not paths:
        raise RuntimeError(f"Import failed: {filename}")
    asset = unreal.load_asset(paths[0])
    if isinstance(asset, unreal.Texture2D):
        asset.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        asset.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        asset.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Import save failed: {filename}")


async def build_card():
    bp = await create("WBP_OptionsCard", "/Script/HeavenHyperVoice.UEOptionsCardWidget")
    size = await add(bp, "SizeBox", "CardSize", values={"widthOverride": 146., "heightOverride": 148., "bOverride_WidthOverride": True, "bOverride_HeightOverride": True})
    button = await add(bp, "Button", "CardButton", size, {"widgetStyle": button_style(), "isFocusable": True})
    content = await add(bp, "CanvasPanel", "CardContent", button, {"visibility": "HitTestInvisible"})
    await props(path_of(content["slot"]), {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill", "padding": margin()})
    await icon(bp, content, "IconImage", "Resume", (47, 26, 52, 52))
    await text(bp, content, "TitleText", "계속하기", (6, 101, 134, 30), 16, PAPER, bold=True, align="Center")
    # Defaults are also shown when this reusable card is opened on its own.
    asset = unreal.load_asset(bp)
    await call(UMG, "CompileWidgetBlueprint", {"widgetBlueprint": ref(bp)})
    defaults = unreal.get_default_object(asset.generated_class())
    defaults.set_editor_property("action_id", "Resume")
    defaults.set_editor_property("title", "계속하기")
    defaults.set_editor_property("icon_texture", unreal.load_asset(icon_path("Resume")))
    await finish(bp)
    return bp


async def build_menu():
    bp = await create("WBP_OptionsMenu", "/Script/HeavenHyperVoice.UEOptionsMenuWidget")
    screen = await add(bp, "CanvasPanel", "ScreenRoot", values={"visibility": "SelfHitTestInvisible"})
    scale = await add(bp, "ScaleBox", "ResponsiveScale", screen, {"stretch": "ScaleToFit", "stretchDirection": "Both", "visibility": "SelfHitTestInvisible"})
    await canvas(scale, (0, 0, 0, 0), anchors=(0, 0, .375, 1), z=2)
    design = await add(bp, "SizeBox", "DesignSize", scale, {"widthOverride": 720., "heightOverride": 1080., "bOverride_WidthOverride": True, "bOverride_HeightOverride": True, "visibility": "SelfHitTestInvisible"})
    await props(path_of(design["slot"]), {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Top"})
    surface = await add(bp, "CanvasPanel", "MenuSurface", design, {"visibility": "SelfHitTestInvisible"})
    backdrop = await panel(bp, surface, "MenuPaper", (0, 0, 720, 1080), IVORY, 0, color(.68, .54, .32), 1)
    await props(path_of(backdrop["widget"]), {"visibility": "Visible"})
    await panel(bp, surface, "InnerPaper", (64, 0, 654, 1080), PAPER, 0)
    await panel(bp, surface, "SideRail", (0, 0, 64, 1080), color(.058, .075, .11), 0, z=3)
    await add(bp, "Image", "JourneyArt", surface, {
        "brush": {"resourceObject": ref(ART), "drawAs": "Image", "imageSize": dict(x=2172., y=724.)},
        "visibility": "HitTestInvisible",
    }, (64, 0, 654, 218), z=2)
    await panel(bp, surface, "HeaderTint", (64, 0, 654, 218), color(.95, .89, .70, .66), 0, z=3)
    await text(bp, surface, "BrandText", "HEAVEN HYPER VOICE", (92, 22, 568, 22), 11, TEAL)
    await text(bp, surface, "MenuTitleText", "모험 메뉴", (94, 170, 540, 30), 18, INK, bold=True)
    await panel(bp, surface, "ProfileMedallion", (94, 57, 90, 90), PAPER, 45, GOLD, 2, 4)
    await icon(bp, surface, "ProfileIcon", "Profile", (115, 78, 48, 48), TEAL)
    await text(bp, surface, "PlayerNameText", "트레이너", (208, 64, 448, 40), 28, INK, bold=True)
    await text(bp, surface, "ProfileSubtitleText", "함께할 다음 모험을 준비해요", (210, 115, 452, 28), 15, MUTED)
    close = await add(bp, "Button", "CloseButton", surface, {"widgetStyle": button_style(), "isFocusable": True, "toolTipText": "메뉴 닫기"}, (10, 16, 44, 44), z=6)
    close_icon = await icon(bp, close, "CloseIcon", "Back")
    await props(path_of(close_icon["slot"]), {"padding": margin(10, 10, 10, 10)})
    await panel(bp, surface, "HeaderRule", (80, 219, 622, 1), color(.68, .61, .43), 0, z=4)
    await text(bp, surface, "QuickMenuText", "빠른 메뉴", (94, 246, 480, 28), 16, INK, bold=True)
    scroll = await add(bp, "ScrollBox", "MenuScroll", surface, {"scrollBarVisibility": "Collapsed", "consumeMouseWheel": "WhenScrollingPossible"}, (78, 290, 624, 714), z=5)
    grid = await add(bp, "UniformGridPanel", "MenuGrid", scroll, {"slotPadding": margin(5, 6, 5, 6)})
    await props(path_of(grid["slot"]), {"horizontalAlignment": "HAlign_Fill"})
    rows = [
        ("ResumeCard", "Resume", "계속하기", "Resume"),
        ("SettingsCard", "Settings", "설정", "Settings"),
        ("CharacterSelectCard", "CharacterSelect", "캐릭터 선택", "Characters"),
        ("LogoutCard", "Logout", "로그아웃", "Logout"),
    ]
    for i, (name, action, title, resource) in enumerate(rows):
        card = await add(bp, FOLDER + "/WBP_OptionsCard.WBP_OptionsCard_C", name, grid)
        instance = unreal.load_object(None, path_of(card["widget"]))
        for key, value in dict(action_id=action, title=title, icon_texture=unreal.load_asset(icon_path(resource))).items():
            instance.set_editor_property(key, value)
        await props(path_of(card["slot"]), {"row": i // 4, "column": i % 4, "horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill"})
    await panel(bp, surface, "FooterRule", (94, 1028, 590, 1), color(.74, .72, .65), 0, z=4)
    await icon(bp, surface, "FooterMark", "Compass", (18, 1032, 28, 28), GOLD)
    await text(bp, surface, "FooterText", "우리의 모험은 계속됩니다", (94, 1047, 550, 22), 12, MUTED)
    await call(UMG, "CompileWidgetBlueprint", {"widgetBlueprint": ref(bp)})
    defaults = unreal.get_default_object(unreal.load_asset(bp).generated_class())
    defaults.set_editor_property("entrance_offset", 720.)
    await finish(bp)
    return bp


async def labeled_button(bp, parent, name, label, rect):
    button = await add(bp, "Button", name, parent,
                       {"widgetStyle": button_style(), "isFocusable": True}, rect, z=6)
    await add(bp, "TextBlock", name + "Label", button, {
        "text": label, "font": {"fontObject": ref(FONT), "size": 18.},
        "colorAndOpacity": slate(PAPER), "justification": "Center", "visibility": "HitTestInvisible",
    })
    return button


async def settings_page(bp, switcher, name):
    scroll = await add(bp, "ScrollBox", name + "Scroll", switcher, {
        "scrollBarVisibility": "Visible", "consumeMouseWheel": "WhenScrollingPossible",
        "clipping": "ClipToBounds", "bAnimateWheelScrolling": True,
    })
    await props(path_of(scroll["slot"]), {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill"})
    rows = await add(bp, "VerticalBox", name + "Rows", scroll)
    await props(path_of(rows["slot"]), {"horizontalAlignment": "HAlign_Fill", "padding": margin(0, 0, 16, 12)})
    return rows


async def settings_row(bp, rows, name, label, hint, options=None):
    size = await add(bp, "SizeBox", name + "Row", rows, {
        "heightOverride": 106., "bOverride_HeightOverride": True})
    await props(path_of(size["slot"]), {"horizontalAlignment": "HAlign_Fill", "padding": margin(0, 0, 0, 10)})
    content = await add(bp, "CanvasPanel", name + "Content", size)
    background = await panel(bp, content, name + "Background", (0, 0, 0, 0), color(.90, .91, .87), 5)
    await canvas(background, (0, 0, 0, 0), anchors=(0, 0, 1, 1))
    await text(bp, content, name + "Label", label, (22, 20, 480, 32), 21, INK, True)
    await text(bp, content, name + "Hint", hint, (22, 61, 530, 28), 15, MUTED)
    if options is None:
        await add(bp, "CheckBox", name, content, {"toolTipText": hint}, (816, 30, 54, 48), z=5)
    else:
        style = button_style()
        style["normal"] = brush(color(.98, .98, .95), 4, color(.60, .65, .61), 1)
        style["hovered"] = brush(color(.85, .93, .89), 4, TEAL, 2)
        style["pressed"] = brush(color(.75, .86, .81), 4, TEAL, 2)
        await add(bp, "ComboBoxString", name, content, {
            "defaultOptions": options, "selectedOption": options[0],
            "font": {"fontObject": ref(FONT), "size": 19.}, "foregroundColor": slate(INK),
            "contentPadding": margin(12, 6, 12, 6),
            "widgetStyle": {"comboButtonStyle": {"buttonStyle": style}},
        }, (578, 27, 292, 52), z=5)


async def screen_surface(bp, height):
    root = await add(bp, "CanvasPanel", "ScreenRoot", values={"visibility": "SelfHitTestInvisible"})
    scale = await add(bp, "ScaleBox", "ResponsiveScale", root, {
        "stretch": "ScaleToFit", "stretchDirection": "DownOnly", "visibility": "SelfHitTestInvisible"})
    await canvas(scale, (24, 24, 24, 24), anchors=(0, 0, .45, 1))
    size = await add(bp, "SizeBox", "ScreenSize", scale, {
        "widthOverride": 700., "heightOverride": float(height),
        "bOverride_WidthOverride": True, "bOverride_HeightOverride": True,
        "visibility": "SelfHitTestInvisible"})
    await props(path_of(size["slot"]), {"horizontalAlignment": "HAlign_Left", "verticalAlignment": "VAlign_Center"})
    surface = await add(bp, "CanvasPanel", "ScreenSurface", size, {"visibility": "SelfHitTestInvisible"})
    background = await panel(bp, surface, "ScreenPaper", (0, 0, 700, height), PAPER, 8, GOLD, 1)
    await props(path_of(background["widget"]), {"visibility": "Visible"})
    await panel(bp, surface, "HeaderAccent", (0, 0, 700, 6), TEAL, 0, z=3)
    return surface


async def build_settings_screen():
    bp = await create("WBP_GameSettings", "/Script/HeavenHyperVoice.UEGameSettingsWidget")
    root = await add(bp, "CanvasPanel", "ScreenRoot", values={"visibility": "SelfHitTestInvisible"})
    scale = await add(bp, "ScaleBox", "ResponsiveScale", root, {
        "stretch": "ScaleToFit", "stretchDirection": "Both", "visibility": "SelfHitTestInvisible"})
    await canvas(scale, (32, 32, 32, 32), anchors=(0, 0, 1, 1))
    size = await add(bp, "SizeBox", "ScreenSize", scale, {
        "widthOverride": 1280., "heightOverride": 820.,
        "bOverride_WidthOverride": True, "bOverride_HeightOverride": True,
        "visibility": "SelfHitTestInvisible"})
    await props(path_of(size["slot"]), {"horizontalAlignment": "HAlign_Center", "verticalAlignment": "VAlign_Center"})
    surface = await add(bp, "CanvasPanel", "ScreenSurface", size, {"visibility": "SelfHitTestInvisible"})
    background = await panel(bp, surface, "ScreenPaper", (0, 0, 1280, 820), PAPER, 10, GOLD, 1)
    await props(path_of(background["widget"]), {"visibility": "Visible"})
    await panel(bp, surface, "HeaderAccent", (0, 0, 1280, 6), TEAL, 0, z=3)
    await icon(bp, surface, "SettingsIcon", "Settings", (34, 28, 42, 42), TEAL)
    await text(bp, surface, "ScreenTitle", "설정", (94, 26, 510, 48), 32, INK, True)
    await text(bp, surface, "ScreenSubtitle", "나에게 맞는 플레이 환경", (800, 40, 430, 26), 16, MUTED, align="Right")
    await panel(bp, surface, "HeaderRule", (28, 94, 1224, 1), GOLD, 0, z=4)
    await panel(bp, surface, "CategoryBackground", (16, 110, 250, 588), IVORY, 6)
    await text(bp, surface, "CategoryLabel", "설정 분류", (36, 126, 210, 28), 15, MUTED)
    await labeled_button(bp, surface, "DisplayTab", "화면", (30, 174, 222, 62))
    await labeled_button(bp, surface, "GraphicsTab", "그래픽", (30, 250, 222, 62))
    await text(bp, surface, "CategoryTitleText", "화면", (298, 115, 900, 42), 27, INK, True)
    pages = await add(bp, "WidgetSwitcher", "SettingsPages", surface,
                      {"activeWidgetIndex": 0}, (298, 172, 932, 526), z=5)
    display = await settings_page(bp, pages, "Display")
    await settings_row(bp, display, "FrameLimitCombo", "프레임 제한", "초당 표시할 최대 프레임 수를 정합니다.", ["제한 없음", "30", "60", "120", "144"])
    await settings_row(bp, display, "VSyncCheckBox", "수직 동기화", "화면의 찢어짐을 줄입니다.")
    graphics = await settings_page(bp, pages, "Graphics")
    quality = ["낮음", "보통", "높음", "최고", "시네마틱"]
    await settings_row(bp, graphics, "QualityCombo", "전체 품질", "프리셋을 선택하면 세부 품질이 함께 바뀝니다.", ["사용자 지정"] + quality)
    for name, title, hint in [
        ("ShadowCombo", "그림자 품질", "그림자의 선명도와 표현 품질을 조절합니다."),
        ("TextureCombo", "텍스처 품질", "물체 표면의 세밀한 표현을 조절합니다."),
        ("EffectsCombo", "효과 품질", "전투와 환경 효과의 표현 품질을 조절합니다."),
        ("ViewDistanceCombo", "표시 거리", "먼 곳에 있는 물체의 표시 품질을 조절합니다."),
        ("AntiAliasingCombo", "안티앨리어싱", "물체 가장자리의 계단 현상을 줄입니다."),
        ("FoliageCombo", "식생 품질", "풀과 식생의 표시 품질을 조절합니다."),
    ]:
        await settings_row(bp, graphics, name, title, hint, quality)
    await panel(bp, surface, "FooterRule", (28, 718, 1224, 1), GOLD, 0, z=4)
    await text(bp, surface, "SettingsStatusText", "", (34, 744, 550, 30), 17, TEAL)
    await text(bp, surface, "ApplyHint", "변경한 설정은 적용을 눌러 저장하세요.", (34, 779, 630, 24), 14, MUTED)
    await labeled_button(bp, surface, "BackButton", "돌아가기", (742, 746, 232, 52))
    await labeled_button(bp, surface, "ApplySettingsButton", "적용", (996, 746, 232, 52))
    await finish(bp)
    return bp


async def build_confirm_screen():
    bp = await create("WBP_OptionsConfirm", "/Script/HeavenHyperVoice.UEOptionsConfirmWidget")
    surface = await screen_surface(bp, 350)
    await text(bp, surface, "ConfirmTitleText", "이동 확인", (36, 34, 628, 44), 28, INK, True)
    message = await text(bp, surface, "ConfirmMessageText", "선택한 화면으로 이동할까요?", (36, 108, 628, 112), 18, MUTED)
    await props(path_of(message["widget"]), {"autoWrapText": True})
    await labeled_button(bp, surface, "BackButton", "취소", (36, 256, 298, 58))
    await labeled_button(bp, surface, "ConfirmButton", "확인", (366, 256, 298, 58))
    await finish(bp)
    return bp


async def build_hud():
    bp = await create("WBP_OptionsHUD", "/Script/HeavenHyperVoice.UEOptionsHUDWidget")
    root = await add(bp, "CanvasPanel", "HUDRoot", values={"visibility": "SelfHitTestInvisible"})
    button = await add(bp, "Button", "MenuButton", root, {
        "widgetStyle": button_style(), "isFocusable": False,
        "toolTipText": "모험 메뉴 (ESC) · G를 누른 채 클릭",
    }, (24, 24, 56, 56), z=1)
    img = await icon(bp, button, "MenuIcon", "Menu")
    await props(path_of(img["slot"]), {"padding": margin(15, 15, 15, 15)})
    await text(bp, root, "MenuKeyHint", "ESC / G+클릭", (4, 85, 96, 22), 10, PAPER, align="Center")
    await finish(bp)
    return bp


async def main():
    report = {"assets": [], "success": False}
    try:
        for cls in ("UEOptionsCardWidget", "UEOptionsMenuWidget", "UEOptionsHUDWidget", "UEGameSettingsWidget", "UEOptionsConfirmWidget"):
            if not unreal.load_class(None, "/Script/HeavenHyperVoice." + cls):
                raise RuntimeError(f"Build and restart the editor first: {cls}")
        for name in ("Resume", "Settings", "Characters", "Logout", "Menu", "Back", "Profile", "Compass"):
            import_asset(f"Icons/{name}.png", "/Icons", f"T_Options{name}")
        import_asset("OptionsJourney.png", "/Art", "T_OptionsJourney")
        report["assets"].append(await build_card())
        report["assets"].append(await build_menu())
        report["assets"].append(await build_hud())
        report["assets"].append(await build_settings_screen())
        report["assets"].append(await build_confirm_screen())
        report["success"] = True
    except Exception as error:
        report["error"] = str(error)
        unreal.log_error(str(error))
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("Options menu: " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    asyncio.run(main())
