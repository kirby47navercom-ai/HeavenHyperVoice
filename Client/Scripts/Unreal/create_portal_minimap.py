"""Author editable portal confirmation and round minimap widgets. Does not run PIE."""
import asyncio,json,sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
import create_options_menu as ui
LIB=unreal.EditorAssetLibrary
MEL=unreal.MaterialEditingLibrary
FOLDER='/Game/UI/Navigation'
DARK=ui.color(.025,.045,.060,.96)
LIGHT=ui.color(.88,.92,.92)
GOLD=ui.color(.65,.53,.32)

def material(name,code,texture=False):
    path=FOLDER+'/'+name
    if LIB.does_asset_exist(path):return LIB.load_asset(path)
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,FOLDER,unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    uv=MEL.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-600,200)
    shape=MEL.create_material_expression(mat,unreal.MaterialExpressionCustom,-320,200)
    shape.set_editor_property('code',code)
    shape.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inp=unreal.CustomInput();inp.set_editor_property('input_name','UV')
    shape.set_editor_property('inputs',[inp])
    MEL.connect_material_expressions(uv,'',shape,'UV')
    MEL.connect_material_property(shape,'',unreal.MaterialProperty.MP_OPACITY)
    if texture:
        rgb=MEL.create_material_expression(mat,unreal.MaterialExpressionTextureSampleParameter2D,-300,-100)
        rgb.set_editor_property('parameter_name','MapTexture')
        rgb.set_editor_property('texture',LIB.load_asset('/Engine/EngineResources/DefaultTexture'))
        MEL.connect_material_property(rgb,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        rgb=MEL.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-300,-100)
        rgb.set_editor_property('constant',unreal.LinearColor(1,1,1,1))
        MEL.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    if not LIB.save_loaded_asset(mat,only_if_is_dirty=False):raise RuntimeError('Material save failed')
    return mat

async def image(bp,parent,name,mat,rect,tint=LIGHT,z=5):
    return await ui.add(bp,'Image',name,parent,{'brush':{'resourceObject':ui.ref(mat.get_path_name()),'drawAs':'Image'},
        'colorAndOpacity':tint,'visibility':'HitTestInvisible'},rect,z=z)

async def button(bp,parent,name,label,rect):
    b=await ui.add(bp,'Button',name,parent,{'widgetStyle':{
        'normal':ui.brush(ui.color(.06,.10,.12),8,GOLD,1),
        'hovered':ui.brush(ui.color(.10,.22,.24),8,LIGHT,1),
        'pressed':ui.brush(ui.color(.03,.07,.08),8,GOLD,2)},'isFocusable':True},rect,z=8)
    await ui.add(bp,'TextBlock',name+'Label',b,{'text':label,'font':{'fontObject':ui.ref(ui.FONT),'size':19},
        'colorAndOpacity':ui.slate(LIGHT),'justification':'Center','visibility':'HitTestInvisible'})

async def author():
    ui.FOLDER=FOLDER
    report={}
    mask=material('M_MinimapCircle','return 1-smoothstep(0.493,0.5,length(UV-0.5));',True)
    arrow=material('M_MinimapArrow','float2 p=UV-float2(0.5,0.1); return (p.y>=0 && p.y<0.75 && abs(p.x)<p.y*0.55 && p.y<0.58+abs(p.x)*0.45) ? 1 : 0;')
    cone=material('M_MinimapView','float2 p=UV-float2(0.5,0.5); return (p.y<0 && abs(p.x)<-p.y*0.65) ? saturate(1-length(p)*2)*0.6 : 0;')
    path=FOLDER+'/WBP_Minimap'
    if not LIB.does_asset_exist(path):
        bp=await ui.create('WBP_Minimap','/Script/HeavenHyperVoice.UEMinimapWidget')
        root=await ui.add(bp,'CanvasPanel','MinimapLayout',values={'visibility':'HitTestInvisible'})
        await ui.panel(bp,root,'MapRim',(4,14,228,228),DARK,114,GOLD,2)
        await image(bp,root,'MapImage',mask,(10,20,216,216))
        # MarkerLayer is an editable canvas for later server-supplied player markers.
        markers=await ui.add(bp,'CanvasPanel','MarkerLayer',root,{'visibility':'HitTestInvisible'},(10,20,216,216),z=7)
        await image(bp,markers,'ViewArrow',cone,(66,66,84,84),ui.color(.45,.90,.94),8)
        await image(bp,markers,'PlayerArrow',arrow,(95,95,26,26),LIGHT,9)
        await ui.panel(bp,root,'NorthPlate',(101,6,34,26),DARK,10,GOLD,1,z=9)
        await ui.text(bp,root,'NorthText','N',(101,7,34,23),14,LIGHT,True,'Center')
        await ui.text(bp,root,'LocationText','금빛시티',(10,246,216,24),16,LIGHT,True,'Center')
        await ui.finish(bp)
    path=FOLDER+'/WBP_PortalConfirm'
    if not LIB.does_asset_exist(path):
        bp=await ui.create('WBP_PortalConfirm','/Script/HeavenHyperVoice.UEOptionsConfirmWidget')
        root=await ui.add(bp,'CanvasPanel','ScreenRoot',values={'visibility':'SelfHitTestInvisible'})
        shield=await ui.add(bp,'Border','ModalBackdrop',root,{'brushColor':ui.color(0,0,0,.32),'visibility':'Visible'})
        await ui.canvas(shield,(0,0,0,0),anchors=(0,0,1,1))
        surface=await ui.add(bp,'CanvasPanel','DialogLayout',root)
        await ui.canvas(surface,(0,0,600,320),anchors=(.5,.5,.5,.5),alignment=(.5,.5),z=2)
        await ui.panel(bp,surface,'DialogPanel',(0,0,600,320),DARK,18,GOLD,1)
        await ui.text(bp,surface,'ConfirmTitleText','필드로 이동',(32,26,536,42),26,LIGHT,True)
        await ui.panel(bp,surface,'TitleRule',(32,84,536,1),GOLD,0)
        message=await ui.text(bp,surface,'ConfirmMessageText','포탈을 통해 다음 지역으로 이동할까요?',(32,110,536,90),18,LIGHT)
        await ui.props(ui.path_of(message['widget']),{'autoWrapText':True})
        await button(bp,surface,'BackButton','머무르기 · ESC',(32,228,252,58))
        await button(bp,surface,'ConfirmButton','이동하기',(316,228,252,58))
        await ui.finish(bp)
    hud='/Game/UI/Options/WBP_OptionsHUD.WBP_OptionsHUD'
    LIB.load_asset(hud)
    if not unreal.find_object(None,hud+':WidgetTree.Minimap'):
        root={'widget':ui.ref(hud+':WidgetTree.HUDRoot')}
        await ui.add(hud,FOLDER+'/WBP_Minimap.WBP_Minimap_C','Minimap',root,
            {'visibility':'HitTestInvisible'},(20,118,236,280))
    await ui.finish(hud)
    controller=LIB.load_asset('/Game/Blueprints/Login/BP_LoginPlayerController')
    unreal.get_default_object(controller.generated_class()).set_editor_property('portal_confirm_class',LIB.load_blueprint_class(FOLDER+'/WBP_PortalConfirm'))
    unreal.BlueprintEditorLibrary.compile_blueprint(controller)
    if not LIB.save_loaded_asset(controller,only_if_is_dirty=False):raise RuntimeError('Controller save failed')
    report['success']=True
    report['assets']=[FOLDER+'/WBP_Minimap',FOLDER+'/WBP_PortalConfirm',hud]
    out=Path(unreal.Paths.project_saved_dir())/'Codex/Navigation';out.mkdir(parents=True,exist_ok=True)
    (out/'authored.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    unreal.log('NAVIGATION UI AUTHORED')

if __name__=='__main__':asyncio.run(author())
