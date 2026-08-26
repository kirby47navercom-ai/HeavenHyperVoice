"""원형 포켓몬 초상화 20종과 WBP 표시 설정을 검증한다."""

import unreal


SPECIES_NAMES = (
    "귀뚤뚜기", "기라티나", "꼬링크", "꼬부기", "꽁어름",
    "디아루가", "랄토스", "모부기", "벼리짱", "불꽃숭이",
    "아르세우스", "이브이", "이상해씨", "자망칼", "터검니",
    "파이리", "파치리스", "팽도리", "펄기아", "피카츄",
)

ROUNDED_BORDER_PATHS = (
    "/Game/UI/PokemonParty/WBP_PokemonParty.WBP_PokemonParty:WidgetTree.ActiveProfilePanel",
    "/Game/UI/PokemonParty/WBP_PokemonParty.WBP_PokemonParty:WidgetTree.ActivePortraitBackground",
    "/Game/UI/PokemonParty/WBP_PokemonProfileEntry.WBP_PokemonProfileEntry:WidgetTree.EntryBackground",
    "/Game/UI/PokemonParty/WBP_PokemonProfileEntry.WBP_PokemonProfileEntry:WidgetTree.EntryIconBackground",
)

SELECTED_INDICATOR_PATH = (
    "/Game/UI/PokemonParty/WBP_PokemonProfileEntry.WBP_PokemonProfileEntry:WidgetTree.SelectedIndicator"
)

IMAGE_PATHS = (
    "/Game/UI/PokemonParty/WBP_PokemonParty.WBP_PokemonParty:WidgetTree.ActivePokemonIcon",
    "/Game/UI/PokemonParty/WBP_PokemonProfileEntry.WBP_PokemonProfileEntry:WidgetTree.PokemonIcon",
)


failures = []
for species_name in SPECIES_NAMES:
    texture_path = (
        f"/Game/UI/PokemonParty/Portraits/{species_name}/"
        f"T_{species_name}_Portrait"
    )
    data_path = f"/Game/Pokemon/SpeciesData/{species_name}/DA_{species_name}"
    texture = unreal.EditorAssetLibrary.load_asset(texture_path)
    species_data = unreal.EditorAssetLibrary.load_asset(data_path)

    if not isinstance(texture, unreal.Texture2D):
        failures.append(f"{species_name}: Texture2D 없음")
        continue
    if species_data is None:
        failures.append(f"{species_name}: DataAsset 없음")
        continue

    width = texture.blueprint_get_size_x()
    height = texture.blueprint_get_size_y()
    connected_texture = species_data.get_editor_property("profile_icon")
    if width != 512 or height != 512:
        failures.append(f"{species_name}: 크기 {width}x{height}")
    if connected_texture != texture:
        failures.append(f"{species_name}: ProfileIcon 연결 불일치")

for border_path in ROUNDED_BORDER_PATHS:
    border = unreal.find_object(None, border_path)
    if not isinstance(border, unreal.Border):
        failures.append(f"WBP Border 없음: {border_path}")
        continue
    brush = border.get_editor_property("background")
    if brush.draw_as != unreal.SlateBrushDrawType.ROUNDED_BOX:
        failures.append(f"RoundedBox 아님: {border_path}")
    if (
        brush.outline_settings.rounding_type
        != unreal.SlateBrushRoundingType.HALF_HEIGHT_RADIUS
    ):
        failures.append(f"HalfHeightRadius 아님: {border_path}")

for image_path in IMAGE_PATHS:
    image = unreal.find_object(None, image_path)
    if not isinstance(image, unreal.Image):
        failures.append(f"WBP Image 없음: {image_path}")
        continue
    slot = image.get_editor_property("slot")
    if not isinstance(slot, unreal.OverlaySlot):
        failures.append(f"OverlaySlot 없음: {image_path}")
        continue
    if slot.get_editor_property("horizontal_alignment") != unreal.HorizontalAlignment.H_ALIGN_FILL:
        failures.append(f"가로 Fill 아님: {image_path}")
    if slot.get_editor_property("vertical_alignment") != unreal.VerticalAlignment.V_ALIGN_FILL:
        failures.append(f"세로 Fill 아님: {image_path}")

selected_indicator = unreal.find_object(None, SELECTED_INDICATOR_PATH)
if not isinstance(selected_indicator, unreal.Border):
    failures.append(f"선택 표시 Border 없음: {SELECTED_INDICATOR_PATH}")
elif (
    selected_indicator.get_editor_property("background").draw_as
    != unreal.SlateBrushDrawType.NO_DRAW_TYPE
):
    failures.append(f"기존 네모 선택 외곽선이 남아 있음: {SELECTED_INDICATOR_PATH}")

entry_background = unreal.find_object(
    None,
    "/Game/UI/PokemonParty/WBP_PokemonProfileEntry.WBP_PokemonProfileEntry:WidgetTree.EntryIconBackground",
)
if isinstance(entry_background, unreal.Border):
    entry_slot = entry_background.get_editor_property("slot")
    if isinstance(entry_slot, unreal.CanvasPanelSlot):
        entry_size = entry_slot.get_size()
        if entry_size.x != entry_size.y:
            failures.append(
                f"작은 프로필이 정사각형이 아님: {entry_size.x}x{entry_size.y}"
            )

if failures:
    raise RuntimeError("원형 초상화 검증 실패: " + " / ".join(failures))

unreal.log("[ROUND PORTRAIT VERIFY] 20종 크기·연결·WBP 원형 표시 설정 검증 통과")
