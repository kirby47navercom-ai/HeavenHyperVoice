"""Author two standalone editable Water Gun Niagara systems; no gameplay or server wiring."""
import unreal,json
from pathlib import Path
ROOT='/Game/VFX/Pokemon/WaterGun'
LIB=unreal.EditorAssetLibrary
MEL=unreal.MaterialEditingLibrary
EDIT=unreal.UEWaterVFXEditorLibrary

def setp(obj,name,value):
    if not EDIT.set_water_property(obj,name,str(value)):
        raise RuntimeError(f'Cannot set {obj.get_path_name()}.{name}: {value}')

def scalar(a,b=None):
    b=a if b is None else b
    return f'(Min={a},Max={b},Mode={"UniformConstant" if a==b else "UniformRange"},ChannelConstantsAndRanges=({a}'+(f',{b}' if a!=b else '')+'))'

def vector(values):
    v='('+','.join(f'{axis}={x}' for axis,x in zip('XYZ',values))+')'
    return f'(Min={v},Max={v},Mode=NonUniformConstant,ChannelConstantsAndRanges=('+','.join(map(str,values))+'))'

def make_material(name,shape):
    path=ROOT+'/Materials/'+name
    if LIB.does_asset_exist(path):return LIB.load_asset(path)
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided',True)
    def node(cls,x,y,**props):
        n=MEL.create_material_expression(mat,cls,x,y)
        for k,v in props.items():n.set_editor_property(k,v)
        return n
    uv=node(unreal.MaterialExpressionTextureCoordinate,-900,0)
    age=node(unreal.MaterialExpressionParticleRelativeTime,-900,180)
    pc=node(unreal.MaterialExpressionParticleColor,-900,360)
    fx=node(unreal.MaterialExpressionCustom,-600,0,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs=[]
    for name in ['UV','Age']:
        i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
    fx.set_editor_property('inputs',inputs)
    formulas={
      'drop': 'float r=length(p*float2(1.13,0.88)); float mask=1-smoothstep(0.76,0.86,r); float edge=smoothstep(0.53,0.80,r); float shine=exp(-length((p-float2(-0.25,-0.3))*float2(5,7))); float light=saturate(edge*0.7+shine*1.7);',
      'sheet': 'float a=atan2(p.y,p.x); float r=length(p); float wave=0.055*sin(a*7+Age*14)+0.03*sin(a*13-Age*22); float ring=abs(r-(0.60+wave)); float gaps=smoothstep(-0.65,-0.15,sin(a*3+Age*8)); float mask=(1-smoothstep(0.11,0.18,ring))*gaps; float light=smoothstep(0.025,0.12,ring);',
      'splash': 'float a=atan2(p.y,p.x); float r=length(p); float spikes=0.54+0.15*sin(a*9)+0.09*sin(a*17+1); float mask=(1-smoothstep(spikes-0.07,spikes+0.04,r))*(smoothstep(0.05,0.24,r)+0.3); float light=smoothstep(spikes-0.22,spikes,r);',
      'ring': 'float r=length(p); float radius=lerp(0.12,0.91,Age); float d=abs(r-radius); float mask=1-smoothstep(0.025,0.075,d); float light=0.85;'
    }
    fx.set_editor_property('code','float2 p=UV*2-1; '+formulas[shape]+' float3 col=lerp(float3(0.09,0.55,0.9),float3(0.85,0.98,1),light); return float4(col,mask*saturate((1-Age)*4));')
    MEL.connect_material_expressions(uv,'',fx,'UV');MEL.connect_material_expressions(age,'',fx,'Age')
    rgb=node(unreal.MaterialExpressionComponentMask,-300,0,r=True,g=True,b=True,a=False)
    alpha=node(unreal.MaterialExpressionComponentMask,-300,180,r=False,g=False,b=False,a=True)
    MEL.connect_material_expressions(fx,'',rgb,'');MEL.connect_material_expressions(fx,'',alpha,'')
    tint=node(unreal.MaterialExpressionMultiply,-80,0)
    MEL.connect_material_expressions(rgb,'',tint,'A');MEL.connect_material_expressions(pc,'RGB',tint,'B')
    opacity=node(unreal.MaterialExpressionMultiply,-80,180)
    MEL.connect_material_expressions(alpha,'',opacity,'A');MEL.connect_material_expressions(pc,'A',opacity,'B')
    gain=node(unreal.MaterialExpressionScalarParameter,-80,-150,parameter_name='Brightness',default_value=1.25)
    final=node(unreal.MaterialExpressionMultiply,150,0)
    MEL.connect_material_expressions(tint,'',final,'A');MEL.connect_material_expressions(gain,'',final,'B')
    MEL.connect_material_property(final,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    MEL.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NIAGARA_SPRITES)
    MEL.recompile_material(mat)
    LIB.save_loaded_asset(mat,only_if_is_dirty=False)
    return mat

def configure_layer(system,name,duplicate,mat,burst,count,life,size,radius,speed,angle=28,gravity=0,alpha=1):
    emitter=EDIT.water_layer(system,name,duplicate)
    if not emitter:raise RuntimeError('Missing lightweight emitter')
    mods={m.get_class().get_name().replace('NiagaraStatelessModule_',''):m for m in emitter.get_editor_property('modules')}
    enabled={'InitializeParticle','ShapeLocation','AddVelocity','SolveVelocitiesAndForces','ScaleColor','ApplyOwnerScaleToAttributes'}
    if gravity:enabled.add('GravityForce')
    for key,m in mods.items():
        setp(m,'bModuleEnabled','True' if key in enabled else 'False')
    init=mods['InitializeParticle']
    setp(init,'LifetimeDistribution',scalar(*life))
    a,b=size
    setp(init,'SpriteSizeDistribution',f'(Min=(X={a},Y={a}),Max=(X={b},Y={b}),Mode=UniformRange,ChannelConstantsAndRanges=({a},{b}))')
    setp(init,'SpriteRotationDistribution',scalar(0,360))
    setp(init,'ColorDistribution',f'(Mode=NonUniformConstant,Values=((R=1,G=1,B=1,A={alpha})),ChannelConstantsAndRanges=(1,1,1,{alpha}))')
    shape=mods['ShapeLocation']
    setp(shape,'ShapePrimitive','Sphere');setp(shape,'SphereRadius',scalar(radius))
    velocity=mods['AddVelocity']
    setp(velocity,'VelocityType','FromPoint' if burst else 'InCone')
    if burst:setp(velocity,'PointVelocityDistribution',scalar(*speed))
    else:
        setp(velocity,'ConeRotationType','Direction')
        setp(velocity,'ConeDirection',vector((-1,0,0)))
        setp(velocity,'ConeAngle',angle)
        setp(velocity,'ConeVelocityDistribution',scalar(*speed))
    if not speed[1]:setp(velocity,'bModuleEnabled','False')
    if gravity:setp(mods['GravityForce'],'GravityDistribution',vector((0,0,gravity)))
    # Preserve the template's smooth alpha fade. All layers are editable Niagara stack modules.
    duration=0.8 if burst else 2
    setp(emitter,'EmitterState',f'(LoopBehavior={"Once" if burst else "Infinite"},LoopDuration={scalar(duration)},InactiveResponse=Complete)')
    spawn=f'(Type=Burst,SpawnTime=0,Amount=(Min={count},Max={count}))' if burst else f'(Type=Rate,Rate={scalar(count)})'
    setp(emitter,'SpawnInfos','('+spawn+')')
    setp(emitter,'FixedBounds','(Min=(X=-400,Y=-400,Z=-450),Max=(X=400,Y=400,Z=450),IsValid=1)')
    renderer=unreal.find_object(None,emitter.get_path_name()+'.Renderer')
    if not renderer:raise RuntimeError('Missing sprite renderer')
    renderer.set_editor_property('material',mat)
    return name

def main():
    materials={k:make_material('M_WaterGun_'+k.title(),k) for k in ['drop','sheet','splash','ring']}
    specs={
        'NS_WaterGun_Projectile':[
            ('WaterCore','drop',110,(.22,.34),(28,42),8,(240,340),12,0,.88),
            ('SpiralSheets','sheet',55,(.28,.42),(40,62),12,(230,350),22,0,1),
            ('FineSpray','drop',130,(.24,.40),(4,11),15,(170,300),50,-50,.9),
            ('LeadingWater','drop',40,(.18,.26),(32,45),8,(0,0),0,0,.75)],
        'NS_WaterGun_Impact':[
            ('SplashPetals','sheet',14,(.26,.46),(42,72),5,(180,330),0,-380,1),
            ('ImpactFlash','splash',1,(.17,.17),(120,120),0,(0,0),0,0,1),
            ('RadialDroplets','drop',48,(.35,.65),(5,15),10,(180,410),0,-620,1),
            ('ExpandingRing','ring',1,(.28,.28),(180,180),0,(0,0),0,0,.8)]}
    report=[]
    for name,layers in specs.items():
        path=ROOT+'/'+name
        # Reruns preserve artists' finished systems; remove a failed draft explicitly before retrying.
        if LIB.does_asset_exist(path):raise RuntimeError('Asset exists: '+path)
        system=LIB.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/FountainLightweight',path)
        for i,(label,kind,count,life,size,radius,speed,angle,gravity,alpha) in enumerate(layers):
            configure_layer(system,label,i>0,materials[kind],name.endswith('Impact'),count,life,size,radius,speed,angle,gravity,alpha)
        if not EDIT.finish_water_system(system):raise RuntimeError('Niagara compile failed: '+path)
        if not LIB.save_loaded_asset(system,only_if_is_dirty=False):raise RuntimeError('Save failed: '+path)
        report.append({'system':path,'layers':[x[0] for x in layers],'ready':True,'stationary_origin':True})
    out=Path(unreal.Paths.project_saved_dir())/'Codex/WaterGun'
    (out/'authored.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    unreal.log('WATER GUN VFX AUTHORED')

if __name__=='__main__':main()
