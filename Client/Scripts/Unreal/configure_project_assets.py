"""Populate the editable project asset catalog after the Content reorganization.

Run in UE 5.8 with the project editor module built. This is an authoring tool,
not a runtime asset-path lookup. Optional travel choices come from
Saved/Codex/ProjectAssets20260919/targets.json.
"""
import json
import re
import shutil
from pathlib import Path

import unreal


ASSET_PATH = '/Game/Blueprints/DA_ProjectAssets'
OUT = Path(unreal.Paths.project_saved_dir()) / 'Codex/ProjectAssets20260919'
LIB = unreal.EditorAssetLibrary
REPORT = {'assigned': {}, 'missing': [], 'changed_blueprints': []}


def backup_asset(path):
    package = path.split('.')[0]
    relative = package[6:]
    content = Path(unreal.Paths.project_content_dir())
    extension = '.umap' if (content / (relative + '.umap')).exists() else '.uasset'
    source = content / (relative + extension)
    destination = OUT / 'before-assets' / (relative + extension)
    if source.exists() and not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def load(path):
    obj = LIB.load_asset(path)
    if not obj:
        raise RuntimeError('Required asset missing: ' + path)
    return obj


def assign(data, name, path, blueprint=False):
    obj = LIB.load_blueprint_class(path) if blueprint else load(path)
    if not obj:
        raise RuntimeError('Required class missing: ' + path)
    data.set_editor_property(name, obj)
    REPORT['assigned'][name] = obj.get_path_name()


def configure_materials(data, registry):
    assets = registry.get_assets_by_path('/Game/Player/CharacterCustomization', recursive=True)
    # Prefer a project-specific replacement if both catalogs have the same source.
    assets = sorted(assets, key=lambda item: ('/HHV/' not in str(item.package_name), str(item.package_name)))
    REPORT['customization_asset_count'] = len(assets)
    material_rows = []
    morph_rows = []
    seen_sources = set()
    for entry in assets:
        path = str(entry.package_name)
        if '/Generated/MorphSafeMaterials/' not in path:
            continue
        material = entry.get_asset()
        if not material or not isinstance(material, unreal.MaterialInstanceConstant):
            continue
        parent = material.get_editor_property('parent')
        if parent and parent.get_path_name() not in seen_sources:
            row = unreal.UEMorphMaterialBinding()
            row.set_editor_property('source', parent)
            row.set_editor_property('replacement', material)
            morph_rows.append(row)
            seen_sources.add(parent.get_path_name())

    # Record the old single-slot outfit correction as explicit mesh/slot references.
    for entry in assets:
        path = str(entry.package_name)
        if str(entry.asset_class_path.asset_name) != 'SkeletalMesh' or '/Outfit/' not in path:
            continue
        if '/AssetsFBX/' in path or '/SkeletalMeshes/' not in path:
            continue
        mesh = entry.get_asset()
        if not mesh:
            continue
        slots = mesh.get_editor_property('materials')
        if len(slots) != 1:
            continue
        current = slots[0].get_editor_property('material_interface')
        if not current or 'Body' not in current.get_name() or 'Outfit' not in mesh.get_name():
            continue
        stem = mesh.get_name().removeprefix('SK_')
        if stem.endswith('_2'):
            stem = stem[:-2]
        if '_v' not in stem:
            stem += '_v01'
        versions = re.findall(r'_v(\d\d)', stem)
        version = 'v' + versions[-1] if versions else 'v01'
        name = 'MI_' + stem + '_M01'
        owner = path.rsplit('/', 2)[0]
        candidates = [owner.rsplit('/', 1)[0] + '/' + version + '/' + name,
                      owner + '/Materials/' + name]
        replacement = next((LIB.load_asset(p) for p in candidates if LIB.does_asset_exist(p)), None)
        if replacement:
            row = unreal.UECharacterMaterialBinding()
            row.set_editor_property('mesh', mesh)
            row.set_editor_property('material_slot', 0)
            row.set_editor_property('material', replacement)
            material_rows.append(row)
    data.set_editor_property('character_materials', material_rows)
    data.set_editor_property('morph_materials', morph_rows)
    REPORT['character_material_count'] = len(material_rows)
    REPORT['morph_material_count'] = len(morph_rows)


def clear_lobby_travel_overrides():
    path = '/Game/StartLevel/Blueprints/WBP_CharacterSelection'
    backup_asset(path)
    bp = load(path)
    defaults = unreal.get_default_object(bp.generated_class())
    defaults.set_editor_property('gameplay_level', None)
    defaults.set_editor_property('gameplay_game_mode_class', None)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not LIB.save_loaded_asset(bp, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + path)
    REPORT['changed_blueprints'].append(path)


def repair_renamed_instance_map(registry):
    # Explorer renamed the file, but the serialized World still has the name Filed.
    package = '/Game/InstanceMap/Plain/Plain'
    entries = registry.get_assets_by_package_name(package)
    for entry in entries:
        if str(entry.asset_class_path.asset_name) != 'World' or str(entry.asset_name) == 'Plain':
            continue
        backup_asset(package)
        # Map loading resolves the World even when the registry still reports
        # the old object name from the untouched file's asset-registry tags.
        world = unreal.EditorLoadingAndSavingUtils.load_map(package)
        if not world:
            raise RuntimeError('Cannot load the renamed instance map: ' + str(entry.asset_name))
        if world.get_name() != 'Plain':
            rename = unreal.AssetRenameData(asset=world, new_package_path='/Game/InstanceMap/Plain', new_name='Plain')
            if not unreal.AssetToolsHelpers.get_asset_tools().rename_assets([rename]):
                raise RuntimeError('Cannot update the instance World name to match its filename')
        if not unreal.EditorLoadingAndSavingUtils.save_map(world, package):
            raise RuntimeError('Cannot save the renamed instance map')
        REPORT['renamed_instance_world'] = world.get_path_name()


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    repair_renamed_instance_map(registry)
    if not LIB.does_directory_exist('/Game/Blueprints'):
        raise RuntimeError('Use the existing Blueprints folder; do not create a new Content root')
    backup_asset(ASSET_PATH)
    if LIB.does_asset_exist(ASSET_PATH):
        data = load(ASSET_PATH)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.UEProjectAssets)
        data = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'DA_ProjectAssets', '/Game/Blueprints', unreal.UEProjectAssets, factory)
    if not data:
        raise RuntimeError('Could not create project asset catalog')
    bindings = {
        'frontend_level': ('/Game/StartLevel/Maps/L_CharacterSelection', False),
        'species_catalog': ('/Game/Pokemon/DA_PokemonSpeciesCatalog', False),
        'pokemon_class': ('/Game/Pokemon/BP_Pokemon', True),
        'party_widget_class': ('/Game/UI/PokemonParty/WBP_FieldParty', True),
        'loading_screen_widget_class': ('/Game/UI/Loading/WBP_LoadingScreen', True),
        'gacha_widget_class': ('/Game/Gacha/UI/WBP_GachaStudio', True),
        'gacha_machine_class': ('/Game/Gacha/Blueprints/BP_GachaMachine', True),
        'gacha_studio_level': ('/Game/Gacha/Maps/L_GachaStudio', False),
    }
    for name, (path, blueprint) in bindings.items():
        assign(data, name, path, blueprint)
    pools = [load('/Game/Gacha/Data/DA_Gacha_' + name)
             for name in ('Fire', 'Water', 'Grass', 'Normal', 'Electric')]
    data.set_editor_property('gacha_pools', pools)
    REPORT['gacha_pool_count'] = len(pools)
    icons = {getattr(unreal.UEPokemonType, name.upper()): load('/Game/UI/PokemonType/T_Type_' + name)
             for name in ('Fire', 'Water', 'Grass', 'Normal', 'Electric')}
    data.set_editor_property('type_icons', icons)
    REPORT['type_icon_count'] = len(icons)
    targets_path = OUT / 'targets.json'
    targets = json.loads(targets_path.read_text(encoding='utf-8')) if targets_path.exists() else {}
    for name in ('field_level', 'instance_level'):
        if targets.get(name):
            assign(data, name, targets[name])
        else:
            REPORT['missing'].append(name)
    collisions = []
    for binding in targets.get('collisions', []):
        row = unreal.UELevelCollisionAsset()
        row.set_editor_property('level', load(binding['level']))
        row.set_editor_property('collision_file', binding['file'])
        collisions.append(row)
    if collisions:
        data.set_editor_property('level_collisions', collisions)
    configure_materials(data, registry)
    if not LIB.save_loaded_asset(data, only_if_is_dirty=False):
        raise RuntimeError('Could not save project asset catalog')
    clear_lobby_travel_overrides()
    REPORT['asset'] = data.get_path_name()
    (OUT / 'assignment-report.json').write_text(
        json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('PROJECT_ASSETS_SAVED ' + json.dumps(REPORT, ensure_ascii=False))


if __name__ == '__main__':
    main()
