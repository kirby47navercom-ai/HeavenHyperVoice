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


# 같은 조명 아래에서 기본 표면 / 얼음 / 서리를 비교해요.
floor=solid((.035,.06,.075))
mesh('Display base','/Engine/BasicShapes/Cube',(0,0,-100),(34,14,1),floor)
for x,name,override in [(-1000,'ClearIce',0),(0,'ClearIce',-1),(1000,'Frost',-1)]:
    material=LIB.load_asset(ROOT+'/Materials/MI_Environment_'+name)
    slab=mesh(name,'/Engine/BasicShapes/Cube',(x,0,-20),(8.8,10,.4),material)
    mid=slab.static_mesh_component.create_dynamic_material_instance(0)
    mid.set_scalar_parameter_value('Override',override)
    mid.set_scalar_parameter_value('CellSizeCm',210)
    mid.set_vector_parameter_value('BaseColor',unreal.LinearColor(.16,.21,.24))
    rock=mesh(name+' rock','/Game/Fab/WaterMaterials/Meshes/SM_River_Rock',(x,180,20),(.55,.55,.55),material)
    mid=rock.static_mesh_component.create_dynamic_material_instance(0)
    mid.set_scalar_parameter_value('Override',override)
    mid.set_scalar_parameter_value('CellSizeCm',90)

# 이 물은 미리보기용이에요. 실제 수위/파도는 기존 Water Body Ocean에서 계산해요.
water=unreal.new_object(unreal.Material)
position=ext.node(water,unreal.MaterialExpressionWorldPosition,-600,0)
clock=ext.node(water,unreal.MaterialExpressionTime,-600,180)
shader=ext.custom(water,'''
float depth=saturate((3200-P.y)/1800);
float3 col=lerp(float3(.09,.48,.49),float3(.012,.12,.22),depth);
float w=sin(P.x*.033+sin(P.y*.026+Time)*2+Time*.7);
float v=sin(P.y*.018-P.x*.024-Time*.9);
col+=float3(.05,.1,.085)*pow(saturate(w*v),6)*(1-depth);
return float4(col,.2);''',{'P':position,'Time':clock},0,0)
rgb=ext.node(water,unreal.MaterialExpressionComponentMask,300,0,r=True,g=True,b=True,a=False);ext.link(shader,rgb)
MEL.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
rough=ext.node(water,unreal.MaterialExpressionConstant,300,150,r=.22)
MEL.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
normal=ext.custom(water,'return float4(normalize(float3(cos(P.x*.024+Time)*.12,sin(P.y*.025-Time)*.16,1)),0);',{'P':position,'Time':clock},0,400)
n=ext.node(water,unreal.MaterialExpressionComponentMask,300,400,r=True,g=True,b=True,a=False);ext.link(normal,n)
MEL.connect_material_property(n,'',unreal.MaterialProperty.MP_NORMAL)
MEL.recompile_material(water)
mesh('Preview sea','/Engine/BasicShapes/Plane',(0,2200,0),(36,20,1),water)
mesh('Preview beach','/Engine/BasicShapes/Plane',(0,4000,-1),(36,16,1),LIB.load_asset(ROOT+'/Materials/M_Environment_Sand'))
for x,y,size in [(-1200,3280,.45),(-1100,3320,.22),(1100,3240,.5),(1300,3380,.24)]:
    mesh('Shore rock','/Game/Fab/WaterMaterials/Meshes/SM_River_Rock',(x,y,15),(size,size,size))
foam=LIB.load_asset(ROOT+'/NS_Environment_ShoreFoam')
assert unreal.UEWaterVFXEditorLibrary.finish_water_system(foam),'Niagara 준비 실패'
bp=LIB.load_asset(ROOT+'/BP_EnvironmentShoreFoam')
assert unreal.get_default_object(bp.generated_class()).get_component_by_class(unreal.NiagaraComponent).get_asset()==foam
actor=actors.spawn_actor_from_class(bp.generated_class(),unreal.Vector(0,3150,5))
actor.set_actor_scale3d(unreal.Vector(2.6,1,1))
fx=actor.get_component_by_class(unreal.NiagaraComponent)
assert fx.get_asset()==foam,'배치한 BP의 Niagara 에셋 누락'
fx.set_editor_property('allow_scalability',False);fx.set_force_solo(True);fx.activate(True)

sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector())
sun.set_actor_rotation(unreal.Rotator(pitch=-50,yaw=-25,roll=0),False)
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);sun.light_component.set_intensity(12)
sun.light_component.set_light_color(unreal.LinearColor(1,.9,.77))
fill=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector())
fill.set_actor_rotation(unreal.Rotator(pitch=-40,yaw=140,roll=0),False)
fill.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);fill.light_component.set_intensity(6)
fill.light_component.set_light_color(unreal.LinearColor(.54,.78,1))

capture=actors.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(0,2200,2200))
capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(capture.get_actor_location(),unreal.Vector(0,0,0)),False)
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
            unreal.RenderingLibrary.export_render_target(world,target,str(OUT),'ice-frost.png')
            capture.set_actor_location(unreal.Vector(1350,4720,1650),False,False)
            capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(capture.get_actor_location(),unreal.Vector(0,2900,0)),False)
            state.update(phase=1,start=time.monotonic()-36)
            return
        if elapsed-state['last']<.16:return
        filename='shore-foam.png' if state['frame']==0 else f'shore-{state["frame"]:02}.png'
        unreal.RenderingLibrary.export_render_target(world,target,str(OUT),filename)
        state['frame']+=1;state['last']=elapsed
        if state['frame']<18:return
        (OUT/'assets-preview.json').write_text(json.dumps({'niagara_ready':True,'active':fx.is_active(),'particles':count,'blueprint_bound':True,'images':['ice-frost.png','shore-foam.png'],'frames':18}),encoding='utf8')
        unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
    except Exception as exc:
        (OUT/'preview-error.txt').write_text(str(exc),encoding='utf8')
        unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor();raise


handle=unreal.register_slate_post_tick_callback(tick)
