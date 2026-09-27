"""날씨 공용 지면 함수와 피격 세부 에셋 제작. 기존 원본 색/노멀을 유지해 실제 Plain 지면에 연결한다.
에디터 제작 스크립트이며 서버 또는 플레이를 실행하지 않는다.
"""
import json
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_instance_weather as base

ROOT=base.ROOT
LIB=base.LIB
MEL=base.MEL
TOOLS=base.TOOLS
save=base.save
setp=base.setp


def node(owner,cls,x=0,y=0,**props):
    n=(MEL.create_material_expression_in_function(owner,cls,x,y) if isinstance(owner,unreal.MaterialFunction)
       else MEL.create_material_expression(owner,cls,x,y))
    for k,v in props.items(): n.set_editor_property(k,v)
    return n


def link(source,target,pin='',output=''):
    assert MEL.connect_material_expressions(source,output,target,pin), (source,target,pin,output)


def custom(owner,code,bindings,x=0,y=0,scalar=False):
    n=node(owner,unreal.MaterialExpressionCustom,x,y,
           output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1 if scalar else unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs=[]
    for name in bindings:
        entry=unreal.CustomInput();entry.set_editor_property('input_name',name);inputs.append(entry)
    n.set_editor_property('inputs',inputs);n.set_editor_property('code',code)
    for name,source in bindings.items(): link(source,n,name)
    return n


def add_collection_bounds(mpc):
    scalar=list(mpc.get_editor_property('scalar_parameters'))
    if not any(str(p.get_editor_property('parameter_name'))=='ExclusionCount' for p in scalar):
        p=unreal.CollectionScalarParameter();p.set_editor_property('parameter_name','ExclusionCount');p.set_editor_property('default_value',0);scalar.append(p)
        mpc.set_editor_property('scalar_parameters',scalar)
    vectors=list(mpc.get_editor_property('vector_parameters'))
    names={str(p.get_editor_property('parameter_name')) for p in vectors}
    for i in range(8):
        for axis in 'XYZ':
            name=f'Exclude{i}{axis}'
            if name in names:continue
            p=unreal.CollectionVectorParameter();p.set_editor_property('parameter_name',name);p.set_editor_property('default_value',unreal.LinearColor(0,0,0,0))
            vectors.append(p)
    mpc.set_editor_property('vector_parameters',vectors);save(mpc)


def material_function(mpc):
    path=ROOT+'/Materials/MF_WeatherSurface'
    if LIB.does_asset_exist(path):return LIB.load_asset(path)
    fn=TOOLS.create_asset('MF_WeatherSurface',ROOT+'/Materials',unreal.MaterialFunction,unreal.MaterialFunctionFactoryNew())
    fn.set_editor_property('description','서버 날씨 + 위쪽 표면 + 웅덩이 마스크 + 실내 차단. 기존 색과 거칠기를 입력한다.')
    fn.set_editor_property('expose_to_library',True)
    inputs={}
    for i,(name,kind,preview) in enumerate([
            ('BaseColor',unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3,(.3,.3,.3,0)),
            ('Roughness',unreal.FunctionInputType.FUNCTION_INPUT_SCALAR,(.7,0,0,0)),
            ('WorldNormal',unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3,(0,0,1,0)),
            ('Exposure',unreal.FunctionInputType.FUNCTION_INPUT_SCALAR,(1,0,0,0)),
            ('PuddleMask',unreal.FunctionInputType.FUNCTION_INPUT_SCALAR,(0,0,0,0))]):
        inp=node(fn,unreal.MaterialExpressionFunctionInput,-1000,i*160,input_name=name,input_type=kind,
                 sort_priority=i,use_preview_value_as_default=True)
        setp(inp,'PreviewValue','(X=%s,Y=%s,Z=%s,W=%s)'%preview);inputs[name]=inp
    for i,name in enumerate(('Wetness','Snow','ExclusionCount')):
        inputs[name]=node(fn,unreal.MaterialExpressionCollectionParameter,-1500,-160-i*160,collection=mpc,parameter_name=name)
    inputs['World']=node(fn,unreal.MaterialExpressionWorldPosition,-1000,-160)
    code='float exposed=saturate(Exposure); float4 wp=float4(World,1);\n'
    for i in range(8):
        for j,axis in enumerate('XYZ'):
            key=f'E{i}{axis}'
            inputs[key]=node(fn,unreal.MaterialExpressionCollectionParameter,-2100-j*320,i*140,
                            collection=mpc,parameter_name=f'Exclude{i}{axis}')
        code+=f'if (ExclusionCount>{i}.5) {{ float3 p=float3(dot(wp,E{i}X),dot(wp,E{i}Y),dot(wp,E{i}Z)); exposed*=step(1.0001,max(abs(p.x),max(abs(p.y),abs(p.z)))); }}\n'
    code+='''
float up=smoothstep(.45,.9,normalize(WorldNormal).z);
float flat=smoothstep(.94,.995,normalize(WorldNormal).z);
float wet=saturate(Wetness)*exposed;
float snow=saturate(Snow)*exposed*up;
float puddle=saturate(PuddleMask)*flat*wet*(1-snow);
float3 color=lerp(BaseColor,BaseColor*.66,wet);
color=lerp(color,BaseColor*.42,puddle);
color=lerp(color,float3(.8,.87,.92),snow);
float rough=lerp(Roughness,min(Roughness,.3),wet);
rough=lerp(rough,.06,puddle);
return float4(color,lerp(rough,.85,snow));'''
    fx=custom(fn,code,inputs,0,0)
    for i,(name,rgb) in enumerate([('BaseColor',True),('Roughness',False)]):
        mask=node(fn,unreal.MaterialExpressionComponentMask,300,i*180,r=rgb,g=rgb,b=rgb,a=not rgb)
        link(fx,mask)
        out=node(fn,unreal.MaterialExpressionFunctionOutput,550,i*180,output_name=name,sort_priority=i)
        link(mask,out)
    MEL.update_material_function(fn);save(fn)
    return fn


def connect_ground(fn):
    # 실제 현재 인스턴스 Landscape가 사용하는 머티리얼. 노멀/레이어/충돌/맵은 바꾸지 않는다.
    mat=LIB.load_asset('/Game/InstanceMap/Plain/Landscape/M_Landscape_GrassSoil')
    for n in MEL.get_material_expressions(mat):
        if isinstance(n,unreal.MaterialExpressionMaterialFunctionCall) and n.get_editor_property('material_function')==fn:
            return mat
    assert not mat.get_editor_property('use_material_attributes')
    color=MEL.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR)
    rough=MEL.get_material_property_input_node(mat,unreal.MaterialProperty.MP_ROUGHNESS)
    color_output=MEL.get_material_property_input_node_output_name(mat,unreal.MaterialProperty.MP_BASE_COLOR)
    rough_output=MEL.get_material_property_input_node_output_name(mat,unreal.MaterialProperty.MP_ROUGHNESS)
    assert color and rough,'기존 지면 색/거칠기 입력을 보존할 수 없음'
    call=node(mat,unreal.MaterialExpressionMaterialFunctionCall,1400,0,material_function=fn)
    link(color,call,'BaseColor',color_output);link(rough,call,'Roughness',rough_output)
    normal=node(mat,unreal.MaterialExpressionVertexNormalWS,900,500);link(normal,call,'WorldNormal')
    exposure=node(mat,unreal.MaterialExpressionScalarParameter,700,300,parameter_name='WeatherExposure',default_value=1,group='Weather')
    marker=node(mat,unreal.MaterialExpressionScalarParameter,700,440,parameter_name='WeatherMaterialEnabled',default_value=1,group='Weather')
    enabled=node(mat,unreal.MaterialExpressionMultiply,1050,300)
    link(exposure,enabled,'A');link(marker,enabled,'B');link(enabled,call,'Exposure')
    # 기존 높이 텍스처의 낮은 곳을 웅덩이 허용 마스크로 사용한다. MI에서 교체/페인트 가능.
    texture=LIB.load_asset('/Game/InstanceMap/Plain/Landscape/rocky_trail_02_disp_4k')
    sample=node(mat,unreal.MaterialExpressionTextureSampleParameter2D,650,950,
                parameter_name='WeatherPuddleMaskTexture',texture=texture,group='Weather')
    world=node(mat,unreal.MaterialExpressionWorldPosition,50,800)
    scale=node(mat,unreal.MaterialExpressionScalarParameter,50,970,parameter_name='WeatherPuddleWorldSize',default_value=600,group='Weather')
    uv=custom(mat,'return World.xy/max(Size,1);',{'World':world,'Size':scale},320,820)
    # UV 함수는 2D 출력을 명시한다.
    uv.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT2);link(uv,sample,'UVs')
    channel=node(mat,unreal.MaterialExpressionComponentMask,900,950,r=True,g=False,b=False,a=False);link(sample,channel,output='RGB')
    vertex=node(mat,unreal.MaterialExpressionVertexColor,650,1200)
    blue=node(mat,unreal.MaterialExpressionComponentMask,900,1200,r=False,g=False,b=True,a=False);link(vertex,blue)
    use_vertex=node(mat,unreal.MaterialExpressionScalarParameter,650,1400,parameter_name='WeatherUseVertexPuddleMask',default_value=0,group='Weather')
    level=node(mat,unreal.MaterialExpressionScalarParameter,650,1560,parameter_name='WeatherPuddleLevel',default_value=.35,group='Weather')
    mask=custom(mat,'return lerp(saturate((Level-Height)*8),saturate(Vertex),saturate(UseVertex));',
                {'Height':channel,'Vertex':blue,'UseVertex':use_vertex,'Level':level},1200,950,scalar=True)
    link(mask,call,'PuddleMask')
    assert MEL.connect_material_property(call,'BaseColor',unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(call,'Roughness',unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(mat);save(mat)
    return mat


def wet_mark():
    path=ROOT+'/Materials/M_Weather_WetImpact'
    if LIB.does_asset_exist(path):return LIB.load_asset(path)
    mat=TOOLS.create_asset('M_Weather_WetImpact',ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    uv=node(mat,unreal.MaterialExpressionTextureCoordinate,-400,0)
    fade=node(mat,unreal.MaterialExpressionDecalLifetimeOpacity,-400,180)
    alpha=custom(mat,'float2 p=UV*2-1; return (1-smoothstep(.35,.95,length(p)))*Fade*.35;',{'UV':uv,'Fade':fade},-100,0,scalar=True)
    color=node(mat,unreal.MaterialExpressionConstant3Vector,-100,-180,constant=unreal.LinearColor(.018,.028,.035,1))
    rough=node(mat,unreal.MaterialExpressionConstant,-100,180,r=.14)
    for n,p in [(alpha,unreal.MaterialProperty.MP_OPACITY),(color,unreal.MaterialProperty.MP_BASE_COLOR),(rough,unreal.MaterialProperty.MP_ROUGHNESS)]:
        assert MEL.connect_material_property(n,'',p)
    MEL.recompile_material(mat);save(mat);return mat


def impact(kind):
    path=ROOT+'/NS_Weather_'+kind
    if LIB.does_asset_exist(path):return LIB.load_asset(path)
    system=LIB.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/FountainLightweight',path)
    sprite=LIB.load_asset(ROOT+'/Materials/M_Weather_Splash')
    if kind=='WallSplash':
        base.water.configure_layer(system,kind,False,sprite,True,3,(.12,.22),(.7,1.5),.5,(15,35),30,0,.55)
    else:
        base.water.configure_layer(system,kind,False,sprite,True,3,(.2,.35),(1,1),1,(30,60),30,-300,1)
    emitter=base.water.EDIT.water_layer(system,kind,False)
    mods={m.get_class().get_name().replace('NiagaraStatelessModule_',''):m for m in emitter.get_editor_property('modules')}
    vel=mods['AddVelocity'];setp(vel,'VelocityType','InCone');setp(vel,'ConeRotationType','Direction')
    setp(vel,'ConeDirection',base.vector((0,0,1)));setp(vel,'ConeAngle',50)
    setp(vel,'ConeVelocityDistribution',base.scalar(*( (15,35) if kind=='WallSplash' else (30,60))))
    if kind=='SnowChunks':
        mat=LIB.load_asset(ROOT+'/Materials/M_Weather_SnowChunk') if LIB.does_asset_exist(ROOT+'/Materials/M_Weather_SnowChunk') else None
        if not mat:
            mat=TOOLS.create_asset('M_Weather_SnowChunk',ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
            color=node(mat,unreal.MaterialExpressionConstant3Vector,0,0,constant=unreal.LinearColor(.8,.88,.95,1))
            rough=node(mat,unreal.MaterialExpressionConstant,0,180,r=.85)
            MEL.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
            MEL.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
            MEL.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES);MEL.recompile_material(mat);save(mat)
        renderer=unreal.new_object(unreal.NiagaraMeshRendererProperties,outer=emitter)
        mesh=unreal.NiagaraMeshRendererMeshProperties();mesh.set_editor_property('mesh',unreal.load_asset('/Engine/BasicShapes/Sphere'))
        renderer.set_editor_property('meshes',[mesh]);renderer.set_editor_property('override_materials',[])
        override=unreal.NiagaraMeshMaterialOverride();override.set_editor_property('explicit_mat',mat)
        renderer.set_editor_property('override_materials',[override])
        setp(renderer,'bOverrideMaterials','True')
        setp(emitter,'RendererProperties',f'("{renderer.get_path_name()}")')
        setp(mods['InitializeParticle'],'MeshScaleDistribution',base.vector((.025,.035,.02)))
    assert base.water.EDIT.finish_water_system(system),kind
    save(system);return system


def main():
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
        ['/Niagara/DefaultAssets','/Game/VFX/Weather','/Game/InstanceMap/Plain'],force_rescan=True)
    mpc=LIB.load_asset(ROOT+'/MPC_InstanceWeather');add_collection_bounds(mpc)
    fn=material_function(mpc);ground=connect_ground(fn)
    bp=LIB.load_asset(ROOT+'/BP_InstanceWeatherDirector');cdo=unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('wall_splash',impact('WallSplash'))
    cdo.set_editor_property('snow_chunks',impact('SnowChunks'))
    cdo.set_editor_property('wet_impact_material',wet_mark())
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    report={'function':fn.get_path_name(),'connected_ground':ground.get_path_name(),
            'future_indoor_material_boxes':8,'existing_instance_water_surfaces':0}
    out=Path(unreal.Paths.project_saved_dir())/'Codex/Weather'
    (out/'extended.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    unreal.log('WEATHER DETAILS CONNECTED')


if __name__=='__main__':main()
