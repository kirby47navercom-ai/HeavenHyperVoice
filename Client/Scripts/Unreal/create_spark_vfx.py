"""Author Spark's attached electric aura and separate one-shot impact in UE 5.8."""
import json
import math
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import create_water_gun_vfx as water

ROOT = '/Game/VFX/Pokemon/Spark'
LIB = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
OUT = Path(unreal.Paths.project_saved_dir()) / 'Codex/Spark'


def save(asset):
    if not LIB.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Save failed: ' + asset.get_path_name())


def node(mat, cls, x, y, **props):
    result = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        result.set_editor_property(k, v)
    return result


def arc_material():
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_Spark_Arc', ROOT+'/Materials', unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)
    edge = node(mat, unreal.MaterialExpressionFresnel, -600, 0, exponent=1.6, base_reflect_fraction=0.0)
    core = node(mat, unreal.MaterialExpressionVectorParameter, -600, 150,
                parameter_name='CoreColor', default_value=unreal.LinearColor(1, 1, .48, 1))
    rim = node(mat, unreal.MaterialExpressionVectorParameter, -600, 300,
               parameter_name='EdgeColor', default_value=unreal.LinearColor(1, .52, .005, 1))
    color = node(mat, unreal.MaterialExpressionLinearInterpolate, -320, 0)
    for src, pin in [(core, 'A'), (rim, 'B'), (edge, 'Alpha')]:
        MEL.connect_material_expressions(src, '', color, pin)
    gain = node(mat, unreal.MaterialExpressionScalarParameter, -300, 220,
                parameter_name='Brightness', default_value=3.0)
    emission = node(mat, unreal.MaterialExpressionMultiply, -80, 0)
    MEL.connect_material_expressions(color, '', emission, 'A')
    MEL.connect_material_expressions(gain, '', emission, 'B')
    particle = node(mat, unreal.MaterialExpressionParticleColor, -80, 240)
    MEL.connect_material_property(emission, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(particle, 'A', unreal.MaterialProperty.MP_OPACITY)
    MEL.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES)
    MEL.recompile_material(mat)
    save(mat)
    return mat


def arc_mesh(mat):
    # A broken arc around a body, with short branches. Random particle orientation
    # wraps these actual 3D segments around the socket from every viewing direction.
    mesh = unreal.DynamicMesh()
    points = []
    for i in range(13):
        angle = math.radians(-65 + i*11)
        radius = 56 + (5 if i % 2 else -3)
        points.append(unreal.Vector(radius*math.cos(angle), radius*math.sin(angle),
                                    5*math.sin(i*2.4)))

    def segment(a, b, thickness):
        delta = b-a
        length = math.sqrt(delta.x**2 + delta.y**2 + delta.z**2)
        rotation = unreal.MathLibrary.make_rot_from_z(delta)
        unreal.GeometryScript_Primitives.append_cylinder(
            mesh, unreal.GeometryScriptPrimitiveOptions(),
            unreal.Transform(location=a, rotation=rotation), radius=thickness,
            height=length, radial_steps=5, height_steps=0, capped=True)

    for i in range(len(points)-1):
        segment(points[i], points[i+1], .65 if 1 < i < 10 else .38)
    for i in (3, 8):
        a = points[i]
        b = a*1.16 + unreal.Vector(0, 0, 7)
        c = a*1.27 + unreal.Vector(0, 4, 1)
        segment(a, b, .38)
        segment(b, c, .20)
    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property('enable_nanite', False)
    options.set_editor_property('enable_collision', False)
    result = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
        mesh, ROOT+'/Meshes/SM_Spark_BranchedArc', options)[0]
    if not result:
        raise RuntimeError('Arc mesh creation failed')
    result.set_material(0, mat)
    save(result)
    return result


def sprite_material(kind):
    previous = water.ROOT
    water.ROOT = ROOT
    try:
        mat = water.make_material('M_Spark_'+kind, 'drop')
    finally:
        water.ROOT = previous
    shapes = {
        'Bolt': '''
float y=p.y; float t=saturate((y+.85)/1.7)*7;
float k=floor(t); float f=frac(t);
float x0=sin(k*19.17+1.3)*.26; float x1=sin((k+1)*19.17+1.3)*.26;
float d=abs(p.x-lerp(x0,x1,f));
float gate=1-smoothstep(.72,.88,abs(y));
float core=1-smoothstep(.012,.035,d);
float mask=(core+.18*(1-smoothstep(.035,.14,d)))*gate;
float3 col=lerp(float3(1,.57,.008),float3(1,1,.65),core);
''',
        'Flash': '''
float a=atan2(p.y,p.x); float r=length(p);
float rays=pow(abs(cos(a*5+0.3)),18);
float edge=.19+.66*rays;
float mask=1-smoothstep(edge-.025,edge+.025,r);
float3 col=lerp(float3(1,.68,.01),float3(1,1,.73),1-smoothstep(.05,.38,r));
''',
        'Fleck': '''
float d=abs(p.x)*1.2+abs(p.y)*.7;
float mask=1-smoothstep(.18,.38,d);
float3 col=float3(1,.95,.31);
''',
        'Glow': '''
float mask=pow(saturate(1-length(p)),3)*.16;
float3 col=float3(1,.78,.015);
''',
    }
    custom = unreal.find_object(None, mat.get_path_name()+':MaterialExpressionCustom_0')
    custom.set_editor_property('code', 'float2 p=UV*2-1;\n'+shapes[kind]+
                               '\nreturn float4(col,mask*saturate((1-Age)*4));')
    gain = unreal.find_object(None, mat.get_path_name()+':MaterialExpressionScalarParameter_0')
    gain.set_editor_property('default_value', 2.7 if kind != 'Glow' else 1.5)
    MEL.recompile_material(mat)
    save(mat)
    return mat


def add_arcs(system, mesh, mat, burst):
    name = 'ContactArcs' if burst else 'BodyArcs'
    water.configure_layer(system, name, False, mat, burst, 9 if burst else 38,
                          (.1, .24) if burst else (.08, .17), (1, 1), 0, (0, 0))
    emitter = water.EDIT.water_layer(system, name, False)
    for mod in emitter.get_editor_property('modules'):
        key = mod.get_class().get_name().replace('NiagaraStatelessModule_', '')
        water.setp(mod, 'bModuleEnabled', str(key in {
            'InitializeParticle', 'InitialMeshOrientation', 'ScaleColor', 'ApplyOwnerScaleToAttributes'}))
        if key == 'InitializeParticle':
            water.setp(mod, 'MeshScaleDistribution', water.vector((1, 1, 1)))
        if key == 'InitialMeshOrientation':
            water.setp(mod, 'MeshOrientationMode', 'Random')
    renderer = unreal.NiagaraMeshRendererProperties(outer=emitter, name='SparkArcRenderer')
    info = unreal.NiagaraMeshRendererMeshProperties()
    info.set_editor_property('mesh', mesh)
    renderer.set_editor_property('meshes', [info])
    water.setp(emitter, 'RendererProperties', "(NiagaraMeshRendererProperties'"+renderer.get_path_name()+"')")


def showcase():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    location, rotation = editor.get_level_viewport_camera_info()
    previous = {'map': editor.get_editor_world().get_path_name().split('.')[0],
                'location': [location.x, location.y, location.z],
                'rotation': [rotation.pitch, rotation.yaw, rotation.roll]}
    (OUT/'previous_view.json').write_text(json.dumps(previous), encoding='utf8')
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    sequence = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'LS_Spark_Preview', ROOT, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    sequence.set_display_rate(unreal.FrameRate(60, 1))
    sequence.set_playback_start(0)
    sequence.set_playback_end(120)
    for suffix, x in [('Aura', 0), ('Impact', 230)]:
        actor = actors.spawn_actor_from_class(LIB.load_blueprint_class(ROOT+'/BP_Spark_'+suffix),
                                             unreal.Vector(x, 0, 0))
        actor.set_actor_label('Spark_'+suffix)
        actor.set_editor_property('sprite_scale', 0.0)
        for arrow in actor.get_components_by_class(unreal.ArrowComponent):
            arrow.set_visibility(False)
        component = actor.get_component_by_class(unreal.NiagaraComponent)
        parent = sequence.add_possessable(actor)
        binding = sequence.add_possessable(component)
        binding.set_parent(parent)
        section = binding.add_track(unreal.MovieSceneNiagaraSystemTrack).add_section()
        section.set_range(36 if suffix == 'Impact' else 0, 120)
        section.set_editor_property('age_update_mode', unreal.NiagaraAgeUpdateMode.DESIRED_AGE)
    save(sequence)
    seq_actor = actors.spawn_actor_from_class(unreal.LevelSequenceActor, unreal.Vector())
    seq_actor.set_sequence(sequence)
    seq_actor.set_actor_label('Spark_PreviewSequence')
    seq_actor.set_editor_property('sprite_scale', 0.0)
    editor.set_level_viewport_camera_info(unreal.Vector(115, -380, 100),
                                          unreal.Rotator(pitch=-14, yaw=90, roll=0))
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, ROOT+'/L_Spark_Showcase'):
        raise RuntimeError('Showcase save failed')
    unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(sequence)
    unreal.LevelSequenceEditorBlueprintLibrary.set_current_time(42)
    unreal.EditorLevelLibrary.editor_invalidate_viewports()
    unreal.log('SPARK SHOWCASE SAVED')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if LIB.does_directory_exist(ROOT) and LIB.list_assets(ROOT, recursive=True):
        raise RuntimeError('Existing Spark assets retained; edit them in the editor.')
    arc = arc_material()
    mesh = arc_mesh(arc)
    mats = {k: sprite_material(k) for k in ('Bolt', 'Flash', 'Fleck', 'Glow')}
    specs = {
        'Aura': [
            ('SurfaceFlecks', 'Fleck', 32, (.10, .22), (3, 8), 57, (0, 0), 0, 0, .9),
            ('ChargeGlow', 'Glow', 9, (.12, .20), (155, 175), 0, (0, 0), 0, 0, .6),
        ],
        'Impact': [
            ('ContactFlash', 'Flash', 1, (.12, .12), (170, 170), 0, (0, 0), 0, 0, 1),
            ('DischargeBolts', 'Bolt', 14, (.12, .29), (33, 62), 13, (110, 270), 0, 0, 1),
            ('ScatteredCharge', 'Fleck', 26, (.20, .48), (3, 8), 8, (130, 300), 0, -80, 1),
            ('ImpactGlow', 'Glow', 1, (.20, .20), (220, 220), 0, (0, 0), 0, 0, 1),
        ],
    }
    report = []
    for suffix, layers in specs.items():
        system = LIB.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/FountainLightweight',
                                     ROOT+'/NS_Spark_'+suffix)
        if not system:
            raise RuntimeError('Niagara creation failed')
        add_arcs(system, mesh, arc, suffix == 'Impact')
        # The helper duplicates the first layer, so restore a sprite renderer on each sprite layer.
        for label, kind, count, life, size, radius, speed, angle, gravity, alpha in layers:
            emitter = water.EDIT.water_layer(system, label, True)
            renderer = unreal.find_object(None, emitter.get_path_name()+'.Renderer')
            if not renderer:
                renderer = unreal.NiagaraSpriteRendererProperties(outer=emitter, name='Renderer')
            water.setp(emitter, 'RendererProperties', "(NiagaraSpriteRendererProperties'"+renderer.get_path_name()+"')")
            water.configure_layer(system, label, False, mats[kind], suffix == 'Impact',
                                  count, life, size, radius, speed, angle, gravity, alpha)
        if not water.EDIT.finish_water_system(system):
            raise RuntimeError('Niagara compile failed: '+suffix)
        save(system)
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.NiagaraActor)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'BP_Spark_'+suffix, ROOT, unreal.Blueprint, factory)
        defaults = unreal.get_default_object(bp.generated_class())
        defaults.get_component_by_class(unreal.NiagaraComponent).set_asset(system)
        if suffix == 'Impact':
            defaults.set_editor_property('initial_life_span', 1.5)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        save(bp)
        report.append({'system': system.get_path_name(), 'looping': suffix == 'Aura',
                       'layers': ['BodyArcs' if suffix == 'Aura' else 'ContactArcs']+[x[0] for x in layers]})
    (OUT/'authored.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    showcase()
    unreal.log('SPARK VFX AUTHORED')


if __name__ == '__main__':
    main()
