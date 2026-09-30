"""환경 C++의 실제 DA/BP/Niagara와 예제 맵을 제작한다. 플레이는 실행하지 않는다.
기존 에셋은 덮어쓰지 않으며 Plain에는 환경 연결 액터 하나만 추가한다.
"""
from pathlib import Path
import json,sys
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_instance_weather as base
import extend_instance_weather as ext
from export_environment_profiles import export_profile

ROOT,LIB,MEL,TOOLS=base.ROOT,base.LIB,base.MEL,base.TOOLS
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def profiles():
    presets={
        'Temperate':{},
        'Coast':dict(tide_amplitude_m=1.5,base_wind_mps=5,thermal_response_seconds=10800,
                     daily_amplitude_c=3,seasonal_amplitude_c=5,wave_max_height_m=1.2),
        'Desert':dict(mean_temperature_c=32,initial_relative_humidity_pct=25,
                      initial_surface_water_kg_m2=0,initial_soil_water_kg_m2=.3,
                      sand_availability=1,base_wind_mps=8,gust_amplitude_mps=6,
                      soil_capacity_kg_m2=5,field_capacity_kg_m2=1,
                      infiltration_kg_m2_per_second=.004,drainage_seconds=7200,
                      thermal_response_seconds=900,daily_amplitude_c=14,latitude_degrees=25)}
    result={}
    for kind,values in presets.items():
        path=ROOT+'/DA_Environment_'+kind
        asset=LIB.load_asset(path) if LIB.does_asset_exist(path) else None
        if not asset:
            factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.UEEnvironmentProfile)
            asset=TOOLS.create_asset('DA_Environment_'+kind,ROOT,unreal.UEEnvironmentProfile,factory)
            for key,value in values.items():asset.set_editor_property(key,value)
            base.save(asset)
        export_profile(asset);result[kind]=asset
    return result


def dust(mpc):
    path=ROOT+'/NS_Environment_Dust'
    matpath=ROOT+'/Materials/M_Environment_Dust'
    mat=LIB.load_asset(matpath) if LIB.does_asset_exist(matpath) else TOOLS.create_asset('M_Environment_Dust',ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    uv=ext.node(mat,unreal.MaterialExpressionTextureCoordinate,-600,0)
    amount=ext.node(mat,unreal.MaterialExpressionCollectionParameter,-600,180,collection=mpc,parameter_name='Sandstorm')
    color=ext.node(mat,unreal.MaterialExpressionParticleColor,-800,360)
    # ParticleColor의 기본 출력은 RGB예요. 알파 핀을 직접 받아야 float3에서 A를 뽑는 컴파일 오류가 없어요.
    alpha=ext.node(mat,unreal.MaterialExpressionReroute,-600,360);ext.link(color,alpha,output='A')
    fx=ext.custom(mat,'float r=length(UV*2-1); return float4(.42,.26,.1,pow(saturate(1-r),2)*.2*Amount*Alpha);',dict(UV=uv,Amount=amount,Alpha=alpha))
    rgb=ext.node(mat,unreal.MaterialExpressionComponentMask,300,0,r=True,g=True,b=True)
    opacity=ext.node(mat,unreal.MaterialExpressionComponentMask,300,150,r=False,g=False,b=False,a=True)
    ext.link(fx,rgb);ext.link(fx,opacity)
    MEL.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat);base.save(mat)
    system=LIB.load_asset(path) if LIB.does_asset_exist(path) else LIB.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/FountainLightweight',path)
    base.water.configure_layer(system,'Dust',False,mat,False,75,(2,4),(35,100),900,(150,350),12)
    emitter=base.water.EDIT.water_layer(system,'Dust',False)
    mods={m.get_class().get_name().replace('NiagaraStatelessModule_',''):m for m in emitter.get_editor_property('modules')}
    base.setp(mods['AddVelocity'],'ConeDirection',base.vector((1,0,0)))
    base.setp(emitter,'FixedBounds','(Min=(X=-3000,Y=-1500,Z=-1500),Max=(X=3000,Y=1500,Z=1500),IsValid=1)')
    if not base.water.EDIT.finish_water_system(system):raise RuntimeError('Dust compile failed')
    base.save(system);return system


def spawn(cls,label,position=(0,0,0)):
    actor=actors.spawn_actor_from_class(cls,unreal.Vector(*position))
    actor.set_actor_label(label);return actor


def connect(bp,profile,ocean=None):
    present=list(actors.get_all_level_actors())
    scene=next((a for a in present if isinstance(a,unreal.UEEnvironmentScene)),None)
    if scene:
        dome=next((a for a in present if 'BP_Sky_Sphere' in a.get_class().get_name()),None)
        if dome:scene.set_editor_property('legacy_sky_dome',dome)
        return scene
    scene=spawn(bp.generated_class(),'EnvironmentScene')
    scene.set_editor_property('profile',profile)
    dome=next((a for a in present if 'BP_Sky_Sphere' in a.get_class().get_name()),None)
    if dome:scene.set_editor_property('legacy_sky_dome',dome)
    bindings=[('sun',unreal.DirectionalLight),('sky',unreal.SkyLight),
              ('fog',unreal.ExponentialHeightFog),('clouds',unreal.VolumetricCloud)]
    for key,cls in bindings:
        target=next((a for a in present if isinstance(a,cls)),None)
        if target:scene.set_editor_property(key,target)
    sun=scene.get_editor_property('sun')
    if sun:
        light=sun.get_component_by_class(unreal.DirectionalLightComponent)
        light.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.set_editor_property('atmosphere_sun_light',True)
    moon=spawn(unreal.DirectionalLight,'EnvironmentMoon')
    light=moon.get_component_by_class(unreal.DirectionalLightComponent)
    light.set_mobility(unreal.ComponentMobility.MOVABLE)
    light.set_editor_property('atmosphere_sun_light',True)
    light.set_editor_property('atmosphere_sun_light_index',1)
    light.set_intensity(0)
    scene.set_editor_property('moon',moon)
    sky=scene.get_editor_property('sky')
    if sky:
        light=sky.get_component_by_class(unreal.SkyLightComponent)
        light.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.set_editor_property('real_time_capture',True)
    cloud=scene.get_editor_property('clouds')
    if cloud:
        material=cloud.get_component_by_class(unreal.VolumetricCloudComponent).get_editor_property('material')
        scene.set_editor_property('cloud_density_parameter','Cloud_GlobalDensity')
        scene.set_editor_property('cloud_density_scale',MEL.get_material_instance_scalar_parameter_value(material,'Cloud_GlobalDensity'))
    if ocean:scene.set_editor_property('ocean',ocean)
    return scene


def sample(kind,bp,profile):
    path=ROOT+'/L_Environment_'+kind
    if LIB.does_asset_exist(path):
        levels.load_level(path);decorate_sample();levels.save_current_level();return path
    if not levels.new_level(path):raise RuntimeError('Cannot create '+path)
    sun=spawn(unreal.DirectionalLight,'Sun');sun.set_actor_rotation(unreal.Rotator(pitch=-35,yaw=-35,roll=0),False)
    spawn(unreal.SkyAtmosphere,'SkyAtmosphere');spawn(unreal.SkyLight,'SkyLight')
    spawn(unreal.ExponentialHeightFog,'Fog');spawn(unreal.VolumetricCloud,'Clouds')
    floor=spawn(unreal.StaticMeshActor,'Ground',(0,0,-100))
    floor.static_mesh_component.set_static_mesh(LIB.load_asset('/Engine/BasicShapes/Cube'))
    floor.set_actor_scale3d(unreal.Vector(80,80,2))
    floor.static_mesh_component.set_collision_profile_name('BlockAll')
    spawn(unreal.PlayerStart,'PlayerStart',(0,0,150))
    ocean=None
    if kind=='Coast':
        ocean=spawn(unreal.WaterBodyOcean,'Ocean',(0,0,-30))
        # Water 플러그인의 기본 바다와 구역을 사용한다. 도시 원본 메시에 손대지 않는다.
        ocean.get_water_body_component().set_editor_property('affects_landscape',False)
    connect(bp,profile,ocean)
    decorate_sample()
    levels.save_current_level();return path


def decorate_sample():
    """눈/젖음 공용 함수가 적용된 모래 지면. 수면은 실제 Water 플러그인 머티리얼이다."""
    path=ROOT+'/Materials/M_Environment_Sand'
    mat=LIB.load_asset(path) if LIB.does_asset_exist(path) else None
    if not mat:
        mat=TOOLS.create_asset('M_Environment_Sand',ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
        color=ext.node(mat,unreal.MaterialExpressionConstant3Vector,-500,0,constant=unreal.LinearColor(.42,.28,.12))
        rough=ext.node(mat,unreal.MaterialExpressionConstant,-500,100,r=.85)
        normal=ext.node(mat,unreal.MaterialExpressionVertexNormalWS,-500,200)
        function=ext.node(mat,unreal.MaterialExpressionMaterialFunctionCall,0,0)
        function.set_material_function(LIB.load_asset(ROOT+'/Materials/MF_WeatherSurface'))
        ext.link(color,function,'BaseColor');ext.link(rough,function,'Roughness');ext.link(normal,function,'WorldNormal')
        MEL.connect_material_property(function,'BaseColor',unreal.MaterialProperty.MP_BASE_COLOR)
        MEL.connect_material_property(function,'Roughness',unreal.MaterialProperty.MP_ROUGHNESS)
        MEL.recompile_material(mat);base.save(mat)
    for actor in actors.get_all_level_actors():
        if actor.get_actor_label()=='Ground':actor.static_mesh_component.set_material(0,mat)
        if isinstance(actor,unreal.WaterBodyOcean):
            spline=actor.get_water_spline()
            spline.set_spline_points([unreal.Vector(x,y,0) for x,y in [(-3300,-3300),(-3300,3300),(3300,3300),(3300,-3300)]],unreal.SplineCoordinateSpace.LOCAL,True)
            for i in range(4):spline.set_spline_point_type(i,unreal.SplinePointType.LINEAR,True)
            spline.set_closed_loop(True,True)


def main():
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Niagara/DefaultAssets',ROOT],force_rescan=True)
    values=profiles()
    mpc=LIB.load_asset(ROOT+'/MPC_InstanceWeather')
    params=list(mpc.get_editor_property('scalar_parameters'));names={str(p.get_editor_property('parameter_name')) for p in params}
    for name in ('Sandstorm','Season','DayFraction'):
        if name not in names:
            p=unreal.CollectionScalarParameter();p.set_editor_property('parameter_name',name);params.append(p)
    mpc.set_editor_property('scalar_parameters',params);base.save(mpc)
    bp=base.blueprint('BP_EnvironmentScene',unreal.UEEnvironmentScene)
    cdo=unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('parameters',mpc);cdo.set_editor_property('dust_system',dust(mpc))
    cdo.set_editor_property('profile',values['Temperate'])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);base.save(bp)
    levels.load_level('/Game/InstanceMap/Plain/Plain')
    connect(bp,values['Temperate']);levels.save_current_level()
    maps=[sample(kind,bp,values[kind]) for kind in ('Coast','Desert')]
    report={'profiles':[a.get_path_name() for a in values.values()],'scene':bp.get_path_name(),'maps':maps}
    out=Path(unreal.Paths.project_saved_dir())/'environment-authored.json'
    out.write_text(json.dumps(report,indent=2),encoding='utf8')


if __name__=='__main__':main()
