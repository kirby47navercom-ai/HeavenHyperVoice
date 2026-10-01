"""UE 5.8 에디터: 얼음 함수/머티리얼, 해안 거품 Niagara와 배치용 BP 제작.
기존 맵은 수정하지 않아요. 이미 있는 에셋은 작가가 수정한 상태를 보존해요.
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
    if LIB.does_asset_exist(path):return LIB.load_asset(path)
    fn=TOOLS.create_asset('MF_EnvironmentIce',ROOT+'/Materials',unreal.MaterialFunction,unreal.MaterialFunctionFactoryNew())
    fn.set_editor_property('description','기존 표면 색/거칠기 + 환경 얼음(mm)/지표 온도. Override=-1은 환경값, 0~1은 미리보기.')
    fn.set_editor_property('expose_to_library',True)
    values={}
    specs=[('BaseColor',(.18,.22,.24,0),True),('Roughness',(.7,0,0,0),False),
           ('WorldNormal',(0,0,1,0),True),('Exposure',(1,0,0,0),False),
           ('Override',(-1,0,0,0),False),('FullThicknessMm',(5,0,0,0),False),
           ('CellSizeCm',(180,0,0,0),False),('FrostAmount',(.45,0,0,0),False),
           ('IceTint',(.065,.28,.34,0),True)]
    for i,(name,preview,vector) in enumerate(specs):
        n=node(fn,unreal.MaterialExpressionFunctionInput,-800,i*150,input_name=name,
               input_type=unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3 if vector else unreal.FunctionInputType.FUNCTION_INPUT_SCALAR,
               sort_priority=i,use_preview_value_as_default=True)
        setp(n,'PreviewValue','(X=%s,Y=%s,Z=%s,W=%s)'%preview);values[name]=n
    values['World']=node(fn,unreal.MaterialExpressionWorldPosition,-1300,0)
    for i,(pin,name) in enumerate([('IceMm','IceMm'),('Temperature','GroundTemperatureC'),('Wetness','Wetness')]):
        values[pin]=node(fn,unreal.MaterialExpressionCollectionParameter,-1300,180+i*180,collection=mpc,parameter_name=name)
    shader=ext.custom(fn,(SHADERS/'EnvironmentIce.ush').read_text(encoding='utf8'),values,0,0)
    for i,(name,rgb) in enumerate([('BaseColor',True),('Roughness',False)]):
        mask=node(fn,unreal.MaterialExpressionComponentMask,300,i*170,r=rgb,g=rgb,b=rgb,a=not rgb);link(shader,mask)
        out=node(fn,unreal.MaterialExpressionFunctionOutput,550,i*170,output_name=name,sort_priority=i);link(mask,out)
    MEL.update_material_function(fn);save(fn)
    return fn


def ice_material(fn):
    path=ROOT+'/Materials/M_Environment_Ice'
    if LIB.does_asset_exist(path):return LIB.load_asset(path)
    mat=TOOLS.create_asset('M_Environment_Ice',ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('tangent_space_normal',False)
    call=node(mat,unreal.MaterialExpressionMaterialFunctionCall,200,0);call.set_material_function(fn)
    color=node(mat,unreal.MaterialExpressionVectorParameter,-500,-250,parameter_name='BaseColor',default_value=unreal.LinearColor(.17,.21,.24),group='Environment')
    tint=node(mat,unreal.MaterialExpressionVectorParameter,-500,-80,parameter_name='IceTint',default_value=unreal.LinearColor(.055,.25,.32),group='Environment')
    normal=node(mat,unreal.MaterialExpressionVertexNormalWS,-500,80)
    link(color,call,'BaseColor');link(tint,call,'IceTint');link(normal,call,'WorldNormal')
    for i,(name,value) in enumerate([('Roughness',.75),('Exposure',1),('Override',-1),('FullThicknessMm',5),('CellSizeCm',180),('FrostAmount',.4)]):
        link(parameter(mat,name,value,-500,250+i*140),call,name)
    MEL.connect_material_property(call,'BaseColor',unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(call,'Roughness',unreal.MaterialProperty.MP_ROUGHNESS)
    # 매우 약한 표면 굴곡. 월드 노멀을 사용해 벽/비스듬한 바위에서도 노멀이 뒤집히지 않아요.
    world=node(mat,unreal.MaterialExpressionWorldPosition,200,400)
    n=ext.custom(mat,'float3 n=normalize(N); float up=smoothstep(.45,.9,n.z); return float4(normalize(n+float3(cos(P.x*.025)*.04,sin(P.y*.031)*.04,0)*up),0);',{'N':normal,'P':world},450,400)
    rgb=node(mat,unreal.MaterialExpressionComponentMask,700,400,r=True,g=True,b=True,a=False);link(n,rgb)
    MEL.connect_material_property(rgb,'',unreal.MaterialProperty.MP_NORMAL)
    spec=parameter(mat,'Specular',.65,500,650);MEL.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
    MEL.recompile_material(mat);save(mat)
    return mat


def foam_material(mpc):
    path=ROOT+'/Materials/M_Environment_ShoreFoam'
    if LIB.does_asset_exist(path):return LIB.load_asset(path)
    mat=TOOLS.create_asset('M_Environment_ShoreFoam',ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT);mat.set_editor_property('two_sided',True)
    mat.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    uv=node(mat,unreal.MaterialExpressionTextureCoordinate,-900,0)
    age=node(mat,unreal.MaterialExpressionParticleRelativeTime,-900,180)
    wave=node(mat,unreal.MaterialExpressionCollectionParameter,-900,360,collection=mpc,parameter_name='WaveHeightM')
    tint=node(mat,unreal.MaterialExpressionVectorParameter,-900,540,parameter_name='FoamTint',default_value=unreal.LinearColor(.62,.86,.89),group='Environment')
    opacity=parameter(mat,'FoamOpacity',.8,-900,700)
    fx=ext.custom(mat,(SHADERS/'EnvironmentFoam.ush').read_text(encoding='utf8'),{'UV':uv,'Age':age,'Wave':wave,'Tint':tint,'Opacity':opacity},-450,0)
    rgb=node(mat,unreal.MaterialExpressionComponentMask,-150,0,r=True,g=True,b=True,a=False);link(fx,rgb)
    alpha=node(mat,unreal.MaterialExpressionComponentMask,-150,180,r=False,g=False,b=False,a=True);link(fx,alpha)
    pc=node(mat,unreal.MaterialExpressionParticleColor,-400,700)
    a=node(mat,unreal.MaterialExpressionMultiply,100,180);link(alpha,a,'A');link(pc,a,'B','A')
    MEL.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=parameter(mat,'FoamRoughness',.75,100,450)
    MEL.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(a,'',unreal.MaterialProperty.MP_OPACITY)
    tide=node(mat,unreal.MaterialExpressionCollectionParameter,-600,950,collection=mpc,parameter_name='TideLevelM')
    time=node(mat,unreal.MaterialExpressionTime,-900,950)
    offset=ext.custom(mat,'return float4(0,0,Tide*100+sin(Time*1.8)*min(Wave*6,4),0);',{'Tide':tide,'Time':time,'Wave':wave},-250,950)
    xyz=node(mat,unreal.MaterialExpressionComponentMask,50,950,r=True,g=True,b=True,a=False);link(offset,xyz)
    MEL.connect_material_property(xyz,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    # 먼저 그래프/WPO 캐시를 갱신해야 UE 5.8 사용 플래그의 셰이더 검사와 일치해요.
    MEL.recompile_material(mat)
    MEL.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NIAGARA_SPRITES)
    MEL.recompile_material(mat);save(mat)
    return mat


def shore_foam(mat):
    path=ROOT+'/NS_Environment_ShoreFoam'
    if LIB.does_asset_exist(path):return LIB.load_asset(path)
    system=LIB.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/FountainLightweight',path)
    water=base.water
    for i,(name,count,size,life,speed,alpha) in enumerate([
        ('FoamLace',12,(140,240),(2.8,4.8),18,1),
        ('SmallBubbles',18,(30,70),(1.5,2.8),28,.85)]):
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
        renderer=unreal.find_object(None,emitter.get_path_name()+'.Renderer');setp(renderer,'FacingMode','CustomFacingVector')
        setp(emitter,'FixedBounds','(Min=(X=-1200,Y=-500,Z=-1000),Max=(X=1200,Y=500,Z=1000),IsValid=1)')
    assert water.EDIT.finish_water_system(system),'해안 거품 Niagara 컴파일 실패'
    save(system)
    return system


def main():
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT,'/Niagara/DefaultAssets/Templates/Systems'],force_rescan=True)
    mpc=collection();fn=ice_function(mpc);mat=ice_material(fn);foam=shore_foam(foam_material(mpc))
    for name,frost,tint in [('MI_Environment_ClearIce',.15,(.055,.22,.29)),('MI_Environment_Frost',.9,(.14,.35,.42))]:
        path=ROOT+'/Materials/'+name
        if LIB.does_asset_exist(path):continue
        inst=TOOLS.create_asset(name,ROOT+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(inst,mat);MEL.set_material_instance_scalar_parameter_value(inst,'FrostAmount',frost)
        MEL.set_material_instance_vector_parameter_value(inst,'IceTint',unreal.LinearColor(*tint));save(inst)
    path=ROOT+'/BP_EnvironmentShoreFoam'
    if not LIB.does_asset_exist(path):
        bp=base.blueprint('BP_EnvironmentShoreFoam',unreal.NiagaraActor)
        cdo=unreal.get_default_object(bp.generated_class());cdo.get_component_by_class(unreal.NiagaraComponent).set_asset(foam)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    report={'ice':mat.get_path_name(),'function':fn.get_path_name(),'foam':foam.get_path_name(),'niagara_ready':True}
    (Path(unreal.Paths.project_saved_dir())/'ice-shore-authored.json').write_text(json.dumps(report,indent=2),encoding='utf8')


if __name__=='__main__':main()
