"""Designer assets for the existing C++ field party widgets. Run in Unreal."""
import unreal
import create_options_menu as a

FOLDER = '/Game/UI/PokemonParty'

def field_button_style():
    return dict(normal=a.brush(a.color(.14, .14, .18), 7),
                hovered=a.brush(a.color(.22, .22, .27), 7),
                pressed=a.brush(a.color(.09, .09, .11), 7),
                disabled=a.brush(a.color(.09, .09, .11), 7))


async def create(name, parent):
    if unreal.EditorAssetLibrary.does_asset_exist(FOLDER + '/' + name):
        raise RuntimeError('Asset already exists: ' + name)
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property('parent_class', unreal.load_class(None, parent))
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.WidgetBlueprint, factory)
    if not asset:
        raise RuntimeError('Could not create ' + name)
    return asset.get_path_name()


async def ensure_assets():
    entry = FOLDER + '/WBP_FieldPartyEntry.WBP_FieldPartyEntry'
    if not unreal.EditorAssetLibrary.does_asset_exist(entry):
        entry = await create('WBP_FieldPartyEntry', '/Script/HeavenHyperVoice.UEFieldPartyEntryWidget')
        size = await a.add(entry, 'SizeBox', 'EntrySize', values={
            'widthOverride': 150., 'heightOverride': 158.,
            'bOverride_WidthOverride': True, 'bOverride_HeightOverride': True})
        frame = await a.add(entry, 'Border', 'SelectionBorder', size, {
            'background': a.brush(a.WHITE, 7), 'brushColor': a.CLEAR, 'padding': a.margin(3, 3, 3, 3)})
        overlay = await a.add(entry, 'Overlay', 'EntryOverlay', frame)
        button = await a.add(entry, 'Button', 'SelectButton', overlay, {'widgetStyle': field_button_style()})
        await a.props(a.path_of(button['slot']), {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'})
        content = await a.add(entry, 'CanvasPanel', 'EntryContent', button, {'visibility': 'HitTestInvisible'})
        await a.props(a.path_of(content['slot']), {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'})
        await a.add(entry, 'Image', 'IconImage', content, {'visibility': 'HitTestInvisible'}, (21, 7, 102, 102))
        await a.text(entry, content, 'LabelText', '포켓몬', (4, 119, 136, 25), 15, a.WHITE, align='Center')
        badge = await a.add(entry, 'SizeBox', 'BadgeSize', overlay, {
            'widthOverride': 28., 'heightOverride': 28.,
            'bOverride_WidthOverride': True, 'bOverride_HeightOverride': True})
        await a.props(a.path_of(badge['slot']), {'horizontalAlignment': 'HAlign_Left', 'verticalAlignment': 'VAlign_Top'})
        border = await a.add(entry, 'Border', 'SlotBadge', badge, {
            'background': a.brush(a.WHITE, 5), 'brushColor': a.GOLD, 'visibility': 'HitTestInvisible', 'padding': a.margin()})
        await a.add(entry, 'TextBlock', 'SlotBadgeText', border, {
            'text': '1', 'font': {'fontObject': a.ref(a.FONT), 'size': 15.},
            'justification': 'Center', 'colorAndOpacity': a.slate(a.color(0, 0, 0))})
        await a.finish(entry)
    party = FOLDER + '/WBP_FieldParty.WBP_FieldParty'
    if not unreal.EditorAssetLibrary.does_asset_exist(party):
        party = await create('WBP_FieldParty', '/Script/HeavenHyperVoice.UEFieldPartyWidget')
        root = await a.add(party, 'CanvasPanel', 'ScreenRoot', values={'visibility': 'SelfHitTestInvisible'})
        scale = await a.add(party, 'ScaleBox', 'ResponsiveScale', root, {'stretch': 'ScaleToFit', 'stretchDirection': 'DownOnly'})
        await a.canvas(scale, (32, 32, 32, 32), anchors=(0, 0, 1, 1))
        size = await a.add(party, 'SizeBox', 'PanelSize', scale, {
            'widthOverride': 1240., 'heightOverride': 900.,
            'bOverride_WidthOverride': True, 'bOverride_HeightOverride': True})
        await a.props(a.path_of(size['slot']), {'horizontalAlignment': 'HAlign_Center', 'verticalAlignment': 'VAlign_Center'})
        surface = await a.add(party, 'CanvasPanel', 'PanelSurface', size)
        background = await a.panel(party, surface, 'PartyPanel', (0, 0, 1240, 900), a.color(.02, .02, .04, .94), 10)
        await a.props(a.path_of(background['widget']), {'visibility': 'Visible'})
        await a.text(party, surface, 'TitleText', '모든 포켓몬', (30, 24, 1180, 48), 28, a.WHITE, True)
        await a.text(party, surface, 'GuideText', '눌러서 파티에 넣고 빼기 (최대 3마리) · 1 2 3 키로 꺼내고 집어넣기', (30, 79, 1180, 32), 17, a.color(.65, .65, .7))
        scroll = await a.add(party, 'ScrollBox', 'PokemonScroll', surface,
                             {'clipping': 'ClipToBounds', 'bAnimateWheelScrolling': True}, (30, 132, 1180, 638))
        await a.add(party, 'WrapBox', 'PokemonList', scroll, {'innerSlotPadding': {'x': 4., 'y': 4.}})
        await a.text(party, surface, 'StatusText', '', (30, 789, 1180, 30), 17, a.color(.65, .65, .7))
        await a.labeled_button(party, surface, 'CloseButton', '닫기', (680, 838, 244, 44))
        await a.labeled_button(party, surface, 'ConfirmButton', '확인', (946, 838, 264, 44))
        await a.call(a.UMG, 'CompileWidgetBlueprint', {'widgetBlueprint': a.ref(party)})
        unreal.get_default_object(unreal.load_asset(party).generated_class()).set_editor_property(
            'entry_widget_class', unreal.load_asset(entry).generated_class())
        for button_name in ('CloseButton', 'ConfirmButton'):
            await a.props(party + ':WidgetTree.' + button_name, {'widgetStyle': field_button_style()})
        await a.finish(party)
    return [entry, party]
