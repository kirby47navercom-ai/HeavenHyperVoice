"""UE 5.8 에디터에서 실행. C++ 날씨 연출용 BP/Niagara/머티리얼을 실제 에셋으로 저장한다.
기존 에셋은 유지한다. 수정은 콘텐츠 브라우저에서 하고, 재제작은 대상 에셋을 직접 지운 뒤 실행한다.
"""
import json
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import create_water_gun_vfx as water

ROOT = '/Game/VFX/Weather'
LIB = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
setp, scalar, vector = water.setp, water.scalar, water.vector


def save(asset):
    if not LIB.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Save failed: '+asset.get_path_name())


def particle_material(kind):
    path = ROOT+'/Materials/M_Weather_'+kind
    if LIB.does_asset_exist(path):
        return LIB.load_asset(path)
    previous = water.ROOT
    water.ROOT = ROOT
    try:
        mat = water.make_material('M_Weather_'+kind, 'drop')
    finally:
        water.ROOT = previous
    shapes = {
        'Rain': 'float mask=(1-smoothstep(.25,.8,abs(p.x)))*pow(saturate(1-p.y*p.y),.7)*.55; float3 col=float3(.56,.74,.87);',
        'Snow': 'float r=length(p); float a=atan2(p.y,p.x); float mask=(1-smoothstep(.28+.05*cos(a*6),.85,r))*.9; float3 col=float3(.87,.94,1);',
        'Splash': 'float r=length(p); float mask=(1-smoothstep(.25,.8,r))*.75; float3 col=float3(.64,.82,.94);',
        'Ripple': 'float r=length(p); float mask=(1-smoothstep(.015,.075,abs(r-lerp(.06,.92,Age))))*.4; float3 col=float3(.7,.87,.98);',
        'SnowPuff': 'float r=length(p); float mask=pow(saturate(1-r),2)*.55; float3 col=float3(.88,.95,1);',
    }
    custom = unreal.find_object(None, mat.get_path_name()+':MaterialExpressionCustom_0')
    custom.set_editor_property('code', 'float2 p=UV*2-1; '+shapes[kind]+
                               ' return float4(col,mask*saturate((1-Age)*5));')
    gain = unreal.find_object(None, mat.get_path_name()+':MaterialExpressionScalarParameter_0')
    gain.set_editor_property('default_value', .8)
    MEL.recompile_material(mat)
    save(mat)
    return mat


def particles(kind, mat):
    path = ROOT+'/NS_Weather_'+kind
    if LIB.does_asset_exist(path):
        return LIB.load_asset(path)
    system = LIB.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/FountainLightweight', path)
    falling = kind in ('Rain', 'Snow')
    specs = {'Rain': (1,(1,1),(1,1),0,(0,0),0),
             'Snow': (1,(4,4),(2,4),0,(0,0),0),
             'Splash': (7,(.15,.3),(1,3),1,(45,100),-450),
             'Ripple': (1,(.5,.5),(30,30),0,(0,0),0),
             'SnowPuff': (5,(.25,.5),(5,10),1,(8,24),-25)}
    count, life, size, radius, speed, gravity = specs[kind]
    water.configure_layer(system, kind, False, mat, True, count, life, size, radius, speed, 30, gravity)
    emitter = water.EDIT.water_layer(system, kind, False)
    mods = {m.get_class().get_name().replace('NiagaraStatelessModule_', ''): m
            for m in emitter.get_editor_property('modules')}
    renderer = unreal.find_object(None, emitter.get_path_name()+'.Renderer')
    init = mods['InitializeParticle']
    # Lightweight 이미터는 로컬 공간이다. 낙하 액터는 회전 없이 고정 생성한다.
    if falling:
        # 한 입자의 실제 속도/수명을 C++ 충돌 경로와 일치시킨다.
        setp(mods['ShapeLocation'], 'bModuleEnabled', 'False')
        setp(mods['AddVelocity'], 'bModuleEnabled', 'True')
        setp(mods['AddVelocity'], 'VelocityType', 'Linear')
        if not water.EDIT.bind_weather_particle(system, init, mods['AddVelocity']):
            raise RuntimeError('Fall parameter binding failed: '+kind)
        setp(init, 'SpriteRotationDistribution', scalar(0))
        if kind == 'Rain':
            setp(init, 'SpriteSizeDistribution', vector((1.3,24)))
            setp(renderer, 'Alignment', 'VelocityAligned')
        setp(emitter, 'FixedBounds', '(Min=(X=-3000,Y=-3000,Z=-4000),Max=(X=3000,Y=3000,Z=2000),IsValid=1)')
    elif kind == 'Ripple':
        facing = mods['SpriteFacingAndAlignment']
        setp(facing, 'bModuleEnabled', 'True')
        setp(facing, 'SpriteFacing', vector((0,0,1)))
        setp(renderer, 'FacingMode', 'CustomFacingVector')
    else:
        # 표면의 법선(+Z) 방향 반구로만 튄다.
        velocity = mods['AddVelocity']
        setp(velocity, 'VelocityType', 'InCone')
        setp(velocity, 'ConeDirection', vector((0,0,1)))
        setp(velocity, 'ConeRotationType', 'Direction')
        setp(velocity, 'ConeAngle', 65)
        setp(velocity, 'ConeVelocityDistribution', scalar(*speed))
    if not water.EDIT.finish_water_system(system):
        raise RuntimeError('Niagara compile failed: '+path)
    save(system)
    return system


def surface_material(rebuild=False):
    path = ROOT+'/Materials/M_Weather_Surface'
    if LIB.does_asset_exist(path):
        mat = LIB.load_asset(path)
        if not rebuild: return mat
        MEL.delete_all_material_expressions(mat)
    else:
        mat = TOOLS.create_asset('M_Weather_Surface', ROOT+'/Materials', unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    def node(cls, x, y, **props):
        n = MEL.create_material_expression(mat,cls,x,y)
        for k,v in props.items(): n.set_editor_property(k,v)
        return n
    uv = node(unreal.MaterialExpressionTextureCoordinate,-700,0)
    world = node(unreal.MaterialExpressionWorldPosition,-700,-160)
    wet = node(unreal.MaterialExpressionScalarParameter,-700,160,parameter_name='Wetness',default_value=0)
    snow = node(unreal.MaterialExpressionScalarParameter,-700,300,parameter_name='Snow',default_value=0)
    fx = node(unreal.MaterialExpressionCustom,-400,0,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for name in ('UV','World','Wetness','Snow'):
        i = unreal.CustomInput(); i.set_editor_property('input_name',name); inputs.append(i)
    fx.set_editor_property('inputs',inputs)
    fx.set_editor_property('code','''
float2 p=abs(UV*2-1);
float edge=1-smoothstep(.78,1,max(p.x,p.y));
float2 q=World.xy/95;
float2 cell=floor(q), f=frac(q); f=f*f*(3-2*f);
float a=frac(sin(dot(cell,float2(127.1,311.7)))*43758.5453);
float b=frac(sin(dot(cell+float2(1,0),float2(127.1,311.7)))*43758.5453);
float c=frac(sin(dot(cell+float2(0,1),float2(127.1,311.7)))*43758.5453);
float d=frac(sin(dot(cell+1,float2(127.1,311.7)))*43758.5453);
float noise=lerp(lerp(a,b,f.x),lerp(c,d,f.x),f.y);
float cover=saturate(Snow)*(.97+.03*noise);
float3 color=lerp(float3(.025,.04,.045),float3(.78,.87,.92),saturate(cover*3));
return float4(color,edge*max(saturate(Wetness)*.18*saturate(noise*2),cover*.95));''')
    for source,name in ((uv,'UV'),(world,'World'),(wet,'Wetness'),(snow,'Snow')):
        MEL.connect_material_expressions(source,'',fx,name)
    rgb=node(unreal.MaterialExpressionComponentMask,-100,0,r=True,g=True,b=True)
    a=node(unreal.MaterialExpressionComponentMask,-100,150,r=False,g=False,b=False,a=True)
    MEL.connect_material_expressions(fx,'',rgb,''); MEL.connect_material_expressions(fx,'',a,'')
    MEL.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(a,'',unreal.MaterialProperty.MP_OPACITY)
    rough=node(unreal.MaterialExpressionLinearInterpolate,-100,300,const_a=.18,const_b=.85)
    MEL.connect_material_expressions(snow,'',rough,'Alpha')
    MEL.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(mat); save(mat)
    return mat


def blueprint(name, parent):
    path=ROOT+'/'+name
    if LIB.does_asset_exist(path): return LIB.load_asset(path)
    factory=unreal.BlueprintFactory(); factory.set_editor_property('parent_class',parent)
    return TOOLS.create_asset(name,ROOT,unreal.Blueprint,factory)


def main():
    # 커맨드릿은 에디터와 달리 플러그인 콘텐츠 검색이 끝나기 전에 실행될 수 있다.
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
        ['/Niagara/DefaultAssets','/Game/VFX/Weather','/Game/Blueprints'], force_rescan=True)
    effects={k:particles(k,particle_material(k)) for k in ('Rain','Snow','Splash','Ripple','SnowPuff')}
    mpc=LIB.load_asset(ROOT+'/MPC_InstanceWeather') if LIB.does_asset_exist(ROOT+'/MPC_InstanceWeather') else None
    if not mpc:
        mpc=TOOLS.create_asset('MPC_InstanceWeather',ROOT,unreal.MaterialParameterCollection,unreal.MaterialParameterCollectionFactoryNew())
        params=[]
        for name in ('Wetness','Snow','Rain','Cloud','Fog'):
            p=unreal.CollectionScalarParameter(); p.set_editor_property('parameter_name',name)
            p.set_editor_property('default_value',0); params.append(p)
        mpc.set_editor_property('scalar_parameters',params)
        wind=unreal.CollectionVectorParameter(); wind.set_editor_property('parameter_name','Wind')
        wind.set_editor_property('default_value',unreal.LinearColor(0,0,0,0))
        mpc.set_editor_property('vector_parameters',[wind]); save(mpc)
    bp=blueprint('BP_InstanceWeatherDirector',unreal.UEInstanceWeatherDirector)
    cdo=unreal.get_default_object(bp.generated_class())
    for prop,kind in [('rain','Rain'),('snow','Snow'),('rain_impact','Splash'),('water_ripple','Ripple'),('snow_impact','SnowPuff')]:
        cdo.set_editor_property(prop,effects[kind])
    cdo.set_editor_property('surface_material',surface_material())
    cdo.set_editor_property('parameters',mpc)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp); save(bp)
    exclusion=blueprint('BP_WeatherExclusionVolume',unreal.UEWeatherExclusionVolume)
    unreal.BlueprintEditorLibrary.compile_blueprint(exclusion); save(exclusion)
    assets=LIB.load_asset('/Game/Blueprints/DA_ProjectAssets')
    if not assets: raise RuntimeError('DA_ProjectAssets missing')
    assets.set_editor_property('instance_weather_director_class',bp.generated_class()); save(assets)
    report={'effects':[s.get_path_name() for s in effects.values()], 'blueprint':bp.get_path_name(),
            'project_assets':assets.get_path_name(), 'ready':True}
    out=Path(unreal.Paths.project_saved_dir())/'Codex/Weather'; out.mkdir(parents=True,exist_ok=True)
    (out/'authored.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    unreal.log('INSTANCE WEATHER ASSETS READY')


if __name__=='__main__': main()
