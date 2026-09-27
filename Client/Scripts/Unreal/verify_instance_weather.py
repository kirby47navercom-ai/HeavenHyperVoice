"""별도 5.8 에디터의 Entry 맵에서 실행. 에셋 연결 검사와 저장하지 않는 렌더 미리보기.
UnrealEditor.exe Project.uproject /Engine/Maps/Entry -ExecutePythonScript=.../verify_instance_weather.py
"""
import json
import random
import time
import sys
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
for prop in ('rain','snow','rain_impact','wall_splash','water_ripple','snow_impact','snow_chunks','wet_impact_material','surface_material','parameters'):
    assert cdo.get_editor_property(prop), prop
assets=LIB.load_asset('/Game/Blueprints/DA_ProjectAssets')
assert str(assets.get_editor_property('instance_weather_director_class')).find('BP_InstanceWeatherDirector')>=0
assert LIB.load_asset(ROOT+'/BP_WeatherExclusionVolume').generated_class()
ground=LIB.load_asset('/Game/InstanceMap/Plain/Landscape/M_Landscape_GrassSoil')
fn=LIB.load_asset(ROOT+'/Materials/MF_WeatherSurface')
for prop in (unreal.MaterialProperty.MP_BASE_COLOR,unreal.MaterialProperty.MP_ROUGHNESS):
    call=unreal.MaterialEditingLibrary.get_material_property_input_node(ground,prop)
    assert isinstance(call,unreal.MaterialExpressionMaterialFunctionCall) and call.get_editor_property('material_function')==fn

actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.new_level('/Temp/WeatherVisualVerification_'+str(time.time_ns()))
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
for kind,x,count in [('Rain',-320,90),('Snow',320,120),('Splash',-320,12),('WallSplash',-320,4),('Ripple',-320,5),('SnowPuff',320,10),('SnowChunks',320,10)]:
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

# 공용 함수 자체의 젖음/눈/차단을 같은 조명에서 비교한다. 임시 머티리얼은 저장하지 않는다.
sys.path.insert(0,str(Path(__file__).resolve().parent))
import extend_instance_weather as ext
probe=unreal.new_object(unreal.Material)
call=ext.node(probe,unreal.MaterialExpressionMaterialFunctionCall,400,0,material_function=fn)
color=ext.node(probe,unreal.MaterialExpressionConstant3Vector,0,0,constant=unreal.LinearColor(.3,.2,.1,1))
rough=ext.node(probe,unreal.MaterialExpressionConstant,0,180,r=.8)
normal=ext.node(probe,unreal.MaterialExpressionVertexNormalWS,0,360)
position=ext.node(probe,unreal.MaterialExpressionWorldPosition,0,540)
mask=ext.custom(probe,'return step(0,sin(P.x*.025)*sin(P.y*.025));',{'P':position},200,540,scalar=True)
for n,pin in ((color,'BaseColor'),(rough,'Roughness'),(normal,'WorldNormal'),(mask,'PuddleMask')):ext.link(n,call,pin)
ext.MEL.connect_material_property(call,'BaseColor',unreal.MaterialProperty.MP_BASE_COLOR)
ext.MEL.connect_material_property(call,'Roughness',unreal.MaterialProperty.MP_ROUGHNESS)
ext.MEL.recompile_material(probe)
box('Function ground',(0,3000,-10),(12,9,.2)).static_mesh_component.set_material(0,probe)
box('Function wall',(0,3400,90),(12,.2,1.8)).static_mesh_component.set_material(0,probe)
mpc=cdo.get_editor_property('parameters')
unreal.MaterialLibrary.set_scalar_parameter_value(world,mpc,'Wetness',1)
unreal.MaterialLibrary.set_scalar_parameter_value(world,mpc,'ExclusionCount',1)
# 왼쪽 좁은 구역만 실내로 간주: x=-400 +/-150, y=3000 +/-600, z=0 +/-400.
for axis,row in [('X',(1/150,0,0,400/150)),('Y',(0,1/600,0,-3000/600)),('Z',(0,0,1/400,0))]:
    unreal.MaterialLibrary.set_vector_parameter_value(world,mpc,'Exclude0'+axis,unreal.LinearColor(*row))
state={'start':time.monotonic(),'capture':False,'phase':0}
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
        names=['weather-preview.png','surface-wet.png','surface-snow.png']
        unreal.RenderingLibrary.export_render_target(world,target,str(OUT),names[state['phase']])
        if state['phase']<2:
            state['phase']+=1
            capture.set_actor_location(unreal.Vector(1000,1600,1150),False,False)
            capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(capture.get_actor_location(),unreal.Vector(0,3000,60)),False)
            unreal.MaterialLibrary.set_scalar_parameter_value(world,mpc,'Snow',1 if state['phase']==2 else 0)
            state['capture']=False;state['start']=time.monotonic()-30
            return
        (OUT/'verified-assets.json').write_text(json.dumps({'bindings':True,'niagara_ready':True,'ground_function':True,'niagara_count':7,'previews':names}))
        unreal.unregister_slate_post_tick_callback(handle); unreal.SystemLibrary.quit_editor()
    except Exception as exc:
        (OUT/'preview-error.txt').write_text(str(exc),encoding='utf8')
        unreal.unregister_slate_post_tick_callback(handle); unreal.SystemLibrary.quit_editor(); raise
handle=unreal.register_slate_post_tick_callback(tick)
