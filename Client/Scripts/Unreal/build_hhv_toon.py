"""Author and apply editable per-surface Toon profiles. Never runs PIE.

Run once to create masters/profiles, then with --apply to connect existing assets.
Existing graphs, textures, opacity masks, animation UVs and customization parameters are retained.
"""
import unreal
import json
import sys
import shutil
from pathlib import Path
from collections import Counter

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT/'Saved/Codex/ToonBuild'
OUT.mkdir(parents=True,exist_ok=True)
LIB = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
FOLDER = '/Game/Toon/HHV'
FBX = '/InterchangeAssets/Materials/FBXLegacyPhongSurfaceMaterial.FBXLegacyPhongSurfaceMaterial'
CHAR = '/Game/CharacterCustomization/Palworld/Materials/M_PalworldCharacterMaster.M_PalworldCharacterMaster'
CITY = '/Game/Environments/Goldenrod_R03'
# shadow RGB, mid RGB, transition intervals, roughness, specular scale, normal strength, rim, anisotropy
STYLES = {
 'Face': ((.56,.44,.46),(.86,.77,.75),(.22,.39,.62,.79),.88,.12,.15,.015,0),
 'Skin': ((.46,.37,.41),(.80,.70,.70),(.25,.37,.64,.76),.82,.18,.35,.02,0),
 'Hair': ((.23,.25,.34),(.66,.67,.77),(.28,.32,.64,.70),.38,.65,.30,.055,.70),
 'Eyes': ((.76,.78,.84),(.90,.92,.96),(.15,.35,.65,.82),.24,.75,.20,.005,0),
 'Cloth': ((.30,.33,.43),(.70,.73,.81),(.25,.33,.65,.74),.86,.20,.45,.025,0),
 'Pokemon': ((.36,.39,.47),(.73,.76,.83),(.25,.32,.63,.72),.70,.35,.50,.035,0),
 'Metal': ((.25,.30,.40),(.63,.70,.82),(.22,.31,.60,.72),.30,.90,.70,.04,0),
 'Foliage': ((.36,.44,.42),(.72,.80,.72),(.20,.40,.58,.82),.90,.15,.40,0,0),
 'Environment': ((.48,.53,.62),(.78,.82,.89),(.17,.40,.58,.84),.78,.45,.45,0,0),
 'Water': ((.60,.72,.80),(.84,.91,.96),(.12,.42,.56,.88),.30,.75,1,0,0),
 'Emission': ((.80,.82,.88),(.94,.96,1),(.14,.35,.60,.80),.65,.25,.25,0,0),
}


def save(asset):
    if not LIB.save_loaded_asset(asset,only_if_is_dirty=False):
        raise RuntimeError('Save failed: '+asset.get_path_name())


def backup(asset):
    package = asset.get_path_name().split('.')[0]
    if not package.startswith('/Game/'): return
    extension = '.umap' if isinstance(asset,unreal.World) else '.uasset'
    src = ROOT/'Content'/(package.removeprefix('/Game/')+extension)
    dst = OUT/'Before'/(package.removeprefix('/Game/')+extension)
    if src.exists() and not dst.exists():
        dst.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(src,dst)


def duplicate(src, dst):
    return LIB.load_asset(dst) if LIB.does_asset_exist(dst) else LIB.duplicate_asset(src,dst)


def curve(points):
    channels=[]
    for channel in range(3):
        keys=','.join(f'(InterpMode=RCIM_Linear,Time={time:.5f},Value={value[channel]:.5f})' for time,value in points)
        channels.append(f'ColorCurves[{channel}]=(Keys=({keys}))')
    return '('+','.join(channels)+')'


def create_profile(name):
    path=FOLDER+'/Profiles/TP_HHV_'+name
    if LIB.does_asset_exist(path): return LIB.load_asset(path)
    asset=duplicate('/Game/Toon/TP_Test',path)
    shadow,mid,edges,*_=STYLES[name]
    a,b,c,d=edges
    diffuse=curve([(0,shadow),(a,shadow),(b,mid),(c,mid),(d,(1,1,1)),(1,(1,1,1))])
    if name in ('Environment','Foliage','Water'):
        spec=curve([(0,(0,0,0)),(.45,(.16,.16,.16)),(.8,(.65,.65,.65)),(1,(1,1,1))])
    elif name in ('Eyes','Hair','Metal'):
        spec=curve([(0,(0,0,0)),(.60,(0,0,0)),(.66,(.40,.40,.40)),(.83,(.40,.40,.40)),(.87,(1,1,1)),(1,(1,1,1))])
    else:
        spec=curve([(0,(0,0,0)),(.72,(0,0,0)),(.82,(.65,.65,.65)),(1,(1,1,1))])
    settings=asset.get_editor_property('settings')
    settings.import_text(f'(DiffuseRamp={diffuse},SpecularRamp={spec},DiffuseRampOffsetStrength=0,SpecularRampOffsetStrength=0,'
                        f'ShadowHatchingPatternStrength=0,DiffuseIndirectScale=0.85,SpecularIndirectScale={.65 if name in ("Water","Metal") else .3},bDiffuseRampIncludeShadow=False)')
    asset.set_editor_property('settings',settings)
    save(asset)
    return asset


def configure(instance, name, profiles):
    unreal.UEToonEditorLibrary.set_instance_profile(instance,profiles[name])
    _,_,_,rough,spec,normal,rim,anisotropy=STYLES[name]
    for key,value in {'ToonRoughness':rough,'ToonRoughnessOverride':.55 if name in ('Environment','Metal') else 1.,
                       'ToonSpecularScale':spec,'ToonNormalStrength':normal,'ToonRimStrength':rim,
                       'ToonAnisotropy':anisotropy,'ToonEmissionStrength':.35 if name=='Emission' else .015 if name=='Eyes' else 0.}.items():
        MEL.set_material_instance_scalar_parameter_value(instance,key,value)


def create_masters(profiles):
    masters={}
    for key,src,default in [('Imported',FBX,'Pokemon'),('Character',CHAR,'Skin')]:
        path=FOLDER+'/Masters/M_HHV_Toon'+key
        asset=duplicate(src,path)
        if not unreal.UEToonEditorLibrary.convert_legacy_material(asset,profiles[default]):
            raise RuntimeError('Unsupported source graph: '+src)
        MEL.set_base_material_usage(asset,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH,True)
        MEL.set_base_material_usage(asset,unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS,True)
        save(asset)
        masters[key]=asset
    for name in STYLES:
        path=FOLDER+'/Presets/MI_HHV_'+name
        if LIB.does_asset_exist(path): continue
        factory=unreal.MaterialInstanceConstantFactoryNew()
        instance=unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_HHV_'+name,FOLDER+'/Presets',unreal.MaterialInstanceConstant,factory)
        MEL.set_material_instance_parent(instance,masters['Character'])
        MEL.set_material_instance_texture_parameter_value(instance,'Base Texture',LIB.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
        configure(instance,name,profiles)
        save(instance)
    return masters


def make_outline():
    path=FOLDER+'/Masters/M_HHV_Outline'
    if LIB.does_asset_exist(path): return LIB.load_asset(path)
    material=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_HHV_Outline',FOLDER+'/Masters',unreal.Material,unreal.MaterialFactoryNew())
    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property('two_sided',True)
    material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    def node(cls,x,y,**props):
        obj=MEL.create_material_expression(material,cls,x,y)
        for k,v in props.items():obj.set_editor_property(k,v)
        return obj
    def connect(a,b,pin):
        if not MEL.connect_material_expressions(a,'',b,pin):raise RuntimeError('Outline connection '+pin)
    color=node(unreal.MaterialExpressionVectorParameter,-250,-200,parameter_name='OutlineColor',default_value=unreal.LinearColor(.028,.035,.055,1))
    width=node(unreal.MaterialExpressionScalarParameter,-900,100,parameter_name='OutlineWidthCM',default_value=.18)
    camera=node(unreal.MaterialExpressionCameraPositionWS,-1500,260)
    position=node(unreal.MaterialExpressionWorldPosition,-1500,430)
    distance=node(unreal.MaterialExpressionDistance,-1300,320)
    connect(camera,distance,'A');connect(position,distance,'B')
    fade=node(unreal.MaterialExpressionSubtract,-900,320,const_a=2200.)
    connect(distance,fade,'B')
    divide=node(unreal.MaterialExpressionDivide,-700,320,const_b=1400.)
    connect(fade,divide,'A')
    clamp=node(unreal.MaterialExpressionClamp,-500,320)
    connect(divide,clamp,'')
    faded_width=node(unreal.MaterialExpressionMultiply,-300,250)
    connect(width,faded_width,'A');connect(clamp,faded_width,'B')
    # Reduce shell thickness to zero with distance rather than abruptly clipping an opacity fade.
    normal=node(unreal.MaterialExpressionVertexNormalWS,-900,0)
    expand=node(unreal.MaterialExpressionMultiply,-650,0)
    connect(normal,expand,'A');connect(faded_width,expand,'B')
    sign=node(unreal.MaterialExpressionTwoSidedSign,-500,520)
    back=node(unreal.MaterialExpressionOneMinus,-250,520)
    connect(sign,back,'')
    # This backface shell is a mesh overlay, not a fullscreen effect.
    MEL.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(expand,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    MEL.connect_material_property(back,'',unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.set_base_material_usage(material,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH,True)
    MEL.set_base_material_usage(material,unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS,True)
    MEL.recompile_material(material)
    save(material)
    return material


def character_style(path):
    name=path.rsplit('/',1)[-1].lower()
    if 'eye' in name or 'iris' in name: return 'Eyes'
    if any(t in name for t in ('hair','beard','brow','mustache')): return 'Hair'
    if any(t in name for t in ('equip','outfit','armor','helmet')): return 'Cloth'
    if 'head' in name and 'outfit' not in name: return 'Face'
    if 'body' in name or 'skin' in name: return 'Skin'
    return 'Cloth'


def apply_all(masters,profiles,outline):
    report={'characters':Counter(),'pokemon':Counter(),'environment':Counter(),'outlines':0,'skipped':[]}
    def progress():
        (OUT/'progress.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    progress()
    # Direct imported and original character instances retain their texture/color overrides.
    for path in LIB.list_assets('/Game/CharacterCustomization/Palworld',recursive=True,include_folder=False):
        data=LIB.find_asset_data(path)
        if str(data.asset_class_path.asset_name)!='MaterialInstanceConstant':continue
        obj=LIB.load_asset(path)
        parent=obj.get_editor_property('parent')
        parent_path=parent.get_path_name() if parent else ''
        target=masters['Imported'] if parent_path in (FBX,masters['Imported'].get_path_name()) else masters['Character'] if parent_path in (CHAR,masters['Character'].get_path_name()) else None
        if not target:continue  # Morph-safe children inherit; archived glTF Substrate graphs stay intact.
        category=character_style(path)
        if '--resume' in sys.argv and parent==target and unreal.UEToonEditorLibrary.get_instance_profile(obj)==profiles[category]:
            report['characters'][category]+=1
            continue
        backup(obj)
        MEL.set_material_instance_parent(obj,target)
        configure(obj,category,profiles)
        save(obj)
        report['characters'][category]+=1
        if sum(report['characters'].values())%40==0:progress()
    pokemon_materials={}
    pokemon_meshes={}
    for path in LIB.list_assets('/Game/Pokemon/SpeciesData',recursive=True,include_folder=False):
        if not path.rsplit('/',1)[-1].startswith('DA_'):continue
        species=LIB.load_asset(path)
        if not isinstance(species,unreal.UEPokemonSpeciesData):continue
        mesh=species.get_editor_property('skeletal_mesh')
        if not mesh:continue
        pokemon_meshes[mesh.get_path_name()]=mesh
        for slot in mesh.get_editor_property('materials'):
            mat=slot.material_interface
            if not isinstance(mat,unreal.MaterialInstanceConstant):continue
            name=str(slot.material_slot_name).lower()
            category='Eyes' if 'eye' in name else 'Emission' if any(s in name for s in ('fire','flame','emissive')) else 'Metal' if species.get_name() in ('DA_자망칼','DA_디아루가') else 'Pokemon'
            pokemon_materials[mat.get_path_name()]=(mat,category)
    for mat,category in pokemon_materials.values():
        parent=mat.get_editor_property('parent')
        if not parent or parent.get_path_name() not in (FBX,masters['Imported'].get_path_name()):
            report['skipped'].append(mat.get_path_name());continue
        if '--resume' in sys.argv and parent==masters['Imported'] and unreal.UEToonEditorLibrary.get_instance_profile(mat)==profiles[category]:
            report['pokemon'][category]+=1
            continue
        backup(mat)
        MEL.set_material_instance_parent(mat,masters['Imported'])
        configure(mat,category,profiles)
        save(mat)
        report['pokemon'][category]+=1
        if sum(report['pokemon'].values())%40==0:progress()
    # Each city material keeps its complete editable texture/water graph.
    for path in LIB.list_assets(CITY+'/Materials',recursive=False,include_folder=False):
        if '/M_R02_' not in path:continue
        original=LIB.load_asset(path)
        name=original.get_name()
        category='Water' if name.endswith('_Water') else 'Foliage' if name.endswith(('_Green','_Leaf')) else 'Emission' if name.endswith(('_Glow','_CyanGlow')) else 'Metal' if name.endswith(('_Bronze','_Aluminum')) else 'Environment'
        backup(original)
        if not unreal.UEToonEditorLibrary.convert_legacy_material(original,profiles[category]):
            report['skipped'].append(path);continue
        save(original)
        instance=duplicate(FOLDER+'/Presets/MI_HHV_'+category,FOLDER+'/Environment/MI_HHV_'+name.removeprefix('M_'))
        MEL.set_material_instance_parent(instance,original)
        configure(instance,category,profiles)
        save(instance)
        report['environment'][category]+=1
        progress()
    land_path=CITY+'/Meshes/SM_Goldenrod_City_Land_R03'
    city_mesh=LIB.load_asset(land_path if LIB.does_asset_exist(land_path) else CITY+'/Meshes/SM_Goldenrod_City_R03')
    backup(city_mesh)
    for index,slot in enumerate(city_mesh.get_editor_property('static_materials')):
        old=slot.material_interface
        if not old:continue
        name=old.get_name().removeprefix('MI_HHV_').removeprefix('M_')
        target=FOLDER+'/Environment/MI_HHV_'+name
        if LIB.does_asset_exist(target):city_mesh.set_material(index,LIB.load_asset(target))
    save(city_mesh)
    water=LIB.load_asset(FOLDER+'/Environment/MI_HHV_R02_Accent_Water')
    ocean_path=CITY+'/Meshes/SM_Goldenrod_Ocean'
    plane=LIB.load_asset(ocean_path if LIB.does_asset_exist(ocean_path) else CITY+'/Meshes/SM_Goldenrod_OceanPlane')
    # The separate sea retains its own editable material and actor override.
    if not LIB.does_asset_exist(CITY+'/Materials/MI_Goldenrod_Ocean'):
        backup(plane);plane.set_material(0,water);save(plane)
    city_bp=LIB.load_asset(CITY+'/Blueprints/BP_GoldenrodCity')
    backup(city_bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(city_bp);save(city_bp)
    # Thin body outlines; face/eyes and translucent fire are deliberately not given a shell.
    for mesh in pokemon_meshes.values():
        if any('fire' in str(s.material_slot_name).lower() for s in mesh.get_editor_property('materials')):continue
        backup(mesh)
        mesh.set_editor_property('overlay_material',outline)
        mesh.set_editor_property('overlay_material_max_draw_distance',2200.)
        save(mesh);report['outlines']+=1
    species_count=len(pokemon_meshes)
    pokemon_meshes.clear()
    unreal.SystemLibrary.collect_garbage()
    for path in LIB.list_assets('/Game/CharacterCustomization/Palworld/AssetsFBX',recursive=True,include_folder=False):
        if str(LIB.find_asset_data(path).asset_class_path.asset_name)!='SkeletalMesh':continue
        if '/Head/' in path or '/Eye/' in path:continue
        mesh=LIB.load_asset(path);backup(mesh)
        mesh.set_editor_property('overlay_material',outline)
        mesh.set_editor_property('overlay_material_max_draw_distance',2200.)
        save(mesh);report['outlines']+=1
        if report['outlines']%20==0:
            progress()
            unreal.SystemLibrary.collect_garbage()
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    backup(LIB.load_asset(CITY+'/Maps/L_Goldenrod'))
    levels.load_level(CITY+'/Maps/L_Goldenrod')
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if isinstance(actor,unreal.UEGoldenrodCity):
            # Replace only known city materials; preserve unrelated per-actor overrides.
            for i in range(actor.city_mesh.get_num_materials()):
                old=actor.city_mesh.get_material(i)
                if not old:continue
                name=old.get_name().removeprefix('MI_HHV_').removeprefix('M_')
                target=FOLDER+'/Environment/MI_HHV_'+name
                if LIB.does_asset_exist(target):actor.city_mesh.set_material(i,LIB.load_asset(target))
    if not levels.save_current_level():raise RuntimeError('Goldenrod save failed')
    report['species_count']=species_count
    (OUT/'applied.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('HHV TOON APPLIED '+json.dumps(report,ensure_ascii=False))


def main():
    profiles={name:create_profile(name) for name in STYLES}
    masters=create_masters(profiles)
    outline=make_outline()
    (OUT/'created.json').write_text(json.dumps({'profiles':{k:v.get_path_name() for k,v in profiles.items()},
        'masters':{k:v.get_path_name() for k,v in masters.items()},'outline':outline.get_path_name()},indent=2),encoding='utf-8')
    if '--apply' in sys.argv:apply_all(masters,profiles,outline)
    unreal.log('HHV TOON AUTHORING COMPLETE')


if __name__=='__main__':main()
