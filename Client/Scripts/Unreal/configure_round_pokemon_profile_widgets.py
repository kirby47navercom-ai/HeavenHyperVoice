"""파티 WBP에서 원형 초상화 뒤의 기존 사각 배경을 제거한다."""

import unreal


ICON_BACKGROUND_PATHS = (
    "/Game/UI/PokemonParty/WBP_PokemonParty.WBP_PokemonParty:WidgetTree.ActivePortraitBackground",
    "/Game/UI/PokemonParty/WBP_PokemonProfileEntry.WBP_PokemonProfileEntry:WidgetTree.EntryIconBackground",
)

PANEL_PATH = (
    "/Game/UI/PokemonParty/WBP_PokemonParty.WBP_PokemonParty:WidgetTree.ActiveProfilePanel"
)

SELECTED_INDICATOR_PATH = (
    "/Game/UI/PokemonParty/WBP_PokemonProfileEntry.WBP_PokemonProfileEntry:WidgetTree.SelectedIndicator"
)

ENTRY_BACKGROUND_PATH = (
    "/Game/UI/PokemonParty/WBP_PokemonProfileEntry.WBP_PokemonProfileEntry:WidgetTree.EntryBackground"
)

IMAGE_PATHS = (
    "/Game/UI/PokemonParty/WBP_PokemonParty.WBP_PokemonParty:WidgetTree.ActivePokemonIcon",
    "/Game/UI/PokemonParty/WBP_PokemonProfileEntry.WBP_PokemonProfileEntry:WidgetTree.PokemonIcon",
)

ASSET_PATHS = (
    "/Game/UI/PokemonParty/WBP_PokemonParty",
    "/Game/UI/PokemonParty/WBP_PokemonProfileEntry",
)


def set_rounded_box(border):
    brush = border.get_editor_property("background")
    brush.draw_as = unreal.SlateBrushDrawType.ROUNDED_BOX
    outline_settings = brush.outline_settings
    outline_settings.rounding_type = (
        unreal.SlateBrushRoundingType.HALF_HEIGHT_RADIUS
    )
    brush.outline_settings = outline_settings
    border.set_editor_property("background", brush)


for background_path in ICON_BACKGROUND_PATHS:
    background = unreal.find_object(None, background_path)
    if not isinstance(background, unreal.Border):
        raise RuntimeError(f"초상화 Border를 찾지 못함: {background_path}")

    # 이미지 뒤 컨테이너도 실제 RoundedBox로 바꿔 네모 모서리가 남지 않게 한다.
    set_rounded_box(background)
    background.set_editor_property(
        "brush_color",
        unreal.LinearColor(0.29, 0.86, 0.91, 1.0),
    )

    if background.get_name() == "EntryIconBackground":
        canvas_slot = background.get_editor_property("slot")
        if not isinstance(canvas_slot, unreal.CanvasPanelSlot):
            raise RuntimeError("작은 프로필 CanvasPanelSlot을 찾지 못함")
        # 기존 44x42 크기 때문에 원형이 납작하게 보였다. 정확한 44x44 정사각형으로 고정한다.
        canvas_slot.set_size(unreal.Vector2D(44.0, 44.0))

panel = unreal.find_object(None, PANEL_PATH)
if not isinstance(panel, unreal.Border):
    raise RuntimeError(f"대표 프로필 패널을 찾지 못함: {PANEL_PATH}")
# 대표 정보 패널도 캡슐형으로 바꿔 원형 얼굴 뒤에 네모 배경이 비치지 않게 한다.
set_rounded_box(panel)

entry_background = unreal.find_object(None, ENTRY_BACKGROUND_PATH)
if not isinstance(entry_background, unreal.Border):
    raise RuntimeError(f"작은 프로필 바깥 Border를 찾지 못함: {ENTRY_BACKGROUND_PATH}")
# 작은 슬롯의 가장 바깥 흰색 Border도 HalfHeightRadius로 바꿔 흰 네모 모서리를 제거한다.
set_rounded_box(entry_background)

selected_indicator = unreal.find_object(None, SELECTED_INDICATOR_PATH)
if not isinstance(selected_indicator, unreal.Border):
    raise RuntimeError(f"선택 표시 Border를 찾지 못함: {SELECTED_INDICATOR_PATH}")
# 기존 선택 표시는 별도의 네모 외곽선을 가지고 있어 색 알파만 0으로 바꿔도 선이 남는다.
# 브러시 자체를 NoDrawType으로 바꾸고, 선택 원형 테두리는 PNG의 청록색 링이 담당한다.
selected_brush = selected_indicator.get_editor_property("background")
selected_brush.draw_as = unreal.SlateBrushDrawType.NO_DRAW_TYPE
selected_indicator.set_editor_property("background", selected_brush)

for image_path in IMAGE_PATHS:
    image = unreal.find_object(None, image_path)
    if not isinstance(image, unreal.Image):
        raise RuntimeError(f"초상화 Image를 찾지 못함: {image_path}")

    slot = image.get_editor_property("slot")
    if not isinstance(slot, unreal.OverlaySlot):
        raise RuntimeError(f"초상화 OverlaySlot을 찾지 못함: {image_path}")

    # 큰 대표 칸과 작은 슬롯 모두 원형 텍스처가 전체 영역을 사용하도록 한다.
    slot.set_editor_property(
        "horizontal_alignment",
        unreal.HorizontalAlignment.H_ALIGN_FILL,
    )
    slot.set_editor_property(
        "vertical_alignment",
        unreal.VerticalAlignment.V_ALIGN_FILL,
    )

for asset_path in ASSET_PATHS:
    if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
        raise RuntimeError(f"원형 프로필 WBP 저장 실패: {asset_path}")

unreal.log("[ROUND PORTRAIT] 원형 컨테이너 저장 및 네모 선택 외곽선 제거 완료")
