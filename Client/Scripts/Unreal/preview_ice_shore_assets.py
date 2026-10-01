"""실제 에셋의 에디터 렌더 미리보기. PIE/서버/게임 맵은 실행하지 않아요.
저장하지 않는 임시 레벨과 미리보기용 물 표면을 사용해요.
"""
import json
import sys
import time
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_ice_shore_assets as authored
import extend_instance_weather as ext

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ROOT=authored.ROOT
OUT=Path(unreal.Paths.project_saved_dir())/'Codex/EarthScienceAssets'
OUT.mkdir(parents=True,exist_ok=True)
LIB,MEL=authored.LIB,authored.MEL
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.new_level('/Temp/IceShorePreview_'+str(time.time_ns()))
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mpc=LIB.load_asset(ROOT+'/MPC_InstanceWeather')


def scalar(name,value):
    unreal.MaterialLibrary.set_scalar_parameter_value(world,mpc,name,value)


def mesh(label,path,pos,scale,material=None):
    a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos))
    a.set_actor_label(label);a.static_mesh_component.set_static_mesh(LIB.load_asset(path))
    a.set_actor_scale3d(unreal.Vector(*scale))
    if material:a.static_mesh_component.set_material(0,material)
    return a


def solid(color,rough=.7):
    mat=unreal.new_object(unreal.Material)
    c=ext.node(mat,unreal.MaterialExpressionConstant3Vector,0,0,constant=unreal.LinearColor(*color))
    r=ext.node(mat,unreal.MaterialExpressionConstant,0,160,r=rough)
    MEL.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(mat)
    return mat


# 실제 재질을 바위/얼어붙은 수면/경사진 모래 위에서 확인해요.
# 모든 배치는 저장하지 않는 에디터 임시 레벨에만 존재해요.
ice=LIB.load_asset(ROOT+'/Materials/MI_Environment_ClearIce')
frost=LIB.load_asset(ROOT+'/Materials/MI_Environment_Frost')
mesh('Frozen lake','/Engine/BasicShapes/Plane',(0,-4300,0),(80,80,1),ice)
for i,(x,y,z,scale) in enumerate([(-1050,-3800,-10,3.4),(-800,-4160,-20,2.1),(800,-4850,-15,2.5),(1100,-4750,-5,1.6)]):
    stone=mesh('Hoarfrost rock','/Game/Fab/WaterMaterials/Meshes/SM_River_Rock',(x,y,z),(scale,scale,scale),frost)
    stone.set_actor_rotation(unreal.Rotator(0,i*53,0),False)
    mid=stone.static_mesh_component.create_dynamic_material_instance(0)
    mid.set_scalar_parameter_value('CellSizeCm',450)
    mid.set_vector_parameter_value('BaseColor',unreal.LinearColor(.18,.16,.14))

# 프로젝트에 있는 실제 Shore 메시와 물 재질을 재사용해요.
water=LIB.load_asset('/Game/Fab/WaterMaterials/Materials/M_Ocean')
sea=mesh('Preview sea','/Game/Fab/WaterMaterials/Meshes/SM_Water_Plane',(0,1700,0),(5,3.5,1),water)
mid=sea.static_mesh_component.create_dynamic_material_instance(0)
for name,value in [('Opacity',.82),('OpacityDeep',.97),('Master_Intensity',.25),('Master_Speed',.6),('Refraction',.01),('Emissive',0),('FakeSpec_Intensity',.3),('Ocean_Depth',.5),('Ocean_DepthScale',1),('CubeMap_Intensity',.7),('OceanShore_Intensity',.2)]:mid.set_scalar_parameter_value(name,value)
mid.set_vector_parameter_value('ColourDeep',unreal.LinearColor(.007,.045,.055))
mid.set_vector_parameter_value('Colour',unreal.LinearColor(.05,.2,.22))
shore=mesh('Sloping shore','/Game/Fab/WaterMaterials/Meshes/SM_Shore',(0,3150,0),(5,1.3,2),LIB.load_asset('/Game/Fab/WaterMaterials/Materials/M_Sand'))
mid=shore.static_mesh_component.create_dynamic_material_instance(0)
mid.set_scalar_parameter_value('NormalIntensity',.5);mid.set_scalar_parameter_value('Tiling',3)
for i,(x,y,scale) in enumerate([(-1250,3270,2.4),(-990,3380,1.2),(-840,3260,.65),(1050,3510,1.7),(1240,3480,.9)]):
    stone=mesh('Shore rock','/Game/Fab/WaterMaterials/Meshes/SM_River_Rock',(x,y,-5),(scale,scale,scale))
    stone.set_actor_rotation(unreal.Rotator(0,i*67,0),False)
foam=LIB.load_asset(ROOT+'/NS_Environment_ShoreFoam')
assert unreal.UEWaterVFXEditorLibrary.finish_water_system(foam),'Niagara 준비 실패'
bp=LIB.load_asset(ROOT+'/BP_EnvironmentShoreFoam')
assert unreal.get_default_object(bp.generated_class()).get_component_by_class(unreal.NiagaraComponent).get_asset()==foam
actor=actors.spawn_actor_from_class(bp.generated_class(),unreal.Vector(0,3230,6))
actor.set_actor_scale3d(unreal.Vector(2.6,1,1))
fx=actor.get_component_by_class(unreal.NiagaraComponent)
assert fx.get_asset()==foam,'배치한 BP의 Niagara 에셋 누락'
fx.set_editor_property('allow_scalability',False);fx.set_force_solo(True);fx.activate(True)

sky=actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector())
sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky.light_component.set_editor_property('cubemap',LIB.load_asset('/Game/Fab/WaterMaterials/Textures/T_Cubemap'))
sky.light_component.set_intensity(1.8)
sky.light_component.recapture_sky()

sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector())
sun.set_actor_rotation(unreal.Rotator(pitch=-50,yaw=-25,roll=0),False)
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);sun.light_component.set_intensity(7)
sun.light_component.set_light_color(unreal.LinearColor(1,.9,.77))
fill=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector())
fill.set_actor_rotation(unreal.Rotator(pitch=-40,yaw=140,roll=0),False)
fill.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);fill.light_component.set_intensity(4)
fill.light_component.set_light_color(unreal.LinearColor(.54,.78,1))

capture=actors.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(350,-3500,850))
capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(capture.get_actor_location(),unreal.Vector(0,-4700,0)),False)
component=capture.get_component_by_class(unreal.SceneCaptureComponent2D)
target=unreal.RenderingLibrary.create_render_target2d(world,1440,900,unreal.TextureRenderTargetFormat.RTF_RGBA8)
component.texture_target=target;component.capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
component.capture_every_frame=False;component.capture_on_movement=False;component.fov_angle=55
component.show_flag_settings=[unreal.EngineShowFlagsSetting(show_flag_name=name,enabled=True) for name in ('Particles','Niagara')]
settings=component.post_process_settings
for name,value in [('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL),('auto_exposure_apply_physical_camera_exposure',False),('auto_exposure_bias',0),('bloom_intensity',.12)]:
    settings.set_editor_property('override_'+name,True);settings.set_editor_property(name,value)
component.post_process_settings=settings
for name,value in [('IceMm',5),('GroundTemperatureC',-8),('Wetness',.65),('Snow',0),('WaveHeightM',.6),('TideLevelM',0),('ExclusionCount',0)]:scalar(name,value)

state={'start':time.monotonic(),'phase':0,'frame':0,'last':0,'activated':False}


def tick(delta):
    try:
        elapsed=time.monotonic()-state['start']
        if elapsed>5 and not state['activated']:
            # 에디터 초기 PSO 준비 뒤 활성화해요. 첫 프레임의 대기 상태를 계속 두지 않아요.
            fx.reinitialize_system();fx.activate(True);state['activated']=True
        if not fx.is_active():fx.activate(False)
        count=unreal.UEWaterVFXEditorLibrary.tick_water_preview(fx,delta)
        assert count>=0
        component.capture_scene()
        if elapsed<40:return
        assert count>0,'해안 거품이 컴파일만 되고 실제 입자는 생성되지 않음'
        if state['phase']==0:
            unreal.RenderingLibrary.export_render_target(world,target,str(OUT),'realistic-ice-frost.png')
            capture.set_actor_location(unreal.Vector(-650,-3400,450),False,False)
            capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(capture.get_actor_location(),unreal.Vector(-1020,-3800,120)),False)
            state.update(phase=1,start=time.monotonic()-36)
            return
        if state['phase']==1:
            unreal.RenderingLibrary.export_render_target(world,target,str(OUT),'realistic-frost.png')
            capture.set_actor_location(unreal.Vector(450,4300,750),False,False)
            capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(capture.get_actor_location(),unreal.Vector(0,3000,0)),False)
            state.update(phase=2,start=time.monotonic()-36)
            return
        if elapsed-state['last']<.18:return
        filename='realistic-shore-foam.png' if state['frame']==0 else f'realistic-shore-{state["frame"]:02}.png'
        unreal.RenderingLibrary.export_render_target(world,target,str(OUT),filename)
        state['frame']+=1;state['last']=elapsed
        if state['frame']<32:return
        (OUT/'assets-preview.json').write_text(json.dumps({'niagara_ready':True,'active':fx.is_active(),'particles':count,'blueprint_bound':True,'images':['realistic-ice-frost.png','realistic-frost.png','realistic-shore-foam.png'],'frames':32}),encoding='utf8')
        unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
    except Exception as exc:
        (OUT/'preview-error.txt').write_text(str(exc),encoding='utf8')
        unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor();raise


handle=unreal.register_slate_post_tick_callback(tick)
