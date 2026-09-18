"""Author editable Heat Wave breath and impact assets in a rendering UE 5.8 editor."""
import json
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import create_water_gun_vfx as water

ROOT = '/Game/VFX/Pokemon/HeatWave'
OUT = Path(unreal.Paths.project_saved_dir())/'Codex/HeatWave'
LIB = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def save(asset):
    if not LIB.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Save failed: '+asset.get_path_name())


def material(kind):
    previous = water.ROOT
    water.ROOT = ROOT
    try:
        mat = water.make_material('M_HeatWave_'+kind, 'drop')
    finally:
        water.ROOT = previous
    shapes = {
        'WindSheet': '''
float y=p.y; float envelope=pow(saturate(1-y*y),.8);
float bend=.22*sin(y*3.2+Age*4)+.08*sin(y*7-Age*8);
float width=(.09+.12*(1-y)*.5)*envelope;
float d=abs(p.x-bend);
float sheet=1-smoothstep(width*.35,width+.07,d);
float inner=1-smoothstep(.01,.045,d-width*.35);
float breaks=.5+.5*smoothstep(-.6,.8,sin(y*13+p.x*9-Age*13));
float mask=sheet*envelope*breaks*.62;
float3 col=lerp(float3(.65,.008,.003),float3(1,.15,.015),inner*.65+envelope*.2);
''',
        'Streak': '''
float y=p.y; float wave=.16*sin(y*3.2+Age*3);
float d=abs(p.x-wave); float gate=pow(saturate(1-y*y),1.5);
float mask=(1-smoothstep(.018,.075,d))*gate;
float3 col=lerp(float3(1,.055,.003),float3(1,.39,.055),gate*.8);
''',
        'Curl': '''
float a=atan2(p.y,p.x); float r=length(p);
float radius=.55+.07*sin(a*3+Age*9);
float d=abs(r-radius);
float arc=smoothstep(-.3,.7,sin(a+Age*5));
float mask=(1-smoothstep(.025,.13,d))*arc;
float3 col=lerp(float3(.8,.025,.003),float3(1,.37,.035),1-saturate(d*12));
''',
        'HotAir': '''
float a=atan2(p.y,p.x); float r=length(p);
float eddy=.08*sin(a*4+Age*8)+.06*sin(p.x*8+p.y*6-Age*7);
float mask=pow(saturate(1-r+eddy),2)*.18;
float3 col=lerp(float3(.45,.008,.003),float3(1,.11,.009),saturate(1-r));
''',
        'Ember': '''
float d=abs(p.x)*1.3+abs(p.y)*.6;
float mask=1-smoothstep(.22,.50,d);
float3 col=lerp(float3(1,.09,.003),float3(1,.76,.19),saturate(1-d*2));
''',
        'Flash': '''
float a=atan2(p.y,p.x); float r=length(p);
float edge=.42+.16*sin(a*6)+.06*sin(a*11+Age*9);
float mask=(1-smoothstep(edge-.06,edge+.06,r))*.65;
float3 col=lerp(float3(1,.035,.003),float3(1,.48,.08),saturate(1-r*2));
''',
    }
    custom = unreal.find_object(None, mat.get_path_name()+':MaterialExpressionCustom_0')
    custom.set_editor_property('code', 'float2 p=UV*2-1;\n'+shapes[kind]+
                               '\nreturn float4(col,mask*saturate((1-Age)*3));')
    gain = unreal.find_object(None, mat.get_path_name()+':MaterialExpressionScalarParameter_0')
    gain.set_editor_property('default_value', 1.8 if kind in ('Streak', 'Ember', 'Flash') else 1.25)
    MEL.recompile_material(mat)
    save(mat)
    return mat


def layer(system, label, mat, duplicate, impact, count, life, size, radius, speed, angle, alpha, stretch=0):
    water.configure_layer(system, label, duplicate, mat, impact, count, life, size,
                          radius, speed, angle, 0, alpha)
    emitter = water.EDIT.water_layer(system, label, False)
    mods = {m.get_class().get_name().replace('NiagaraStatelessModule_', ''): m
            for m in emitter.get_editor_property('modules')}
    renderer = unreal.find_object(None, emitter.get_path_name()+'.Renderer')
    # A breath originates at the mouth and flows forwards (+X), unlike a projectile tail.
    if not impact:
        water.setp(mods['AddVelocity'], 'ConeDirection', water.vector((1, 0, 0)))
    renderer.set_editor_property('alignment', unreal.NiagaraSpriteAlignment.VELOCITY_ALIGNED
                                if stretch else unreal.NiagaraSpriteAlignment.UNALIGNED)
    if stretch:
        width, length = size[0], stretch
        water.setp(mods['InitializeParticle'], 'SpriteSizeDistribution',
                   f'(Min=(X={width},Y={length}),Max=(X={width},Y={length}),'
                   f'Mode=NonUniformConstant,ChannelConstantsAndRanges=({width},{length}))')
        water.setp(mods['InitializeParticle'], 'SpriteRotationDistribution', water.scalar(0))
    grow = mods['ScaleSpriteSize']
    water.setp(grow, 'bModuleEnabled', 'True')
    water.setp(grow, 'ScaleDistribution',
               '(Mode=UniformCurve,ChannelConstantsAndRanges=,ChannelCurves=((Keys='
               '((Time=0,Value=0.35),(Time=0.25,Value=0.85),(Time=1,Value=1.6)))))')
    water.setp(emitter, 'FixedBounds',
               '(Min=(X=-350,Y=-450,Z=-450),Max=(X=1000,Y=450,Z=450),IsValid=1)')


def showcase():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    location, rotation = editor.get_level_viewport_camera_info()
    state = {'map': editor.get_editor_world().get_path_name().split('.')[0],
             'location': [location.x, location.y, location.z],
             'rotation': [rotation.pitch, rotation.yaw, rotation.roll]}
    (OUT/'previous_view.json').write_text(json.dumps(state), encoding='utf8')
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    # Bind Sequencer to the persistent map path, never to /Temp/Untitled.
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, ROOT+'/L_HeatWave_Showcase'):
        raise RuntimeError('Initial showcase save failed')
    sequence = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'LS_HeatWave_Preview', ROOT, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    sequence.set_display_rate(unreal.FrameRate(60, 1))
    sequence.set_playback_start(0)
    sequence.set_playback_end(120)
    for suffix, x in [('Attack', 0), ('Impact', 470)]:
        actor = actors.spawn_actor_from_class(LIB.load_blueprint_class(ROOT+'/BP_HeatWave_'+suffix),
                                             unreal.Vector(x, 0, 0))
        actor.set_actor_label('HeatWave_'+suffix)
        actor.set_editor_property('sprite_scale', 0.0)
        for arrow in actor.get_components_by_class(unreal.ArrowComponent):
            arrow.set_visibility(False)
        component = actor.get_component_by_class(unreal.NiagaraComponent)
        parent = sequence.add_possessable(actor)
        binding = sequence.add_possessable(component)
        binding.set_parent(parent)
        section = binding.add_track(unreal.MovieSceneNiagaraSystemTrack).add_section()
        section.set_range(36 if suffix == 'Impact' else 0, 108)
        section.set_editor_property('age_update_mode', unreal.NiagaraAgeUpdateMode.DESIRED_AGE)
    save(sequence)
    actor = actors.spawn_actor_from_class(unreal.LevelSequenceActor, unreal.Vector())
    actor.set_sequence(sequence)
    actor.set_actor_label('HeatWave_PreviewSequence')
    actor.set_editor_property('sprite_scale', 0.0)
    editor.set_level_viewport_camera_info(unreal.Vector(250, -740, 150),
                                          unreal.Rotator(pitch=-10, yaw=90, roll=0))
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, ROOT+'/L_HeatWave_Showcase'):
        raise RuntimeError('Showcase save failed')
    unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(sequence)
    unreal.LevelSequenceEditorBlueprintLibrary.set_current_time(45)
    unreal.EditorLevelLibrary.editor_invalidate_viewports()
    unreal.log('HEAT WAVE SHOWCASE SAVED')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if LIB.does_directory_exist(ROOT) and LIB.list_assets(ROOT, recursive=True):
        raise RuntimeError('Existing Heat Wave assets retained; edit them directly.')
    mats = {k: material(k) for k in ('WindSheet', 'Streak', 'Curl', 'HotAir', 'Ember', 'Flash')}
    # label, material, rate/burst, lifetime, size, radius, speed, cone, alpha, stretch length
    specs = {
        'Attack': [
            ('RedWindSheets', 'WindSheet', 48, (.55, .8), (130, 130), 18, (450, 610), 38, .8, 200),
            ('HotStreamlines', 'Streak', 26, (.4, .65), (30, 30), 20, (530, 740), 32, .65, 220),
            ('HeatedAir', 'HotAir', 55, (.45, .75), (130, 190), 20, (340, 490), 40, .8, 0),
            ('CarriedEmbers', 'Ember', 65, (.4, .7), (5, 10), 18, (480, 760), 36, 1, 0),
        ],
        'Impact': [
            ('ScatteringGusts', 'Curl', 14, (.24, .52), (70, 125), 15, (100, 230), 0, .9, 0),
            ('RedPressureCloud', 'HotAir', 9, (.3, .62), (95, 145), 12, (60, 150), 0, .9, 0),
            ('WindShear', 'Streak', 12, (.18, .38), (23, 23), 10, (150, 290), 0, .8, 110),
            ('HotContact', 'Flash', 1, (.16, .16), (150, 150), 0, (0, 0), 0, .9, 0),
            ('ScatteredEmbers', 'Ember', 35, (.28, .62), (5, 11), 10, (130, 320), 0, 1, 0),
        ],
    }
    report = []
    for suffix, layers in specs.items():
        system = LIB.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/FountainLightweight',
                                     ROOT+'/NS_HeatWave_'+suffix)
        if not system:
            raise RuntimeError('Niagara creation failed')
        for i, (label, kind, *values) in enumerate(layers):
            layer(system, label, mats[kind], i > 0, suffix == 'Impact', *values)
        if not water.EDIT.finish_water_system(system):
            raise RuntimeError('Niagara compile failed: '+suffix)
        save(system)
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.NiagaraActor)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'BP_HeatWave_'+suffix, ROOT, unreal.Blueprint, factory)
        defaults = unreal.get_default_object(bp.generated_class())
        defaults.get_component_by_class(unreal.NiagaraComponent).set_asset(system)
        if suffix == 'Impact':
            defaults.set_editor_property('initial_life_span', 1.5)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        save(bp)
        report.append({'system': system.get_path_name(), 'looping': suffix == 'Attack',
                       'layers': [x[0] for x in layers]})
    (OUT/'authored.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    showcase()
    unreal.log('HEAT WAVE VFX AUTHORED')


if __name__ == '__main__':
    main()
