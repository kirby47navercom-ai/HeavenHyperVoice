"""별도 5.8 에디터의 Entry 맵에서 실행. 에셋 연결 검사와 저장하지 않는 렌더 미리보기.
UnrealEditor.exe Project.uproject /Engine/Maps/Entry -ExecutePythonScript=.../verify_instance_weather.py
"""
import json
import random
import time
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ROOT='/Game/VFX/Weather'
OUT=Path(unreal.Paths.project_saved_dir())/'Codex/Weather'
OUT.mkdir(parents=True,exist_ok=True)
LIB=unreal.EditorAssetLibrary
bp=LIB.load_asset(ROOT+'/BP_InstanceWeatherDirector')
assert bp and bp.generated_class()
cdo=unreal.get_default_object(bp.generated_class())
for prop in ('rain','snow','rain_impact','water_ripple','snow_impact','surface_material','parameters'):
    assert cdo.get_editor_property(prop), prop
assets=LIB.load_asset('/Game/Blueprints/DA_ProjectAssets')
assert str(assets.get_editor_property('instance_weather_director_class')).find('BP_InstanceWeatherDirector')>=0
assert LIB.load_asset(ROOT+'/BP_WeatherExclusionVolume').generated_class()

actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level.new_level('/Temp/WeatherVisualVerification')
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
cube=unreal.load_asset('/Engine/BasicShapes/Cube')
def box(label,pos,scale):
    a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos))
    a.set_actor_label(label); a.static_mesh_component.set_static_mesh(cube)
    a.set_actor_scale3d(unreal.Vector(*scale)); return a
box('Outdoor ground',(0,0,-15),(13,13,.3))
box('Shelter roof',(-330,250,260),(4,4,.2))
for x in (-510,-150):
    box('Shelter post',(x,420,125),(.15,.15,2.5))
sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector())
sun.set_actor_rotation(unreal.Rotator(pitch=-40,yaw=-35,roll=0),False)
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sun.light_component.set_intensity(40)
fill=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector())
fill.set_actor_rotation(unreal.Rotator(pitch=-30,yaw=145,roll=0),False)
fill.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
fill.light_component.set_intensity(8)

surface=LIB.load_asset(ROOT+'/Materials/M_Weather_Surface')
for x,snow,wet in [(-320,0,1),(320,1,0)]:
    for i in range(4):
        for j in range(7):
            px=x+(i-1.5)*140; py=(j-3)*150
            if px< -140 and py>50: continue
            a=actors.spawn_actor_from_class(unreal.DecalActor,unreal.Vector(px,py,2),unreal.Rotator(pitch=-90,yaw=0,roll=0))
            d=a.decal; d.set_decal_material(surface); d.set_editor_property('decal_size',unreal.Vector(6,100,100))
            mid=d.create_dynamic_material_instance(); mid.set_scalar_parameter_value('Snow',snow)
            mid.set_scalar_parameter_value('Wetness',wet)

random.seed(47)
components=[]
for kind,x,count in [('Rain',-320,90),('Snow',320,120),('Splash',-320,12),('Ripple',-320,5),('SnowPuff',320,10)]:
    system=LIB.load_asset(ROOT+'/NS_Weather_'+kind)
    assert unreal.UEWaterVFXEditorLibrary.finish_water_system(system), kind
    for i in range(count):
        px=x+random.uniform(-250,250); py=random.uniform(-550,550)
        if px<-140 and py>50: continue
        falling=kind in ('Rain','Snow')
        a=actors.spawn_actor_from_class(unreal.NiagaraActor,unreal.Vector(px,py,620 if falling else 3))
        c=a.get_component_by_class(unreal.NiagaraComponent); c.set_asset(system)
        c.set_editor_property('allow_scalability',False); c.set_force_solo(True)
        speed=2200 if kind=='Rain' else 180
        life=620/speed if falling else .5
        c.set_variable_float('User.FallLifetime',life)
        c.set_variable_vec3('User.FallVelocity',unreal.Vector(20,0,-speed))
        c.activate(True)
        components.append({'component':c,'life':life,'age':random.uniform(0,life)})

capture=actors.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(1450,-1700,1150))
capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(capture.get_actor_location(),unreal.Vector(0,0,160)),False)
component=capture.get_component_by_class(unreal.SceneCaptureComponent2D)
target=unreal.RenderingLibrary.create_render_target2d(world,1280,960,unreal.TextureRenderTargetFormat.RTF_RGBA8)
component.texture_target=target; component.capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
component.capture_every_frame=False; component.capture_on_movement=False; component.fov_angle=50
settings=component.post_process_settings
settings.set_editor_property('override_auto_exposure_method',True)
settings.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True)
settings.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
settings.set_editor_property('override_auto_exposure_bias',True)
settings.set_editor_property('auto_exposure_bias',0)
settings.set_editor_property('override_bloom_intensity',True)
settings.set_editor_property('bloom_intensity',0)
component.post_process_settings=settings
state={'start':time.monotonic(),'capture':False}
def tick(delta):
    try:
        for item in components:
            item['age']+=min(delta,.05)
            c=item['component']
            if item['age']>=item['life']:
                c.reinitialize_system(); c.activate(True); item['age']=0
            c.advance_simulation(1,min(max(delta,.001),.05))
        elapsed=time.monotonic()-state['start']
        if elapsed<35: return
        if not state['capture']:
            component.capture_every_frame=True; state['capture']=True; state['start']=time.monotonic()-32
            return
        component.capture_every_frame=False; component.capture_scene()
        unreal.RenderingLibrary.export_render_target(world,target,str(OUT),'weather-preview.png')
        (OUT/'verified-assets.json').write_text(json.dumps({'bindings':True,'niagara_ready':True,'preview':'weather-preview.png'}))
        unreal.unregister_slate_post_tick_callback(handle); unreal.SystemLibrary.quit_editor()
    except Exception as exc:
        (OUT/'preview-error.txt').write_text(str(exc),encoding='utf8')
        unreal.unregister_slate_post_tick_callback(handle); unreal.SystemLibrary.quit_editor(); raise
handle=unreal.register_slate_post_tick_callback(tick)
