"""Author editable photo HUD, menu card, input assets and Goldenrod ocean. No PIE."""
import asyncio
import json
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import create_options_menu as ui

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
PHOTO = '/Game/UI/Options/WBP_PhotoMode'
CITY = '/Game/Environments/Goldenrod_R03'
LIB = unreal.EditorAssetLibrary
DARK = ui.color(.018, .029, .042, .88)
LIGHT = ui.color(.88, .90, .89)
ACCENT = ui.color(.67, .53, .31)


async def photo_button(bp, parent, name, label, rect, radius=10):
    style = {
        'normal': ui.brush(ui.color(.055, .074, .09, .95), radius, ACCENT, 1),
        'hovered': ui.brush(ui.color(.12, .17, .19, .98), radius, LIGHT, 1),
        'pressed': ui.brush(ui.color(.03, .045, .06), radius, ACCENT, 2),
        'normalPadding': ui.margin(), 'pressedPadding': ui.margin(0, 2, 0, 0),
    }
    button = await ui.add(bp, 'Button', name, parent, {'widgetStyle': style, 'isFocusable': False}, rect, z=6)
    if label:
        await ui.add(bp, 'TextBlock', name+'Label', button, {
            'text': label, 'font': {'fontObject': ui.ref(ui.FONT), 'size': 17.},
            'colorAndOpacity': ui.slate(LIGHT), 'justification': 'Center', 'visibility': 'HitTestInvisible'})
    return button


async def author_hud():
    if LIB.does_asset_exist(PHOTO):
        return PHOTO
    bp = await ui.create('WBP_PhotoMode', '/Script/HeavenHyperVoice.UEPhotoModeWidget')
    screen = await ui.add(bp, 'CanvasPanel', 'ScreenRoot', values={'visibility': 'SelfHitTestInvisible'})
    scale = await ui.add(bp, 'ScaleBox', 'ResponsiveScale', screen,
                         {'stretch': 'ScaleToFit', 'visibility': 'SelfHitTestInvisible'})
    await ui.canvas(scale, (0, 0, 0, 0), anchors=(0, 0, 1, 1))
    size = await ui.add(bp, 'SizeBox', 'DesignSize', scale, {
        'widthOverride': 1920., 'heightOverride': 1080., 'bOverride_WidthOverride': True,
        'bOverride_HeightOverride': True, 'visibility': 'SelfHitTestInvisible'})
    root = await ui.add(bp, 'CanvasPanel', 'CameraLayout', size, {'visibility': 'SelfHitTestInvisible'})
    await ui.panel(bp, root, 'TitlePlate', (44, 40, 240, 64), DARK, 12, ACCENT, 1)
    await ui.icon(bp, root, 'CameraIcon', 'Camera', (62, 58, 30, 30), LIGHT)
    await ui.text(bp, root, 'ModeTitle', '카메라', (108, 54, 160, 36), 23, LIGHT)
    await photo_button(bp, root, 'ExitButton', '나가기  ·  ESC', (1660, 40, 216, 64))
    await ui.text(bp, root, 'MovementHint', 'W A S D  이동     ·     우클릭 드래그  시점     ·     휠  확대 / 축소',
                  (400, 58, 1120, 34), 16, LIGHT, align='Center')
    # Each viewfinder mark is an ordinary editable UMG Border; the center remains clear.
    for name, x, y, dx, dy in [('TL', 180, 170, 1, 1), ('TR', 1740, 170, -1, 1),
                               ('BL', 180, 850, 1, -1), ('BR', 1740, 850, -1, -1)]:
        await ui.panel(bp, root, name+'Horizontal', (x if dx > 0 else x-44, y, 44, 2), ui.color(.9,.9,.87,.5), 0)
        await ui.panel(bp, root, name+'Vertical', (x, y if dy > 0 else y-44, 2, 44), ui.color(.9,.9,.87,.5), 0)
    await ui.panel(bp, root, 'ControlsPlate', (530, 906, 860, 136), DARK, 18, ACCENT, 1)
    await ui.text(bp, root, 'ZoomLabel', '줌', (566, 925, 80, 24), 14, LIGHT)
    await photo_button(bp, root, 'ZoomOutButton', '−', (566, 964, 44, 44))
    await ui.add(bp, 'Slider', 'ZoomSlider', root, {
        'minValue': 0., 'maxValue': 1., 'value': 0., 'stepSize': .02,
        'isFocusable': False, 'sliderBarColor': ui.color(.22,.28,.30), 'sliderHandleColor': ACCENT,
    }, (626, 970, 268, 32), z=6)
    await photo_button(bp, root, 'ZoomInButton', '+', (910, 964, 44, 44))
    await ui.text(bp, root, 'ZoomText', '1.0 ×', (840, 925, 114, 26), 16, LIGHT, align='Right')
    await ui.panel(bp, root, 'ControlDivider', (984, 931, 1, 86), ui.color(.4,.43,.42,.4), 0)
    capture = await photo_button(bp, root, 'CaptureButton', '', (1016, 928, 88, 88), 44)
    image = await ui.icon(bp, capture, 'ShutterIcon', 'Camera', tint=LIGHT)
    await ui.props(ui.path_of(image['slot']), {'padding': ui.margin(24,24,24,24)})
    await ui.text(bp, root, 'CaptureTitle', '사진 촬영', (1130, 944, 210, 30), 22, LIGHT)
    await ui.text(bp, root, 'CaptureHint', 'SPACE', (1132, 983, 190, 22), 13, ACCENT)
    status = await ui.text(bp, root, 'StatusText', '', (300, 1051, 1320, 25), 12, LIGHT, align='Center')
    await ui.props(ui.path_of(status['widget']), {'textOverflowPolicy': 'Ellipsis'})
    await ui.call(ui.UMG, 'CompileWidgetBlueprint', {'widgetBlueprint': ui.ref(bp)})
    defaults = unreal.get_default_object(unreal.load_asset(bp).generated_class())
    defaults.set_editor_property('is_focusable', True)
    await ui.finish(bp)
    return bp


async def add_menu_card():
    bp = '/Game/UI/Options/WBP_OptionsMenu.WBP_OptionsMenu'
    LIB.load_asset(bp)
    card = unreal.find_object(None, bp+':WidgetTree.CameraCard')
    if not card:
        grid = {'widget': ui.ref(bp+':WidgetTree.MenuGrid')}
        item = await ui.add(bp, '/Game/UI/Options/WBP_OptionsCard.WBP_OptionsCard_C', 'CameraCard', grid)
        card = unreal.load_object(None, ui.path_of(item['widget']))
        await ui.props(ui.path_of(item['slot']), {'row': 1, 'column': 0,
                         'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'})
    card.set_editor_property('action_id', 'Camera')
    card.set_editor_property('title', '카메라')
    card.set_editor_property('icon_texture', LIB.load_asset(ui.icon_path('Camera')))
    card.set_tool_tip_text('1인칭으로 걸으며 사진을 촬영합니다. 우클릭 시점 · 휠 줌 · Space 촬영')
    await ui.finish(bp)


def duplicate_if_missing(source, target):
    return LIB.load_asset(target) if LIB.does_asset_exist(target) else LIB.duplicate_asset(source, target)


def key(name):
    value = unreal.Key()
    value.import_text(name)
    return value


def author_inputs():
    actions = {}
    for name, axis in [('ToggleChat', False), ('PhotoCapture', False), ('PhotoLook', False), ('PhotoZoom', True)]:
        source = '/Game/Input/Actions/IA_LookYaw' if axis else '/Game/Input/Actions/IA_Chat'
        action = duplicate_if_missing(source, '/Game/Input/Actions/IA_'+name)
        action.set_editor_property('triggers', [])
        action.set_editor_property('modifiers', [])
        action.set_editor_property('consume_input', True)
        action.set_editor_property('value_type', unreal.InputActionValueType.AXIS1D if axis else unreal.InputActionValueType.BOOLEAN)
        LIB.save_loaded_asset(action, only_if_is_dirty=False)
        actions[name] = action
    data = LIB.load_asset('/Game/Data/Input/DA_PlayerInput')
    context = data.get_editor_property('input_mapping_context')
    context.unmap_all_keys_from_action(actions['ToggleChat'])
    context.map_key(actions['ToggleChat'], key('T'))
    entries = [e for e in data.get_editor_property('input_actions') if 'Input.Action.ToggleChat' not in e.export_text()]
    entry = unreal.UEInputAction()
    entry.import_text('(InputTag=(TagName="Input.Action.ToggleChat"),InputAction="/Script/EnhancedInput.InputAction\''
                      + actions['ToggleChat'].get_path_name() + '\'")')
    entries.append(entry)
    data.set_editor_property('input_actions', entries)
    LIB.save_loaded_asset(data, only_if_is_dirty=False)
    LIB.save_loaded_asset(context, only_if_is_dirty=False)
    photo_context = duplicate_if_missing(context.get_path_name(), '/Game/Input/Mapping/IMC_PhotoMode')
    photo_context.unmap_all()
    for name, binding in [('PhotoCapture', 'SpaceBar'), ('PhotoLook', 'RightMouseButton'), ('PhotoZoom', 'MouseWheelAxis')]:
        photo_context.map_key(actions[name], key(binding))
    LIB.save_loaded_asset(photo_context, only_if_is_dirty=False)
    controller = LIB.load_asset('/Game/Blueprints/Login/BP_LoginPlayerController')
    defaults = unreal.get_default_object(controller.generated_class())
    for prop, value in {'photo_mapping_context': photo_context, 'photo_capture_action': actions['PhotoCapture'],
                        'photo_zoom_action': actions['PhotoZoom'], 'photo_look_action': actions['PhotoLook'],
                        'photo_mode_widget_class': LIB.load_asset(PHOTO).generated_class()}.items():
        defaults.set_editor_property(prop, value)
    unreal.BlueprintEditorLibrary.compile_blueprint(controller)
    LIB.save_loaded_asset(controller, only_if_is_dirty=False)
    return {name: value.get_path_name() for name, value in actions.items()}


def extend_ocean():
    from separate_goldenrod_ocean import main as separate_ocean
    return separate_ocean()


async def main():
    report = {'success': False}
    try:
        if not LIB.does_asset_exist(ui.icon_path('Camera')):
            ui.import_asset('Icons/Camera.png', '/Icons', 'T_OptionsCamera')
        report['widget'] = await author_hud()
        await add_menu_card()
        report['actions'] = author_inputs()
        report['ocean'] = extend_ocean()
        report['success'] = True
    finally:
        output = ROOT/'Saved/Codex/PhotoMode/authoring.json'
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        unreal.log('PHOTO AUTHORING: '+json.dumps(report))


if __name__ == '__main__':
    asyncio.run(main())
