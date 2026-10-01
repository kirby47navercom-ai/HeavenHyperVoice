"""독립 날씨 라이브러리의 실제 Unreal 렌더 사진을 저장해요.
저장하지 않는 임시 레벨만 만들며 게임/서버/PIE를 실행하지 않아요.
출력은 현재 프로젝트의 Saved 아래에 두어요.
"""
import json
import sys
import time
from pathlib import Path
import unreal

sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_weather_asset_library as library

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
OUT=Path(unreal.Paths.project_saved_dir())/'Codex/WeatherAssetLibrary'
OUT.mkdir(parents=True,exist_ok=True)
LIB=library.LIB
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.new_level('/Temp/WeatherLibraryPreview_'+str(time.time_ns()))
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def mesh(label,path,position,scale,material=None):
    a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*position))
    a.set_actor_label(label);a.static_mesh_component.set_static_mesh(LIB.load_asset(path))
    a.set_actor_scale3d(unreal.Vector(*scale))
    if material:a.static_mesh_component.set_material(0,material)
    return a


ground=mesh('Surface material sample','/Engine/BasicShapes/Plane',(0,0,0),(18,18,1))
stones=[]
for i,(x,y,scale) in enumerate([(-430,180,1.9),(370,380,2.7),(450,100,1.2)]):
    stone=mesh('Surface context rock','/Game/Fab/WaterMaterials/Meshes/SM_River_Rock',(x,y,0),(scale,scale,scale))
    stone.set_actor_rotation(unreal.Rotator(0,i*67,0),False);stones.append(stone)

sky=actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector())
sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky.light_component.set_editor_property('cubemap',LIB.load_asset('/Game/Fab/WaterMaterials/Textures/T_Cubemap'))
sky.light_component.set_intensity(2);sky.light_component.recapture_sky()
actors.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector())

sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector())
sun.set_actor_rotation(unreal.Rotator(-38,-35,0),False)
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);sun.light_component.set_intensity(6)
sun.light_component.set_editor_property('atmosphere_sun_light',True)
sun.light_component.set_light_color(unreal.LinearColor(1,.92,.82))
fill=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector())
fill.set_actor_rotation(unreal.Rotator(-30,145,0),False)
fill.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);fill.light_component.set_intensity(2)
fill.light_component.set_editor_property('cast_shadows',False)
fill.light_component.set_light_color(unreal.LinearColor(.55,.75,1))

capture=actors.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(280,-740,520))
component=capture.get_component_by_class(unreal.SceneCaptureComponent2D)
target=unreal.RenderingLibrary.create_render_target2d(world,1200,800,unreal.TextureRenderTargetFormat.RTF_RGBA8)
component.texture_target=target;component.capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
component.capture_every_frame=False;component.capture_on_movement=False;component.fov_angle=57
component.always_persist_rendering_state=True
component.show_flag_settings=[unreal.EngineShowFlagsSetting(show_flag_name=name,enabled=True) for name in ('Particles','Niagara','Atmosphere','SkyLighting','ReflectionEnvironment')]
settings=component.post_process_settings
for name,value in [('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL),('auto_exposure_apply_physical_camera_exposure',False),('auto_exposure_bias',0),('bloom_intensity',.08)]:
    settings.set_editor_property('override_'+name,True);settings.set_editor_property(name,value)
for name,value in [('dynamic_global_illumination_method',unreal.DynamicGlobalIlluminationMethod.LUMEN),('reflection_method',unreal.ReflectionMethod.LUMEN)]:
    settings.set_editor_property('override_'+name,True);settings.set_editor_property(name,value)
component.post_process_settings=settings

shots=[(name,None) for name in library.PRESETS]+[
    ('WetSoil','GroundMist'),('DrySand','SandDrift'),('SnowSoil','SnowDrift')]
state={'index':0,'start':time.monotonic(),'fx':None,'particles':0,'ready':[]}


def prepare(index):
    name,effect=shots[index]
    ground.static_mesh_component.set_material(0,LIB.load_asset(library.SURFACES+'/MI_Surface_'+name))
    stone_name=('FrostStone' if 'Snow' in name or 'Frost' in name else
                'WetStone' if 'Wet' in name or 'Puddle' in name or 'Thaw' in name else 'DryStone')
    for stone in stones:stone.static_mesh_component.set_material(0,LIB.load_asset(library.SURFACES+'/MI_Surface_'+stone_name))
    if state['fx']:
        actors.destroy_actor(state['fx'].get_owner());state['fx']=None
    if effect:
        bp=LIB.load_asset(library.BLUEPRINTS+'/BP_Atmosphere_'+effect);assert bp
        actor=actors.spawn_actor_from_class(bp.generated_class(),unreal.Vector(-100,100,65))
        actor.set_actor_location(unreal.Vector(-100,100,65),False,False)
        fx=actor.get_component_by_class(unreal.NiagaraComponent)
        assert fx.get_asset().get_path_name().split('.')[0]==library.SYSTEMS+'/NS_Atmosphere_'+effect
        assert unreal.UEWaterVFXEditorLibrary.finish_water_system(fx.get_asset())
        fx.set_editor_property('allow_scalability',False);fx.set_force_solo(True)
        fx.reinitialize_system();fx.activate(True);state['fx']=fx
        capture.set_actor_location(unreal.Vector(0,-800,230),False,False)
    else:capture.set_actor_location(unreal.Vector(280,-740,520),False,False)
    capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(capture.get_actor_location(),unreal.Vector(0,100,0)),False)
    state['start']=time.monotonic();state['particles']=0


prepare(0)


def tick(delta):
    try:
        if state['fx']:
            state['particles']=unreal.UEWaterVFXEditorLibrary.tick_water_preview(state['fx'],delta)
        component.capture_scene()
        wait=35 if state['index']==0 else 7 if state['fx'] else 3
        if time.monotonic()-state['start']<wait:return
        name,effect=shots[state['index']]
        if effect:assert state['particles']>0,effect+' 입자가 생성되지 않았어요'
        filename=(effect or name)+'.png'
        unreal.RenderingLibrary.export_render_target(world,target,str(OUT),filename)
        state['ready'].append({'image':filename,'preset':name,'effect':effect,'particles':state['particles']})
        state['index']+=1
        if state['index']<len(shots):prepare(state['index']);return
        (OUT/'preview.json').write_text(json.dumps({'shots':state['ready'],'game_maps_saved':False},ensure_ascii=False,indent=2),encoding='utf8')
        unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
    except Exception as exc:
        (OUT/'preview-error.txt').write_text(str(exc),encoding='utf8')
        unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor();raise


handle=unreal.register_slate_post_tick_callback(tick)
