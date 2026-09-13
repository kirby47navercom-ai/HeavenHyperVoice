"""Author an animated 3D vine whip and a separate Niagara impact. No combat logic."""
import json
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import create_water_gun_vfx as water

ROOT = '/Game/VFX/Pokemon/VineWhip'
LIB = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
OUT = Path(unreal.Paths.project_saved_dir())/'Codex/VineWhip'

# One continuous tapered tube. The root stays at zero and the tip reaches +X at age .58.
BEND = r'''
struct VineMotion {
 float3 center(float u, float age, float reach) {
  float a=u*7.54;
  float3 coil=float3(.15*u+.14*sin(a),.035*sin(3.14159*u),.19*(1-cos(a)));
  float3 raised=float3(-.08*u+.32*sin(3.14159*u),.04*sin(3.14159*u),.74*u);
  float3 strike=float3(u,.025*sin(3.14159*u),.13*sin(3.14159*u));
  float lift=smoothstep(0,.30,age);
  float snap=pow(saturate((age-.30)/.28),3);
  float3 c=lerp(lerp(coil,raised,lift),strike,snap);
  float retract=smoothstep(.62,1,age);
  c*=1-retract;
  c.z+=.26*sin(3.14159*u)*sin(retract*3.14159);
  return c*reach;
 }
};
VineMotion motion;
float u=saturate(Pos.x/400.0);
float t=saturate(Age);
float3 center=motion.center(u,t,Reach);
float3 tangent=normalize(motion.center(min(u+.002,1),t,Reach)-motion.center(max(u-.002,0),t,Reach)+float3(.00001,0,0));
float3 side=normalize(cross(float3(0,1,0),tangent));
float3 up=cross(tangent,side);
float growth=smoothstep(0,.04,t)*(1-smoothstep(.93,1,t));
float taper=lerp(1,.075,pow(u,1.25))*Thickness*growth;
float3 bent=center+(Pos.y*up+Pos.z*side)*taper;
return bent-Pos;
'''


def node(mat, cls, x, y, **props):
    n=MEL.create_material_expression(mat,cls,x,y)
    for k,v in props.items(): n.set_editor_property(k,v)
    return n


def vine_material():
    path=ROOT+'/Materials/M_VineWhip_Vine'
    if LIB.does_asset_exist(path): return LIB.load_asset(path)
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_VineWhip_Vine',ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided',True)
    pos=node(mat,unreal.MaterialExpressionPreSkinnedPosition,-900,0)
    age=node(mat,unreal.MaterialExpressionParticleColor,-900,180)
    reach=node(mat,unreal.MaterialExpressionScalarParameter,-900,330,parameter_name='Reach',default_value=400)
    thickness=node(mat,unreal.MaterialExpressionScalarParameter,-900,470,parameter_name='Thickness',default_value=2.6)
    bend=node(mat,unreal.MaterialExpressionCustom,-500,0,code=BEND,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs=[]
    for name in ['Pos','Age','Reach','Thickness']:
        i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
    bend.set_editor_property('inputs',inputs)
    for src,name in [(pos,'Pos'),(age,'Age'),(reach,'Reach'),(thickness,'Thickness')]:
        MEL.connect_material_expressions(src,'R' if name=='Age' else '',bend,name)
    transform=node(mat,unreal.MaterialExpressionTransform,-150,0,
        transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
        transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    MEL.connect_material_expressions(bend,'',transform,'')
    MEL.connect_material_property(transform,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    uv=node(mat,unreal.MaterialExpressionTextureCoordinate,-900,640)
    color=node(mat,unreal.MaterialExpressionCustom,-450,600,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        code='float v=0.5+0.5*sin(Pos.x*6.283185); float ridge=pow(saturate(v),3); return lerp(float3(.035,.30,.008),float3(.40,.86,.055),ridge);')
    inp=unreal.CustomInput();inp.set_editor_property('input_name','Pos');color.set_editor_property('inputs',[inp])
    MEL.connect_material_expressions(uv,'',color,'Pos')
    MEL.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES)
    MEL.recompile_material(mat)
    LIB.save_loaded_asset(mat,only_if_is_dirty=False)
    return mat


def vine_mesh(mat):
    path=ROOT+'/Meshes/SM_VineWhip_Segmented'
    if LIB.does_asset_exist(path): return LIB.load_asset(path)
    dynamic=unreal.DynamicMesh()
    unreal.GeometryScript_Primitives.append_cylinder(dynamic,unreal.GeometryScriptPrimitiveOptions(),
        unreal.Transform(rotation=unreal.Rotator(pitch=-90,yaw=0,roll=0)),radius=1,height=400,
        radial_steps=10,height_steps=96,capped=True)
    options=unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property('enable_nanite',False)
    options.set_editor_property('enable_collision',False)
    result=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dynamic,path,options)
    mesh=result[0]
    if not mesh: raise RuntimeError('Vine mesh creation failed')
    mesh.set_material(0,mat)
    # WPO can curl behind the root and rise above the unbent cylinder.
    mesh.set_editor_property('positive_bounds_extension',unreal.Vector(150,450,450))
    mesh.set_editor_property('negative_bounds_extension',unreal.Vector(450,450,450))
    LIB.save_loaded_asset(mesh,only_if_is_dirty=False)
    return mesh


def sprite_material(kind):
    path=ROOT+'/Materials/M_VineWhip_'+kind
    if LIB.does_asset_exist(path): return LIB.load_asset(path)
    previous=water.ROOT;water.ROOT=ROOT
    try: mat=water.make_material('M_VineWhip_'+kind,'drop')
    finally: water.ROOT=previous
    custom=unreal.find_object(None,mat.get_path_name()+':MaterialExpressionCustom_0')
    shapes={
        'Leaf': '''float x=p.x; float y=p.y;
float width=.40*sin(saturate((x+.86)/1.72)*3.14159);
float mask=(1-smoothstep(width-.035,width+.035,abs(y+.12*sin(x*3))))*(1-smoothstep(.80,.88,abs(x)));
float vein=1-smoothstep(.015,.045,abs(y+.12*sin(x*3)));
float3 col=lerp(float3(.12,.42,.012),float3(.62,.91,.13),saturate(.42+vein*.5-p.y));''',
        'Flash': '''float a=atan2(p.y,p.x); float r=length(p);
float edge=.52+.16*sin(a*8)+.07*sin(a*13);
float mask=1-smoothstep(edge-.06,edge+.035,r);
float3 col=lerp(float3(.43,.9,.03),float3(.94,1,.60),saturate(1-r*2));''',
        'Slash': '''float d=length(p*float2(.8,3.8));
float mask=1-smoothstep(.55,.85,d);
float3 col=lerp(float3(.30,.66,.025),float3(.93,1,.57),1-saturate(d));''',
    }
    custom.set_editor_property('code','float2 p=UV*2-1; '+shapes[kind]+' return float4(col,mask*saturate((1-Age)*3));')
    gain=unreal.find_object(None,mat.get_path_name()+':MaterialExpressionScalarParameter_0')
    gain.set_editor_property('default_value',1.0 if kind=='Leaf' else 1.5)
    MEL.recompile_material(mat);LIB.save_loaded_asset(mat,only_if_is_dirty=False)
    return mat


def save_system(system):
    if not water.EDIT.finish_water_system(system): raise RuntimeError('Niagara compile failed: '+system.get_name())
    if not LIB.save_loaded_asset(system,only_if_is_dirty=False): raise RuntimeError('Niagara save failed')


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    for suffix in ['Attack','Impact']:
        if LIB.does_asset_exist(ROOT+'/NS_VineWhip_'+suffix):
            raise RuntimeError('Existing finished systems retained; edit them directly.')
    mats={k:sprite_material(k) for k in ['Leaf','Flash','Slash']}
    vine=vine_material();mesh=vine_mesh(vine)
    attack=LIB.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/FountainLightweight',ROOT+'/NS_VineWhip_Attack')
    water.configure_layer(attack,'BendingVine',False,mats['Leaf'],True,1,(1.1,1.1),(1,1),0,(0,0))
    emitter=water.EDIT.water_layer(attack,'BendingVine',False)
    for m in emitter.get_editor_property('modules'):
        key=m.get_class().get_name().replace('NiagaraStatelessModule_','')
        water.setp(m,'bModuleEnabled','True' if key in ['InitializeParticle','ScaleColor'] else 'False')
        if key=='ScaleColor':
            # The red channel carries normalized animation age to the mesh vertex shader.
            water.setp(m,'ScaleDistribution','(Mode=NonUniformCurve,Values=((R=0,G=1,B=1,A=1),(R=1,G=1,B=1,A=1)),ChannelConstantsAndRanges=,ChannelCurves=((Keys=((Time=0,Value=0),(Time=1,Value=1))),(Keys=((Value=1))),(Keys=((Value=1))),(Keys=((Value=1)))))')
        if key=='InitializeParticle':
            water.setp(m,'MeshScaleDistribution',water.vector((1,1,1)))
    water.setp(emitter,'EmitterState','(LoopBehavior=Once,LoopDuration='+water.scalar(1.15)+',InactiveResponse=Complete)')
    water.setp(emitter,'FixedBounds','(Min=(X=-500,Y=-500,Z=-500),Max=(X=650,Y=500,Z=650),IsValid=1)')
    renderer=unreal.NiagaraMeshRendererProperties(outer=emitter,name='VineMeshRenderer')
    mesh_info=unreal.NiagaraMeshRendererMeshProperties();mesh_info.set_editor_property('mesh',mesh)
    renderer.set_editor_property('meshes',[mesh_info])
    water.setp(emitter,'RendererProperties',"(NiagaraMeshRendererProperties'"+renderer.get_path_name()+"')")
    save_system(attack)
    impact=LIB.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/FountainLightweight',ROOT+'/NS_VineWhip_Impact')
    layers=[
        ('ContactFlash','Flash',1,(.14,.14),(72,72),0,(0,0),0,0,1),
        ('CuttingStreaks','Slash',12,(.12,.24),(16,29),4,(140,240),0,0,1),
        ('LeafFragments','Leaf',18,(.38,.72),(10,22),8,(100,220),0,-140,1),
    ]
    for i,(label,kind,count,life,size,radius,speed,angle,gravity,alpha) in enumerate(layers):
        water.configure_layer(impact,label,i>0,mats[kind],True,count,life,size,radius,speed,angle,gravity,alpha)
    save_system(impact)
    for suffix,system in [('Attack',attack),('Impact',impact)]:
        factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.NiagaraActor)
        bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_VineWhip_'+suffix,ROOT,unreal.Blueprint,factory)
        defaults=unreal.get_default_object(bp.generated_class())
        defaults.get_component_by_class(unreal.NiagaraComponent).set_asset(system)
        defaults.set_editor_property('initial_life_span',1.5)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        if not LIB.save_loaded_asset(bp,only_if_is_dirty=False): raise RuntimeError('Blueprint save failed')
    (OUT/'authored.json').write_text(json.dumps({'attack':attack.get_path_name(),'impact':impact.get_path_name(),'ready':True,'duration':1.1,'contact_time':.638},indent=2))
    print('VINE WHIP VFX AUTHORED')


if __name__=='__main__': main()
