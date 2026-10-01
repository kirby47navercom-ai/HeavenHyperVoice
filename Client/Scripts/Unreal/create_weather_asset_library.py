"""독립 날씨 에셋 라이브러리 제작. 게임 맵/서버/BP 연결은 변경하지 않아요.

기존 제작 도구를 재사용해 편집 가능한 실제 uasset을 저장해요.
PC 경로는 프로젝트 위치에서 구하고 셰이더 코드는 에셋 내부에 저장해요.
재실행은 작가의 편집을 보존하며 --refresh일 때만 이 라이브러리를 갱신해요.
"""
import json
import sys
from pathlib import Path
import unreal

sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_instance_weather as base
import extend_instance_weather as ext
import create_ice_shore_assets as ice

ROOT=base.ROOT
SURFACES=ROOT+'/Materials/Surfaces'
ATMOSPHERE=ROOT+'/Materials/Atmosphere'
SYSTEMS=ROOT+'/Niagara/Atmosphere'
BLUEPRINTS=ROOT+'/Blueprints/Atmosphere'
LIB,MEL,TOOLS=base.LIB,base.MEL,base.TOOLS
node,link,save,setp=ext.node,ext.link,base.save,base.setp
REFRESH='--refresh' in sys.argv
SHADERS=Path(__file__).with_name('Shaders')


def asset(name,folder,cls,factory):
    path=folder+'/'+name
    return LIB.load_asset(path) if LIB.does_asset_exist(path) else TOOLS.create_asset(name,folder,cls,factory)


def scalar(owner,name,value,x,y,group='Surface'):
    return node(owner,unreal.MaterialExpressionScalarParameter,x,y,parameter_name=name,default_value=value,group=group)


def texture(owner,name,path,x,y,normal=False):
    tex=LIB.load_asset(path);assert tex,path
    sampler=(unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else
             unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if tex.get_editor_property('srgb') else
             unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    return node(owner,unreal.MaterialExpressionTextureSampleParameter2D,x,y,parameter_name=name,
                texture=tex,sampler_type=sampler,group='Textures')


def split(owner,source,x,y,rgb=True):
    n=node(owner,unreal.MaterialExpressionComponentMask,x,y,r=rgb,g=rgb,b=rgb,a=not rgb)
    link(source,n);return n


def wetness_function():
    path=SURFACES+'/MF_SurfaceWetness'
    if LIB.does_asset_exist(path) and not REFRESH:return LIB.load_asset(path)
    fn=asset('MF_SurfaceWetness',SURFACES,unreal.MaterialFunction,unreal.MaterialFunctionFactoryNew())
    # 이미 연결한 함수 핀의 ID를 유지하고 계산 노드만 다시 만들어요.
    expressions=MEL.get_material_function_expressions(fn)
    old_inputs={str(n.get_editor_property('input_name')):n for n in expressions if isinstance(n,unreal.MaterialExpressionFunctionInput)}
    old_outputs={str(n.get_editor_property('output_name')):n for n in expressions if isinstance(n,unreal.MaterialExpressionFunctionOutput)}
    for n in expressions:
        if not isinstance(n,(unreal.MaterialExpressionFunctionInput,unreal.MaterialExpressionFunctionOutput)):
            MEL.delete_material_expression_in_function(fn,n)
    fn.set_editor_property('description','기존 색/거칠기 + 수동 젖음/높이/마스크. MPC나 PC 경로에 의존하지 않아요.')
    fn.set_editor_property('expose_to_library',True)
    specs=[('BaseColor',(.3,.25,.2,0),True),('Roughness',(.75,0,0,0),False),
           ('WorldNormal',(0,0,1,0),True),('Height',(.5,0,0,0),False),
           ('Mask',(1,0,0,0),False),('Exposure',(1,0,0,0),False),
           ('Wetness',(0,0,0,0),False),('PuddleStrength',(0,0,0,0),False),
           ('PuddleLevel',(.4,0,0,0),False),('WetDarkening',(.58,0,0,0),False),
           ('WetRoughness',(.3,0,0,0),False),('PuddleRoughness',(.08,0,0,0),False)]
    inputs={}
    for i,(name,preview,vector) in enumerate(specs):
        n=old_inputs.get(name) or node(fn,unreal.MaterialExpressionFunctionInput,-800,i*145,
              input_name=name,input_type=unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3 if vector else unreal.FunctionInputType.FUNCTION_INPUT_SCALAR,
              sort_priority=i,use_preview_value_as_default=True)
        setp(n,'PreviewValue','(X=%s,Y=%s,Z=%s,W=%s)'%preview);inputs[name]=n
    result=ext.custom(fn,(SHADERS/'EnvironmentSurfaceWetness.ush').read_text(encoding='utf8'),inputs,0,0)
    puddles=ext.custom(fn,'return (1-smoothstep(Level-.06,Level+.06,Height))*saturate(Strength)*saturate(Wetness)*saturate(Exposure)*saturate(Mask)*smoothstep(.6,.95,normalize(N).z);',
                {'Level':inputs['PuddleLevel'],'Height':inputs['Height'],'Strength':inputs['PuddleStrength'],'Wetness':inputs['Wetness'],'Exposure':inputs['Exposure'],'Mask':inputs['Mask'],'N':inputs['WorldNormal']},0,420,scalar=True)
    for i,(name,source) in enumerate([('BaseColor',split(fn,result,300,0)),('Roughness',split(fn,result,300,180,False)),('PuddleMask',puddles)]):
        output=old_outputs.get(name) or node(fn,unreal.MaterialExpressionFunctionOutput,650,i*180,output_name=name,sort_priority=i)
        link(source,output)
    MEL.update_material_function(fn);save(fn);return fn


def surface_material(fn):
    path=SURFACES+'/M_EnvironmentSurface'
    if LIB.does_asset_exist(path) and not REFRESH:return LIB.load_asset(path)
    mat=asset('M_EnvironmentSurface',SURFACES,unreal.Material,unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    # 원래 지면의 텍스처를 받아요. 기존 맵의 머티리얼은 수정하지 않아요.
    uv=node(mat,unreal.MaterialExpressionTextureCoordinate,-1800,0)
    tile=scalar(mat,'Tiling',3,-1800,160,'Textures')
    scaled=node(mat,unreal.MaterialExpressionMultiply,-1500,0);link(uv,scaled,'A');link(tile,scaled,'B')
    soil='/Game/InstanceMap/Plain/Landscape/rocky_trail_02_'
    maps={name:texture(mat,name,path,-1200,i*210,normal=name=='SurfaceNormal') for i,(name,path) in enumerate([
        ('SurfaceColor',soil+'diff_4k'),('SurfaceNormal','/Game/Fab/WaterMaterials/Textures/T_Stone_N'),
        ('SurfaceRoughness',soil+'rough_4k'),('SurfaceHeight',soil+'disp_4k'),
        ('CoverageTexture','/Game/Fab/WaterMaterials/Textures/T_Noises'),('SoilNormalEncoded',soil+'nor_gl_4k')])}
    for sample in maps.values():link(scaled,sample,'UVs')
    # 기존 GL 흙 노멀은 일반 선형 텍스처예요. 복사/압축 변경 없이 여기서만 디코딩해요.
    original_normal=ext.custom(mat,'float3 decoded=float3(Encoded.r*2-1,1-Encoded.g*2,Encoded.b*2-1); return float4(normalize(lerp(N,decoded,saturate(UseEncoded))),0);',
                    {'Encoded':maps['SoilNormalEncoded'],'N':maps['SurfaceNormal'],'UseEncoded':scalar(mat,'UseEncodedSoilNormal',1,-1200,1300,'Textures')},-850,850)
    original_normal=split(mat,original_normal,-600,850)
    tint=node(mat,unreal.MaterialExpressionVectorParameter,-1500,-220,parameter_name='SurfaceTint',default_value=unreal.LinearColor(1,1,1),group='Surface')
    color=node(mat,unreal.MaterialExpressionMultiply,-850,0);link(maps['SurfaceColor'],color,'A',output='RGB');link(tint,color,'B')
    rough=node(mat,unreal.MaterialExpressionMultiply,-850,210);link(maps['SurfaceRoughness'],rough,'A',output='R');link(scalar(mat,'RoughnessScale',1,-1200,1100),rough,'B')
    normal=node(mat,unreal.MaterialExpressionVertexNormalWS,-850,650)
    parameters={name:scalar(mat,name,value,-700,900+i*145) for i,(name,value) in enumerate([
        ('Wetness',0),('SnowCoverage',0),('IceCoverage',0),('FrostAmount',0),('Exposure',1),
        ('PuddleStrength',0),('PuddleLevel',.4),('WetDarkening',.58),('WetRoughness',.3),
        ('PuddleRoughness',.08),('NormalStrength',1),('SnowBreakup',.55),('IceInPuddles',0)])}
    # 수동 입력만 사용해요. 실제 날씨 연결은 이후 BP에서 지정하면 돼요.
    # 이 노드는 uasset에 저장되므로 게임 실행 시 Python 파일을 읽지 않아요.
    wet=node(mat,unreal.MaterialExpressionMaterialFunctionCall,-300,0);wet.set_material_function(fn)
    link(color,wet,'BaseColor');link(rough,wet,'Roughness');link(normal,wet,'WorldNormal')
    link(maps['SurfaceHeight'],wet,'Height',output='R')
    mask=scalar(mat,'SurfaceMask',1,-700,700);link(mask,wet,'Mask')
    for name in ('Exposure','Wetness','PuddleStrength','PuddleLevel','WetDarkening','WetRoughness','PuddleRoughness'):
        link(parameters[name],wet,name)
    frozen=node(mat,unreal.MaterialExpressionMaterialFunctionCall,0,0)
    ice_fn=LIB.load_asset(ROOT+'/Materials/MF_EnvironmentIce');assert ice_fn
    frozen.set_material_function(ice_fn)
    link(wet,frozen,'BaseColor',output='BaseColor');link(wet,frozen,'Roughness',output='Roughness')
    link(normal,frozen,'WorldNormal');link(parameters['IceCoverage'],frozen,'Override');link(parameters['FrostAmount'],frozen,'FrostAmount')
    exposed=node(mat,unreal.MaterialExpressionMultiply,-50,550);link(parameters['Exposure'],exposed,'A');link(mask,exposed,'B');link(exposed,frozen,'Exposure')
    # 웅덩이 결빙은 낮은 홈에만 남겨요. 젖음이 0으로 줄어도 얼음 마스크는 유지돼요.
    local_ice=ext.custom(mat,'return lerp(1,1-smoothstep(Level-.06,Level+.06,Height),saturate(InPuddles));',
             {'Level':parameters['PuddleLevel'],'Height':maps['SurfaceHeight'],'InPuddles':parameters['IceInPuddles']},-100,1200,scalar=True)
    height=node(mat,unreal.MaterialExpressionComponentMask,-300,1150,r=True,g=False,b=False,a=False);link(maps['SurfaceHeight'],height,output='RGB');link(height,local_ice,'Height')
    ice_exposure=node(mat,unreal.MaterialExpressionMultiply,200,1200);link(exposed,ice_exposure,'A');link(local_ice,ice_exposure,'B');link(ice_exposure,frozen,'Exposure')
    ice_tint=node(mat,unreal.MaterialExpressionVectorParameter,-350,700,parameter_name='IceTint',default_value=unreal.LinearColor(.035,.085,.105),group='Surface')
    link(ice_tint,frozen,'IceTint');link(scalar(mat,'IceCellSizeCm',600,-350,900),frozen,'CellSizeCm')
    snow=ext.custom(mat,'float up=smoothstep(.55,.93,normalize(N).z); float s=saturate(Snow); float patch=smoothstep(1-s-.15,1-s+.15,Noise); patch=lerp(patch,1,smoothstep(.9,1,s)); return saturate(lerp(s,patch,saturate(Breakup))*smoothstep(0,.08,s)*saturate(Exposure)*saturate(Mask)*up);',
         {'N':normal,'Snow':parameters['SnowCoverage'],'Exposure':parameters['Exposure'],'Mask':mask,'Breakup':parameters['SnowBreakup'],'Noise':maps['CoverageTexture']},400,600,scalar=True)
    # 노이즈 입력은 R 채널 하나만 사용해요.
    noise=node(mat,unreal.MaterialExpressionComponentMask,100,800,r=True,g=False,b=False,a=False);link(maps['CoverageTexture'],noise,output='RGB');link(noise,snow,'Noise')
    snow_tint=node(mat,unreal.MaterialExpressionVectorParameter,100,1050,parameter_name='SnowTint',default_value=unreal.LinearColor(.68,.73,.76),group='Surface')
    final_color=node(mat,unreal.MaterialExpressionLinearInterpolate,800,0)
    link(frozen,final_color,'A',output='BaseColor');link(snow_tint,final_color,'B');link(snow,final_color,'Alpha')
    final_rough=node(mat,unreal.MaterialExpressionLinearInterpolate,800,210,const_b=.87)
    link(frozen,final_rough,'A',output='Roughness');link(snow,final_rough,'Alpha')
    MEL.connect_material_property(final_color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(final_rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    # 물이 고인 부분은 미세 요철을 완화하고, 눈은 작은 결정 무늬로 반사 방향을 바꿔요.
    crystals=ice.texture_parameter(mat,'SnowCrystalTexture',ROOT+'/Materials/T_Environment_FrostCrystals',100,1250)
    normal_fx=ext.custom(mat,'float ice=saturate(Ice)*saturate(Exposure)*smoothstep(-.12,.9,normalize(WorldNormal).z); float flatten=saturate(max(Puddle,ice*.75)); float3 n=normalize(lerp(float3(N.xy*Strength,N.z),float3(0,0,1),flatten)); float2 q=UV*7.3; float h=Texture2DSample(Crystals,CrystalsSampler,q).r; float2 crystal=float2(Texture2DSample(Crystals,CrystalsSampler,q+float2(.002,0)).r-h,Texture2DSample(Crystals,CrystalsSampler,q+float2(0,.002)).r-h)*.8; return float4(normalize(lerp(n,normalize(float3(-crystal,1)),Snow)),0);',
         {'N':original_normal,'Strength':parameters['NormalStrength'],'Puddle':wet,'Ice':parameters['IceCoverage'],'Exposure':ice_exposure,'WorldNormal':normal,'Snow':snow,'UV':scaled,'Crystals':crystals},800,500)
    link(wet,normal_fx,'Puddle',output='PuddleMask')
    MEL.connect_material_property(split(mat,normal_fx,1100,500),'',unreal.MaterialProperty.MP_NORMAL)
    coat=ext.custom(mat,'float ice=saturate(Ice)*saturate(Exposure)*smoothstep(-.12,.9,normalize(N).z); return saturate(max(Puddle,ice*(1-Frost)))*(1-Snow);',{'Puddle':wet,'Ice':parameters['IceCoverage'],'Frost':parameters['FrostAmount'],'Snow':snow,'Exposure':ice_exposure,'N':normal},800,900,scalar=True)
    link(wet,coat,'Puddle',output='PuddleMask')
    setp(mat,'ClearCoat',f'(Expression={coat.get_path_name()},OutputIndex=0)')
    setp(mat,'ClearCoatRoughness',f'(Expression={scalar(mat,"WaterCoatRoughness",.07,800,1100).get_path_name()},OutputIndex=0)')
    MEL.recompile_material(mat);save(mat);return mat


PRESETS={
    'DrySoil':('Soil',{}),
    'WetSoil':('Soil',dict(Wetness=1,WetRoughness=.38)),
    'PuddleSoil':('Soil',dict(Wetness=1,PuddleStrength=1,PuddleLevel=.52)),
    'SnowSoil':('Soil',dict(SnowCoverage=.9,SnowBreakup=.7)),
    'ThawingSoil':('Soil',dict(Wetness=.95,PuddleStrength=.8,PuddleLevel=.42,SnowCoverage=.42,SnowBreakup=1)),
    'DryStone':('Stone',{}),
    'WetStone':('Stone',dict(Wetness=1,WetDarkening=.68,WetRoughness=.2)),
    'FrostStone':('Stone',dict(IceCoverage=.65,FrostAmount=.96)),
    'FrozenPuddle':('Stone',dict(Wetness=.8,PuddleStrength=1,PuddleLevel=.55,IceCoverage=.85,FrostAmount=.08,IceInPuddles=1)),
    'DrySand':('Sand',dict(RoughnessScale=1.2)),
    'WetSand':('Sand',dict(Wetness=1,WetDarkening=.48,WetRoughness=.46)),
}


def surface_presets(parent):
    paths=[]
    for name,(kind,values) in PRESETS.items():
        label='MI_Surface_'+name;path=SURFACES+'/'+label
        if LIB.does_asset_exist(path) and not REFRESH:paths.append(path);continue
        inst=asset(label,SURFACES,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(inst,parent)
        if kind!='Soil':
            stem='/Game/Fab/WaterMaterials/Textures/T_'+kind
            for parameter,suffix in [('SurfaceColor',''),('SurfaceNormal','_N')]:
                tex=LIB.load_asset(stem+suffix);assert tex
                MEL.set_material_instance_texture_parameter_value(inst,parameter,tex)
                assert MEL.get_material_instance_texture_parameter_value(inst,parameter)==tex
            MEL.set_material_instance_scalar_parameter_value(inst,'UseEncodedSoilNormal',0)
        for parameter,value in values.items():
            # UE 5.8의 setter는 적용 후에도 false를 반환해요. 실제 저장된 값으로 확인해요.
            MEL.set_material_instance_scalar_parameter_value(inst,parameter,value)
            assert abs(MEL.get_material_instance_scalar_parameter_value(inst,parameter)-value)<.0001
        save(inst);paths.append(path)
    return paths


def atmosphere_material(kind,tint,density):
    name='M_Atmosphere_'+kind;path=ATMOSPHERE+'/'+name
    if LIB.does_asset_exist(path) and not REFRESH:return LIB.load_asset(path)
    mat=asset(name,ATMOSPHERE,unreal.Material,unreal.MaterialFactoryNew());MEL.delete_all_material_expressions(mat)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    uv=node(mat,unreal.MaterialExpressionTextureCoordinate,-900,0)
    time=node(mat,unreal.MaterialExpressionTime,-900,180)
    tex=ice.texture_parameter(mat,'WispTexture','/Game/Fab/WaterMaterials/Textures/T_Noises',-900,360)
    color=node(mat,unreal.MaterialExpressionVectorParameter,-900,540,parameter_name='Tint',default_value=unreal.LinearColor(*tint),group='Atmosphere')
    amount=scalar(mat,'Density',density,-900,720,'Atmosphere')
    result=ext.custom(mat,(SHADERS/'EnvironmentAtmosphereSprite.ush').read_text(encoding='utf8'),{'UV':uv,'Time':time,'Noise':tex,'Tint':color,'Density':amount},-350,0)
    MEL.connect_material_property(split(mat,result,0,0),'',unreal.MaterialProperty.MP_BASE_COLOR)
    alpha=split(mat,result,0,180,False)
    pc=node(mat,unreal.MaterialExpressionParticleColor,-350,500)
    opacity=node(mat,unreal.MaterialExpressionMultiply,230,180);link(alpha,opacity,'A');link(pc,opacity,'B',output='A')
    fade=node(mat,unreal.MaterialExpressionDepthFade,480,180);link(opacity,fade,'Opacity');link(scalar(mat,'IntersectionFadeCm',50 if kind=='GroundMist' else 25,230,450,'Atmosphere'),fade,'FadeDistance')
    MEL.connect_material_property(fade,'',unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat);MEL.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NIAGARA_SPRITES);MEL.recompile_material(mat);save(mat);return mat


def atmosphere_system(kind,mat):
    path=SYSTEMS+'/NS_Atmosphere_'+kind
    if LIB.does_asset_exist(path) and not REFRESH:return LIB.load_asset(path)
    system=LIB.load_asset(path) if LIB.does_asset_exist(path) else LIB.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/FountainLightweight',path)
    specs={
        'GroundMist':[(18,(5,8),(130,230),(8,18),0,.7,(1000,750,35))],
        'SandDrift':[(45,(2,4),(35,85),(95,150),0,.7,(1000,600,30)),(90,(1.2,2.1),(.8,1.5),(110,190),-8,.75,(1000,600,18))],
        'SnowDrift':[(22,(2,3.5),(50,110),(55,100),0,.65,(1000,600,35)),(75,(1.5,3.5),(1.4,2.6),(75,140),-12,.9,(1000,600,80))],
    }
    for i,(rate,life,size,speed,gravity,alpha,box) in enumerate(specs[kind]):
        label='Wisp' if i==0 else 'FineGrains'
        base.water.configure_layer(system,label,i>0,mat,False,rate,life,size,0,speed,12,gravity,alpha)
        emitter=base.water.EDIT.water_layer(system,label,False)
        mods={m.get_class().get_name().replace('NiagaraStatelessModule_',''):m for m in emitter.get_editor_property('modules')}
        setp(mods['ShapeLocation'],'ShapePrimitive','Box');setp(mods['ShapeLocation'],'BoxSize',base.vector(box))
        setp(mods['AddVelocity'],'ConeDirection',base.vector((1,0,.015)))
        setp(mods['ApplyOwnerScaleToAttributes'],'bModuleEnabled','False')
        if i==0:
            # 낮은 층을 가로로 흐르게 해요. 작은 정사각형 점들이 흩어지는 모양을 피해요.
            lo,hi=((240,80),(400,140)) if kind=='GroundMist' else ((160,35),(260,60))
            setp(mods['InitializeParticle'],'SpriteSizeDistribution',f'(Min=(X={lo[0]},Y={lo[1]}),Max=(X={hi[0]},Y={hi[1]}),Mode=NonUniformRange,ChannelConstantsAndRanges=({lo[0]},{hi[0]},{lo[1]},{hi[1]}))')
            setp(mods['InitializeParticle'],'SpriteRotationDistribution',base.scalar(-5,5))
        white='(Keys=((Time=0,Value=1),(Time=1,Value=1)))'
        fade='(Keys=((Time=0,Value=0),(Time=.18,Value=.8),(Time=.65,Value=.65),(Time=1,Value=0)))'
        setp(mods['ScaleColor'],'ScaleDistribution',f'(Mode=NonUniformCurve,LookupValueMode=0,ChannelConstantsAndRanges=,ChannelCurves=({white},{white},{white},{fade}))')
        setp(emitter,'FixedBounds','(Min=(X=-1600,Y=-900,Z=-250),Max=(X=1600,Y=900,Z=500),IsValid=1)')
    assert base.water.EDIT.finish_water_system(system),path
    save(system);return system


def atmosphere_blueprint(kind,system):
    name='BP_Atmosphere_'+kind;path=BLUEPRINTS+'/'+name
    if LIB.does_asset_exist(path) and not REFRESH:return LIB.load_asset(path)
    factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.NiagaraActor)
    bp=asset(name,BLUEPRINTS,unreal.Blueprint,factory)
    component=unreal.get_default_object(bp.generated_class()).get_component_by_class(unreal.NiagaraComponent)
    component.set_asset(system)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp);return bp


def main():
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT,'/Niagara/DefaultAssets/Templates/Systems','/Game/Fab/WaterMaterials','/Game/InstanceMap/Plain/Landscape'],force_rescan=True)
    fn=wetness_function();parent=surface_material(fn);presets=surface_presets(parent)
    effects=[]
    for kind,tint,density in [('GroundMist',(.55,.61,.65),.25),('SandDrift',(.5,.36,.19),.5),('SnowDrift',(.7,.77,.82),.35)]:
        system=atmosphere_system(kind,atmosphere_material(kind,tint,density))
        bp=atmosphere_blueprint(kind,system);effects.append({'system':system.get_path_name(),'blueprint':bp.get_path_name()})
    report={'surface_function':fn.get_path_name(),'surface_parent':parent.get_path_name(),'presets':presets,'effects':effects,'game_maps_changed':False,'runtime_pc_paths':False}
    out=Path(unreal.Paths.project_saved_dir())/'Codex/WeatherAssetLibrary';out.mkdir(parents=True,exist_ok=True)
    (out/'authored.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    unreal.log('WEATHER ASSET LIBRARY AUTHORED')


if __name__=='__main__':main()
