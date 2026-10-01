"""UE 5.8 에디터: 얼음 함수/머티리얼, 해안 거품 Niagara와 배치용 BP 제작.
기존 맵은 수정하지 않아요. --refresh가 있을 때만 제작한 그래프를 갱신해요.
"""
import json
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_instance_weather as base
import extend_instance_weather as ext

ROOT,LIB,MEL,TOOLS=base.ROOT,base.LIB,base.MEL,base.TOOLS
node,link,save,setp=ext.node,ext.link,base.save,base.setp
SHADERS=Path(__file__).with_name('Shaders')
# 명시적으로 --refresh를 줄 때만 이 스크립트가 만든 그래프를 다시 만들어요.
REFRESH='--refresh' in sys.argv


def texture_parameter(owner,name,path,x,y,normal=False):
    return node(owner,unreal.MaterialExpressionTextureObjectParameter,x,y,
                parameter_name=name,texture=LIB.load_asset(path),group='Environment',
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)


def import_masks():
    # 원본은 SourceArt에 보관하고 실행 시에는 저장된 uasset만 사용해요.
    for source,name in [('IceFractures','T_Environment_IceFractures'),('FrostCrystals','T_Environment_FrostCrystals')]:
        path=ROOT+'/Materials/'+name
        tex=LIB.load_asset(path) if LIB.does_asset_exist(path) else None
        if tex and not REFRESH:continue
        if not tex:
            file=Path(unreal.Paths.project_dir())/'SourceArt/Weather'/f'{source}.png'
            assert file.exists(),f'원본 텍스처가 없어요: {file}'
            task=unreal.AssetImportTask();task.filename=str(file);task.destination_path=ROOT+'/Materials'
            task.destination_name=name;task.automated=True;task.save=False
            TOOLS.import_asset_tasks([task]);tex=LIB.load_asset(path)
        assert tex
        tex.set_editor_property('srgb',False)
        tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
        # 원본 비정규 해상도를 빌드할 때 2의 거듭제곱으로 맞춰 mip/스트리밍을 켜요.
        setp(tex,'PowerOfTwoMode','StretchToPowerOfTwo')
        save(tex)


def parameter(owner,name,value,x,y):
    return node(owner,unreal.MaterialExpressionScalarParameter,x,y,
                parameter_name=name,default_value=value,group='Environment')


def collection():
    mpc=LIB.load_asset(ROOT+'/MPC_InstanceWeather')
    entries=list(mpc.get_editor_property('scalar_parameters'))
    original_count=len(entries)
    names={str(p.get_editor_property('parameter_name')) for p in entries}
    for name,default in [('IceMm',0),('GroundTemperatureC',20),('WaveHeightM',0),('TideLevelM',0)]:
        if name in names:continue
        p=unreal.CollectionScalarParameter();p.set_editor_property('parameter_name',name)
        p.set_editor_property('default_value',default);entries.append(p)
    if len(entries)!=original_count:
        mpc.set_editor_property('scalar_parameters',entries);save(mpc)
    return mpc


def ice_function(mpc):
    path=ROOT+'/Materials/MF_EnvironmentIce'
    fn=LIB.load_asset(path) if LIB.does_asset_exist(path) else None
    if fn and not REFRESH:return fn
    if not fn:fn=TOOLS.create_asset('MF_EnvironmentIce',ROOT+'/Materials',unreal.MaterialFunction,unreal.MaterialFunctionFactoryNew())
    # 함수 입출력의 ID를 보존해 이미 연결한 머티리얼의 핀을 유지해요.
    expressions=MEL.get_material_function_expressions(fn)
    inputs={str(n.get_editor_property('input_name')):n for n in expressions if isinstance(n,unreal.MaterialExpressionFunctionInput)}
    outputs={str(n.get_editor_property('output_name')):n for n in expressions if isinstance(n,unreal.MaterialExpressionFunctionOutput)}
    for n in expressions:
        if not isinstance(n,(unreal.MaterialExpressionFunctionInput,unreal.MaterialExpressionFunctionOutput)):MEL.delete_material_expression_in_function(fn,n)
    fn.set_editor_property('description','기존 표면 색/거칠기 + 환경 얼음(mm)/지표 온도. Override=-1은 환경값, 0~1은 미리보기.')
    fn.set_editor_property('expose_to_library',True)
    values={}
    specs=[('BaseColor',(.18,.22,.24,0),True),('Roughness',(.7,0,0,0),False),
           ('WorldNormal',(0,0,1,0),True),('Exposure',(1,0,0,0),False),
           ('Override',(-1,0,0,0),False),('FullThicknessMm',(5,0,0,0),False),
           ('CellSizeCm',(180,0,0,0),False),('FrostAmount',(.45,0,0,0),False),
           ('IceTint',(.065,.28,.34,0),True)]
    for i,(name,preview,vector) in enumerate(specs):
        n=inputs.get(name) or node(fn,unreal.MaterialExpressionFunctionInput,-800,i*150,input_name=name,
               input_type=unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3 if vector else unreal.FunctionInputType.FUNCTION_INPUT_SCALAR,
               sort_priority=i,use_preview_value_as_default=True)
        setp(n,'PreviewValue','(X=%s,Y=%s,Z=%s,W=%s)'%preview);values[name]=n
    values['World']=node(fn,unreal.MaterialExpressionWorldPosition,-1300,0)
    values['View']=node(fn,unreal.MaterialExpressionCameraVectorWS,-1300,-200)
    values['Cracks']=texture_parameter(fn,'IceFractureTexture',ROOT+'/Materials/T_Environment_IceFractures',-1300,-400)
    values['Frost']=texture_parameter(fn,'FrostCrystalTexture',ROOT+'/Materials/T_Environment_FrostCrystals',-1300,-600)
    for i,(pin,name) in enumerate([('IceMm','IceMm'),('Temperature','GroundTemperatureC'),('Wetness','Wetness')]):
        values[pin]=node(fn,unreal.MaterialExpressionCollectionParameter,-1300,180+i*180,collection=mpc,parameter_name=name)
    shader=ext.custom(fn,(SHADERS/'EnvironmentIce.ush').read_text(encoding='utf8'),values,0,0)
    for i,(name,rgb) in enumerate([('BaseColor',True),('Roughness',False)]):
        mask=node(fn,unreal.MaterialExpressionComponentMask,300,i*170,r=rgb,g=rgb,b=rgb,a=not rgb);link(shader,mask)
        out=outputs.get(name) or node(fn,unreal.MaterialExpressionFunctionOutput,550,i*170,output_name=name,sort_priority=i);link(mask,out)
    MEL.update_material_function(fn);save(fn)
    return fn


def ice_material(fn):
    path=ROOT+'/Materials/M_Environment_Ice'
    mat=LIB.load_asset(path) if LIB.does_asset_exist(path) else None
    if mat and not REFRESH:return mat
    if not mat:mat=TOOLS.create_asset('M_Environment_Ice',ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    mat.set_editor_property('tangent_space_normal',False)
    call=node(mat,unreal.MaterialExpressionMaterialFunctionCall,200,0);call.set_material_function(fn)
    color=node(mat,unreal.MaterialExpressionVectorParameter,-500,-250,parameter_name='BaseColor',default_value=unreal.LinearColor(.17,.21,.24),group='Environment')
    tint=node(mat,unreal.MaterialExpressionVectorParameter,-500,-80,parameter_name='IceTint',default_value=unreal.LinearColor(.035,.085,.105),group='Environment')
    normal=node(mat,unreal.MaterialExpressionVertexNormalWS,-500,80)
    original=node(mat,unreal.MaterialExpressionTextureSampleParameter2D,-900,-450,parameter_name='SurfaceTexture',texture=LIB.load_asset('/Game/Fab/WaterMaterials/Textures/T_Stone'),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    mix=node(mat,unreal.MaterialExpressionLinearInterpolate,-450,-400)
    link(color,mix,'A');link(original,mix,'B')
    link(parameter(mat,'SurfaceTextureWeight',0,-900,-250),mix,'Alpha')
    link(mix,call,'BaseColor');link(tint,call,'IceTint');link(normal,call,'WorldNormal')
    params={}
    for i,(name,value) in enumerate([('Roughness',.75),('Exposure',1),('Override',-1),('FullThicknessMm',5),('CellSizeCm',600),('FrostAmount',.4)]):
        params[name]=parameter(mat,name,value,-500,250+i*140);link(params[name],call,name)
    MEL.connect_material_property(call,'BaseColor',unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(call,'Roughness',unreal.MaterialProperty.MP_ROUGHNESS)
    # 텍스처의 미세 높이 기울기로 결정 표면의 반사 방향을 바꿔요.
    world=node(mat,unreal.MaterialExpressionWorldPosition,200,400)
    tex=texture_parameter(mat,'FrostCrystalTexture',ROOT+'/Materials/T_Environment_FrostCrystals',100,600)
    ice=node(mat,unreal.MaterialExpressionCollectionParameter,100,800,collection=collection(),parameter_name='IceMm')
    n=ext.custom(mat,'float2 uv=P.xy/max(Size*.16,1); float e=.001; float h=Texture2DSample(T,TSampler,uv).r; float2 d=float2(Texture2DSample(T,TSampler,uv+float2(e,0)).r-h,Texture2DSample(T,TSampler,uv+float2(0,e)).r-h); float amount=Override>=0?saturate(Override):smoothstep(.02,max(Full,.03),Ice); return float4(normalize(N+float3(d*.75*Frost,0)*amount*saturate(Exposure)*smoothstep(.45,.9,N.z)),0);',{'N':normal,'P':world,'T':tex,'Size':params['CellSizeCm'],'Frost':params['FrostAmount'],'Override':params['Override'],'Full':params['FullThicknessMm'],'Ice':ice,'Exposure':params['Exposure']},450,400)
    rgb=node(mat,unreal.MaterialExpressionComponentMask,700,400,r=True,g=True,b=True,a=False);link(n,rgb)
    stone_normal=node(mat,unreal.MaterialExpressionTextureSampleParameter2D,-100,1100,parameter_name='SurfaceNormal',texture=LIB.load_asset('/Game/Fab/WaterMaterials/Textures/T_Stone_N'),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    transform=node(mat,unreal.MaterialExpressionTransform,150,1100)
    setp(transform,'TransformSourceType','TRANSFORMSOURCE_Tangent');setp(transform,'TransformType','TRANSFORM_World')
    link(stone_normal,transform)
    blended=node(mat,unreal.MaterialExpressionLinearInterpolate,700,1200)
    link(rgb,blended,'A');link(transform,blended,'B');link(parameter(mat,'SurfaceNormalWeight',0,400,1250),blended,'Alpha')
    normalized=node(mat,unreal.MaterialExpressionNormalize,950,1200);link(blended,normalized)
    MEL.connect_material_property(normalized,'',unreal.MaterialProperty.MP_NORMAL)
    spec=parameter(mat,'Specular',.28,500,650);MEL.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
    coat=ext.custom(mat,'float amount=Override>=0?saturate(Override):smoothstep(.02,max(Full,.03),Ice); return amount*(1-saturate(Frost))*saturate(Exposure)*smoothstep(-.12,.9,N.z);',{'Override':params['Override'],'Full':params['FullThicknessMm'],'Ice':ice,'Frost':params['FrostAmount'],'Exposure':params['Exposure'],'N':normal},700,800,scalar=True)
    # Clear Coat의 두 핀은 엔진에서 CustomData0/1로 저장돼요.
    setp(mat,'ClearCoat',f'(Expression={coat.get_path_name()},OutputIndex=0)')
    setp(mat,'ClearCoatRoughness',f'(Expression={parameter(mat,"IceCoatRoughness",.085,700,1000).get_path_name()},OutputIndex=0)')
    MEL.recompile_material(mat);save(mat)
    return mat


def shore_displacement(mat,mpc,wave):
    # BP를 돌려 놓아도 +Y 파도 방향이 BP 회전을 따라가요. 조석은 월드 높이예요.
    tide=node(mat,unreal.MaterialExpressionCollectionParameter,-600,950,collection=mpc,parameter_name='TideLevelM')
    time=node(mat,unreal.MaterialExpressionTime,-900,950)
    position=node(mat,unreal.MaterialExpressionWorldPosition,-900,1100)
    forward=node(mat,unreal.MaterialExpressionConstant3Vector,-900,1250,constant=unreal.LinearColor(0,1,0))
    direction=node(mat,unreal.MaterialExpressionTransform,-600,1250)
    setp(direction,'TransformSourceType','TRANSFORMSOURCE_Local');setp(direction,'TransformType','TRANSFORM_World');link(forward,direction)
    offset=ext.custom(mat,'float3 d=normalize(Direction); float phase=Time*1.05+dot(P,cross(d,float3(0,0,1)))*.0013; return float4(d*sin(phase)*min(Wave*130,110)+float3(0,0,Tide*100+sin(phase*2)*min(Wave*5,3)),0);',{'Tide':tide,'Time':time,'Wave':wave,'P':position,'Direction':direction},-250,950)
    xyz=node(mat,unreal.MaterialExpressionComponentMask,50,950,r=True,g=True,b=True,a=False);link(offset,xyz)
    MEL.connect_material_property(xyz,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)


def foam_material(mpc):
    path=ROOT+'/Materials/M_Environment_ShoreFoam'
    mat=LIB.load_asset(path) if LIB.does_asset_exist(path) else None
    if mat and not REFRESH:return mat
    if not mat:mat=TOOLS.create_asset('M_Environment_ShoreFoam',ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT);mat.set_editor_property('two_sided',True)
    mat.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    uv=node(mat,unreal.MaterialExpressionTextureCoordinate,-900,0)
    age=node(mat,unreal.MaterialExpressionParticleRelativeTime,-900,180)
    wave=node(mat,unreal.MaterialExpressionCollectionParameter,-900,360,collection=mpc,parameter_name='WaveHeightM')
    tint=node(mat,unreal.MaterialExpressionVectorParameter,-900,540,parameter_name='FoamTint',default_value=unreal.LinearColor(.83,.89,.9),group='Environment')
    opacity=parameter(mat,'FoamOpacity',1,-900,700)
    texture=texture_parameter(mat,'FoamTexture','/Game/Fab/WaterMaterials/Textures/T_Ocean_Foam',-1200,0)
    fx=ext.custom(mat,(SHADERS/'EnvironmentFoam.ush').read_text(encoding='utf8'),{'UV':uv,'Age':age,'Wave':wave,'Tint':tint,'Opacity':opacity,'Foam':texture},-450,0)
    rgb=node(mat,unreal.MaterialExpressionComponentMask,-150,0,r=True,g=True,b=True,a=False);link(fx,rgb)
    alpha=node(mat,unreal.MaterialExpressionComponentMask,-150,180,r=False,g=False,b=False,a=True);link(fx,alpha)
    pc=node(mat,unreal.MaterialExpressionParticleColor,-400,700)
    a=node(mat,unreal.MaterialExpressionMultiply,100,180);link(alpha,a,'A');link(pc,a,'B','A')
    MEL.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=parameter(mat,'FoamRoughness',.75,100,450)
    MEL.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(a,'',unreal.MaterialProperty.MP_OPACITY)
    shore_displacement(mat,mpc,wave)
    # 먼저 그래프/WPO 캐시를 갱신해야 UE 5.8 사용 플래그의 셰이더 검사와 일치해요.
    MEL.recompile_material(mat)
    MEL.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NIAGARA_SPRITES)
    MEL.recompile_material(mat);save(mat)
    return mat


def shore_foam(mat):
    path=ROOT+'/NS_Environment_ShoreFoam'
    system=LIB.load_asset(path) if LIB.does_asset_exist(path) else None
    if system and not REFRESH:return system
    if not system:system=LIB.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/FountainLightweight',path)
    water=base.water
    for i,(name,count,size,life,speed,alpha) in enumerate([
        ('FoamLace',18,(180,300),(3.5,5.5),26,1),
        ('SmallBubbles',38,(22,55),(1.6,2.8),45,.9)]):
        water.configure_layer(system,name,i>0,mat,False,count,life,size,0,(speed,speed),0,0,alpha)
        emitter=water.EDIT.water_layer(system,name,False)
        mods={m.get_class().get_name().replace('NiagaraStatelessModule_',''):m for m in emitter.get_editor_property('modules')}
        # 템플릿의 색 곡선을 물려받지 않고 거품의 수명에 맞게 투명도 곡선을 지정해요.
        white='(Keys=((Time=0,Value=1),(Time=1,Value=1)))'
        fade='(Keys=((Time=0,Value=0),(Time=.12,Value=1),(Time=.65,Value=.9),(Time=1,Value=0)))'
        setp(mods['ScaleColor'],'ScaleDistribution',f'(Mode=NonUniformCurve,LookupValueMode=0,ChannelConstantsAndRanges=,ChannelCurves=({white},{white},{white},{fade}))')
        shape=mods['ShapeLocation'];setp(shape,'ShapePrimitive','Box');setp(shape,'BoxSize',water.vector((1200,140,2)))
        setp(mods['AddVelocity'],'ConeDirection',water.vector((0,1,0)))
        facing=mods['SpriteFacingAndAlignment'];setp(facing,'bModuleEnabled','True');setp(facing,'SpriteFacing',water.vector((0,0,1)))
        # 얇은 물막이 퍼진 뒤 사라지도록 수명에 맞춰 입자 크기도 키워요.
        growth='(Keys=((Time=0,Value=.45),(Time=.45,Value=1),(Time=1,Value=1.5)))'
        setp(mods['ScaleSpriteSize'],'bModuleEnabled','True')
        setp(mods['ScaleSpriteSize'],'ScaleDistribution',f'(Mode=UniformCurve,ChannelCurves=({growth}))')
        renderer=unreal.find_object(None,emitter.get_path_name()+'.Renderer');setp(renderer,'FacingMode','CustomFacingVector')
        setp(emitter,'FixedBounds','(Min=(X=-1200,Y=-500,Z=-1000),Max=(X=1200,Y=500,Z=1000),IsValid=1)')
    # 평면 포말 위로 짧게 솟는 물보라. 기존 4x4 flipbook을 재사용해요.
    water.configure_layer(system,'SeaSpray',True,spray_material(collection()),False,26,(.45,.8),(25,65),0,(55,95),22,-145,.65)
    emitter=water.EDIT.water_layer(system,'SeaSpray',False)
    mods={m.get_class().get_name().replace('NiagaraStatelessModule_',''):m for m in emitter.get_editor_property('modules')}
    setp(mods['ShapeLocation'],'ShapePrimitive','Box');setp(mods['ShapeLocation'],'BoxSize',water.vector((1200,65,4)))
    setp(mods['AddVelocity'],'ConeDirection',water.vector((0,.5,1)))
    setp(mods['InitializeParticle'],'SpriteRotationDistribution',water.scalar(-12,12))
    renderer=unreal.find_object(None,emitter.get_path_name()+'.Renderer');setp(renderer,'FacingMode','FaceCamera')
    setp(emitter,'FixedBounds','(Min=(X=-1200,Y=-500,Z=-1000),Max=(X=1200,Y=500,Z=1000),IsValid=1)')
    assert water.EDIT.finish_water_system(system),'해안 거품 Niagara 컴파일 실패'
    save(system)
    return system


def spray_material(mpc):
    path=ROOT+'/Materials/M_Environment_SeaSpray'
    mat=LIB.load_asset(path) if LIB.does_asset_exist(path) else None
    if mat and not REFRESH:return mat
    if not mat:mat=TOOLS.create_asset('M_Environment_SeaSpray',ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    uv=node(mat,unreal.MaterialExpressionTextureCoordinate,-600,0)
    age=node(mat,unreal.MaterialExpressionParticleRelativeTime,-600,160)
    tex=texture_parameter(mat,'SprayTexture','/Game/Fab/WaterMaterials/Textures/T_WaterSplash2',-600,320)
    wave=node(mat,unreal.MaterialExpressionCollectionParameter,-600,480,collection=mpc,parameter_name='WaveHeightM')
    fx=ext.custom(mat,'float frame=min(15,floor(saturate(Age)*16)); float2 uv=(UV+float2(fmod(frame,4),floor(frame/4)))*.25; float v=Texture2DSample(T,TSampler,uv).r; return float4(.8,.88,.9,v*smoothstep(.005,.3,Wave));',{'UV':uv,'Age':age,'T':tex,'Wave':wave},-100,0)
    rgb=node(mat,unreal.MaterialExpressionComponentMask,200,0,r=True,g=True,b=True,a=False);link(fx,rgb)
    alpha=node(mat,unreal.MaterialExpressionComponentMask,200,180,r=False,g=False,b=False,a=True);link(fx,alpha)
    pc=node(mat,unreal.MaterialExpressionParticleColor,0,420)
    opacity=node(mat,unreal.MaterialExpressionMultiply,400,180);link(alpha,opacity,'A');link(pc,opacity,'B','A')
    MEL.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    shore_displacement(mat,mpc,wave)
    MEL.recompile_material(mat);MEL.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NIAGARA_SPRITES)
    MEL.recompile_material(mat);save(mat)
    return mat


def main():
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT,'/Niagara/DefaultAssets/Templates/Systems'],force_rescan=True)
    import_masks();mpc=collection();fn=ice_function(mpc);mat=ice_material(fn);foam=shore_foam(foam_material(mpc))
    for name,frost,tint in [('MI_Environment_ClearIce',.08,(.035,.085,.105)),('MI_Environment_Frost',.95,(.06,.13,.16))]:
        path=ROOT+'/Materials/'+name
        if LIB.does_asset_exist(path) and not REFRESH:continue
        inst=LIB.load_asset(path) if LIB.does_asset_exist(path) else TOOLS.create_asset(name,ROOT+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(inst,mat);MEL.set_material_instance_scalar_parameter_value(inst,'FrostAmount',frost)
        MEL.set_material_instance_vector_parameter_value(inst,'IceTint',unreal.LinearColor(*tint))
        MEL.set_material_instance_scalar_parameter_value(inst,'CellSizeCm',1800 if name.endswith('ClearIce') else 450)
        MEL.set_material_instance_scalar_parameter_value(inst,'SurfaceTextureWeight',1 if name.endswith('Frost') else 0)
        MEL.set_material_instance_scalar_parameter_value(inst,'SurfaceNormalWeight',.8 if name.endswith('Frost') else 0)
        save(inst)
    path=ROOT+'/BP_EnvironmentShoreFoam'
    if not LIB.does_asset_exist(path):
        bp=base.blueprint('BP_EnvironmentShoreFoam',unreal.NiagaraActor)
        cdo=unreal.get_default_object(bp.generated_class());cdo.get_component_by_class(unreal.NiagaraComponent).set_asset(foam)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    report={'ice':mat.get_path_name(),'function':fn.get_path_name(),'foam':foam.get_path_name(),'niagara_ready':True}
    (Path(unreal.Paths.project_saved_dir())/'ice-shore-authored.json').write_text(json.dumps(report,indent=2),encoding='utf8')


if __name__=='__main__':main()
