"""Author Bubble Beam sprites, world-space trail emitters and a separate impact.

Run main(), finish(), then showcase() in the editor. Existing completed assets are retained.
"""
import json
import re
import struct
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import create_water_gun_vfx as water

ROOT = '/Game/VFX/Pokemon/BubbleBeam'
OUT = Path(unreal.Paths.project_saved_dir())/'Codex/BubbleBeam'
LIB = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def save(asset):
    if not LIB.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Save failed: '+asset.get_path_name())


def material(kind):
    previous = water.ROOT
    water.ROOT = ROOT
    try:
        mat = water.make_material('M_BubbleBeam_'+kind, 'drop')
    finally:
        water.ROOT = previous
    shapes = {
        'Bubble': '''
float r=length(p); float grow=lerp(.52,.78,smoothstep(0,.22,Age));
float pop=smoothstep(.78,1,Age); float radius=grow+pop*.14;
float d=abs(r-radius); float rim=1-smoothstep(.025,.07,d);
float body=(1-smoothstep(radius-.02,radius,r))*.12*(1-pop);
float shine=exp(-dot((p-float2(-.27,-.32))*float2(9,6),(p-float2(-.27,-.32))*float2(9,6)))*.9;
float crescent=(1-smoothstep(.01,.055,abs(r-radius*.76)))*smoothstep(.1,.55,-p.x-p.y);
float mask=max(rim*.82,body)+shine+crescent*.6*(1-pop);
mask*=1-pop;
float3 col=lerp(float3(.07,.59,.88),float3(.88,.99,1),saturate(shine+crescent+rim*.55));
''',
        'Foam': '''
float r=length(p); float mask=1-smoothstep(.35,.65,r);
float3 col=lerp(float3(.16,.71,.96),float3(.86,1,1),saturate(1-r*1.8));
''',
        'PopRing': '''
float r=length(p); float radius=lerp(.15,.88,Age);
float mask=(1-smoothstep(.025,.075,abs(r-radius)))*saturate((1-Age)*2);
float3 col=float3(.48,.91,1);
''',
        'Splash': '''
float a=atan2(p.y,p.x); float r=length(p);
float edge=.5+.13*sin(a*8)+.065*sin(a*13+1);
float mask=(1-smoothstep(edge-.06,edge+.03,r))*.8;
float3 col=lerp(float3(.08,.65,.97),float3(.79,.99,1),saturate(1-r*1.4));
''',
    }
    custom = unreal.find_object(None, mat.get_path_name()+':MaterialExpressionCustom_0')
    custom.set_editor_property('code', 'float2 p=UV*2-1;\n'+shapes[kind]+
                               '\nreturn float4(col,saturate(mask)*saturate((1-Age)*4));')
    gain = unreal.find_object(None, mat.get_path_name()+':MaterialExpressionScalarParameter_0')
    gain.set_editor_property('default_value', 1.25 if kind == 'Bubble' else 1.5)
    MEL.recompile_material(mat)
    save(mat)
    return mat


def configure_emitter(emitter, mat, fine=False):
    # Preserve the native module graph and every unrelated typed parameter. Export
    # offsets describe the current engine version, so no offsets/type IDs are hardcoded.
    task = unreal.AssetExportTask()
    task.object = emitter
    task.filename = str(OUT/(emitter.get_name()+'.t3d'))
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.ObjectExporterT3D()
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError('Emitter export failed')
    text = Path(task.filename).read_text()
    if 'bLocalSpace=True' in text:
        raise RuntimeError('Trail requires a world-space emitter')
    overrides = {
        'SpawnRate.SpawnRate': (100 if fine else 65,),
        'InitializeParticle.Lifetime Min': (.30 if fine else .38,),
        'InitializeParticle.Lifetime Max': (.55 if fine else .72,),
        'InitializeParticle.Uniform Sprite Size Min': (3 if fine else 12,),
        'InitializeParticle.Uniform Sprite Size Max': (7 if fine else 28,),
        'InitializeParticle.Sprite Rotation Angle Min': (0,),
        'InitializeParticle.Sprite Rotation Angle Max': (0,),
        'ShapeLocation.Sphere Radius': (10 if fine else 14,),
        'RandomRangeFloat.Minimum': (4,),
        'RandomRangeFloat.Maximum': (18,),
        'AddVelocity.Cone Angle': (160,),
        'AddVelocity.Velocity': (0, 0, 0),
        'GravityForce.Gravity': (0, 0, 12),
        'Drag.Drag': (.8,),
    }
    changed = set()
    for name in ('EmitterUpdateScript', 'SpawnScript', 'UpdateScript'):
        block = re.search(r'Begin Object Name="'+name+r'".*?RapidIterationParameters=(.*?)\n', text, re.S).group(1)
        data = bytearray(map(int, re.search(r'ParameterData=\(([^)]*)\)', block).group(1).split(',')))
        entries = list(re.finditer(r'Offset=(\d+),Name="([^"]+)"', block.split('ParameterData=')[0]))
        for i, entry in enumerate(entries):
            key = entry[2].split('.', 2)[-1]
            if key not in overrides:
                continue
            start = int(entry[1])
            end = int(entries[i+1][1]) if i+1 < len(entries) else len(data)
            value = struct.pack('<'+'f'*len(overrides[key]), *overrides[key])
            if len(value) != end-start:
                raise RuntimeError('Parameter size mismatch: '+key)
            data[start:end] = value
            changed.add(key)
        block = re.sub(r'ParameterData=\([^)]*\)', 'ParameterData=('+','.join(map(str, data))+')', block)
        script = unreal.find_object(None, emitter.get_path_name()+':'+name)
        water.setp(script, 'RapidIterationParameters', block)
    if changed != set(overrides):
        raise RuntimeError('Missing Niagara inputs: '+str(set(overrides)-changed))
    renderer = unreal.find_object(None, emitter.get_path_name()+':NiagaraSpriteRendererProperties_0')
    renderer.set_editor_property('material', mat)
    save(emitter)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if LIB.does_asset_exist(ROOT+'/BP_BubbleBeam_Impact'):
        raise RuntimeError('Finished Bubble Beam assets retained')
    mats = {k: material(k) for k in ('Bubble', 'Foam', 'PopRing', 'Splash')}
    for suffix, kind in [('Bubbles', 'Bubble'), ('FineFoam', 'Foam')]:
        path = ROOT+'/NE_BubbleBeam_'+suffix
        emitter = LIB.load_asset(path) if LIB.does_asset_exist(path) else LIB.duplicate_asset(
            '/Niagara/DefaultAssets/Templates/Emitters/Fountain', path)
        configure_emitter(emitter, mats[kind], suffix == 'FineFoam')
    if not LIB.does_asset_exist(ROOT+'/NS_BubbleBeam_Projectile'):
        system = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'NS_BubbleBeam_Projectile', ROOT, unreal.NiagaraSystem, unreal.NiagaraSystemFactoryNew())
        save(system)
    projectile = LIB.load_asset(ROOT+'/NS_BubbleBeam_Projectile')
    for name in ('Bubbles', 'FineFoam'):
        if not water.EDIT.add_standard_vfx_layer(projectile, LIB.load_asset(ROOT+'/NE_BubbleBeam_'+name)):
            raise RuntimeError('Cannot add standard emitter: '+name)
    impact = LIB.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/FountainLightweight', ROOT+'/NS_BubbleBeam_Impact')
    if not impact:
        raise RuntimeError('Impact exists or creation failed')
    layers = [
        ('BurstingBubbles', 'Bubble', 32, (.25, .65), (14, 38), 10, (100, 230), 0, 25, 1),
        ('WhiteFoam', 'Foam', 45, (.16, .4), (3, 9), 8, (130, 290), 0, -70, .9),
        ('BubblePops', 'PopRing', 10, (.18, .32), (22, 42), 18, (70, 140), 0, 0, .8),
        ('WaterContact', 'Splash', 1, (.14, .14), (95, 95), 0, (0, 0), 0, 0, .8),
    ]
    for i, (label, kind, *values) in enumerate(layers):
        water.configure_layer(impact, label, i > 0, mats[kind], True, *values)
    if not water.EDIT.finish_water_system(impact):
        raise RuntimeError('Impact compile failed')
    save(impact)
    unreal.log('BUBBLE BEAM EMITTERS AUTHORED')


def finish():
    for suffix in ('Projectile', 'Impact'):
        system = LIB.load_asset(ROOT+'/NS_BubbleBeam_'+suffix)
        if not water.EDIT.finish_water_system(system):
            raise RuntimeError('Niagara compile failed: '+suffix)
        save(system)
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.NiagaraActor)
        path = ROOT+'/BP_BubbleBeam_'+suffix
        bp = LIB.load_asset(path) if LIB.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'BP_BubbleBeam_'+suffix, ROOT, unreal.Blueprint, factory)
        defaults = unreal.get_default_object(bp.generated_class())
        defaults.get_component_by_class(unreal.NiagaraComponent).set_asset(system)
        if suffix == 'Impact':
            defaults.set_editor_property('initial_life_span', 1.5)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        save(bp)
    (OUT/'authored.json').write_text(json.dumps({'projectile': 'world-space, stationary origin, looping',
                                               'impact': 'one-shot', 'ready': True}), encoding='utf8')
    unreal.log('BUBBLE BEAM VFX AUTHORED')


def showcase():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    loc, rot = editor.get_level_viewport_camera_info()
    (OUT/'previous_view.json').write_text(json.dumps({
        'map': editor.get_editor_world().get_path_name().split('.')[0],
        'location': [loc.x, loc.y, loc.z], 'rotation': [rot.pitch, rot.yaw, rot.roll]}))
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    map_path = ROOT+'/L_BubbleBeam_Showcase'
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
        raise RuntimeError('Map save failed')
    seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'LS_BubbleBeam_Preview', ROOT, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(unreal.FrameRate(60, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(120)
    for label, suffix, x, z in [('Stationary', 'Projectile', -180, 130),
                                 ('MovingTrail', 'Projectile', -260, -90),
                                 ('Impact', 'Impact', 300, -90)]:
        actor = actors.spawn_actor_from_class(LIB.load_blueprint_class(ROOT+'/BP_BubbleBeam_'+suffix),
                                             unreal.Vector(x, 0, z))
        actor.set_actor_label('BubbleBeam_'+label)
        actor.set_editor_property('sprite_scale', 0.0)
        for arrow in actor.get_components_by_class(unreal.ArrowComponent):
            arrow.set_visibility(False)
        parent = seq.add_possessable(actor)
        component = actor.get_component_by_class(unreal.NiagaraComponent)
        binding = seq.add_possessable(component)
        binding.set_parent(parent)
        section = binding.add_track(unreal.MovieSceneNiagaraSystemTrack).add_section()
        section.set_range(60 if suffix == 'Impact' else 0, 115)
        section.set_editor_property('age_update_mode', unreal.NiagaraAgeUpdateMode.DESIRED_AGE)
        if label == 'MovingTrail':
            transform = parent.add_track(unreal.MovieScene3DTransformTrack).add_section()
            transform.set_range(0, 120)
            channels = transform.get_all_channels()
            for channel, value in zip(channels, [x, 0, z, 0, 0, 0, 1, 1, 1]):
                channel.set_default(value)
            for frame, value in [(0, x), (60, 300), (120, 300)]:
                channels[0].add_key(unreal.FrameNumber(frame), value,
                                    interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)
    save(seq)
    actor = actors.spawn_actor_from_class(unreal.LevelSequenceActor, unreal.Vector())
    actor.set_sequence(seq)
    actor.set_editor_property('sprite_scale', 0.0)
    actor.set_actor_label('BubbleBeam_PreviewSequence')
    exposure = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, -10000))
    exposure.set_actor_label('BubbleBeam_PreviewExposure')
    exposure.set_editor_property('unbound', True)
    settings = exposure.get_editor_property('settings')
    for name, value in [('override_auto_exposure_min_brightness', True),
                        ('override_auto_exposure_max_brightness', True),
                        ('auto_exposure_min_brightness', 1.0), ('auto_exposure_max_brightness', 1.0),
                        ('override_auto_exposure_bias', True), ('auto_exposure_bias', 0.0)]:
        settings.set_editor_property(name, value)
    exposure.set_editor_property('settings', settings)
    editor.set_level_viewport_camera_info(unreal.Vector(50, 600, 150),
                                          unreal.Rotator(pitch=-12, yaw=-90, roll=0))
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
        raise RuntimeError('Map save failed')
    unreal.log('BUBBLE BEAM SHOWCASE SAVED')


if __name__ == '__main__':
    main()
