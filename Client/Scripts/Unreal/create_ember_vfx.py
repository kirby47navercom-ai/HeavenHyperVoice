"""Author stationary Ember attack and one-shot impact assets in a rendering UE editor.

Run alongside create_water_gun_vfx.py. Existing finished assets are never overwritten.
"""
import json
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import create_water_gun_vfx as water

ROOT = '/Game/VFX/Pokemon/Ember'
LIB = unreal.EditorAssetLibrary
OUT = Path(unreal.Paths.project_saved_dir()) / 'Codex/Ember'


def material(kind):
    path = ROOT + '/Materials/M_Ember_' + kind
    if LIB.does_asset_exist(path):
        return LIB.load_asset(path)
    # Reuse the existing Niagara sprite material graph, then author flame masks/colors.
    old_root = water.ROOT
    water.ROOT = ROOT
    try:
        mat = water.make_material('M_Ember_' + kind, 'drop')
    finally:
        water.ROOT = old_root
    custom = unreal.find_object(None, mat.get_path_name() + ':MaterialExpressionCustom_0')
    shapes = {
        'Flame': '''
float a = atan2(p.y,p.x);
float radius = length(p * float2(1.05,0.87));
float lobes = 0.64 + 0.055*sin(a*5+Age*19) + 0.035*sin(a*9-Age*27);
float d = radius / (lobes * lerp(1.0,0.45,Age));
float mask = 1-smoothstep(0.77,1.02,d);
float heat = saturate(1-d);
float3 col = lerp(float3(1,0.075,0.003),float3(1,0.68,0.025),smoothstep(0.05,0.3,heat));
col = lerp(col,float3(1,0.95,0.36),smoothstep(0.36,0.73,heat));
''',
        'Spark': '''
float d = length(p*float2(1.3,0.72)) / lerp(0.78,0.28,Age);
float mask = 1-smoothstep(0.32,1.0,d);
float3 col = lerp(float3(1,0.09,0.002),float3(1,0.88,0.15),saturate(1-d));
''',
        'Glow': '''
float d=length(p);
float mask=pow(saturate(1-d),3)*0.32;
float3 col=float3(1,0.24,0.006);
''',
        'Flash': '''
float a=atan2(p.y,p.x);
float d=length(p)/(0.62+0.085*sin(a*7)+0.045*sin(a*11+Age*8));
float mask=(1-smoothstep(0.68,1.0,d))*saturate((1-Age)*2);
float3 col=lerp(float3(1,0.14,0.003),float3(1,0.92,0.3),saturate(1-d)*1.5);
''',
    }
    custom.set_editor_property('code', 'float2 p=UV*2-1;\n' + shapes[kind] +
                               '\nreturn float4(col,mask*saturate((1-Age)*3));')
    gain = unreal.find_object(None, mat.get_path_name() + ':MaterialExpressionScalarParameter_0')
    gain.set_editor_property('default_value', 2.2 if kind != 'Glow' else 1.4)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    if not LIB.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError('Material save failed: ' + path)
    return mat


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    specs = {
        'Projectile': [
            ('LeadingFlames','Flame',32,(.13,.22),(19,29),6,(0,0),0,0,1),
            ('TrailingFlames','Flame',95,(.18,.32),(13,25),9,(230,370),12,30,1),
            ('FlyingEmbers','Spark',85,(.23,.4),(3,8),12,(180,330),27,45,1),
            ('WarmGlow','Glow',18,(.16,.24),(55,72),5,(110,170),10,0,.55),
        ],
        'Impact': [
            ('FlamePetals','Flame',23,(.22,.42),(21,40),8,(105,230),0,70,1),
            ('HotFlash','Flash',1,(.13,.13),(70,70),0,(0,0),0,0,.9),
            ('ScatteredEmbers','Spark',45,(.3,.62),(3,9),7,(140,300),0,-180,1),
            ('FadingGlow','Glow',1,(.3,.3),(160,160),0,(0,0),0,0,.75),
        ],
    }
    for suffix in specs:
        for prefix in ('NS_', 'BP_'):
            if LIB.does_asset_exist(ROOT + '/' + prefix + 'Ember_' + suffix):
                raise RuntimeError('Existing Ember assets retained; edit them directly in the editor.')
    mats = {k: material(k) for k in ('Flame','Spark','Glow','Flash')}
    report = []
    for suffix, layers in specs.items():
        name = 'NS_Ember_' + suffix
        system = LIB.duplicate_asset('/Niagara/DefaultAssets/Templates/Systems/FountainLightweight', ROOT+'/'+name)
        if not system:
            raise RuntimeError('Cannot create ' + name)
        for i,(label,kind,count,life,size,radius,speed,angle,gravity,alpha) in enumerate(layers):
            water.configure_layer(system,label,i>0,mats[kind],suffix=='Impact',count,life,size,radius,speed,angle,gravity,alpha)
        if not water.EDIT.finish_water_system(system):
            raise RuntimeError('Niagara compile failed: ' + name)
        if not LIB.save_loaded_asset(system, only_if_is_dirty=False):
            raise RuntimeError('Niagara save failed: ' + name)
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.NiagaraActor)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_Ember_'+suffix,ROOT,unreal.Blueprint,factory)
        defaults = unreal.get_default_object(bp.generated_class())
        defaults.get_component_by_class(unreal.NiagaraComponent).set_asset(system)
        if suffix == 'Impact':
            defaults.set_editor_property('initial_life_span', 1.5)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        if not LIB.save_loaded_asset(bp, only_if_is_dirty=False):
            raise RuntimeError('Blueprint save failed: ' + suffix)
        report.append({'system':ROOT+'/'+name,'layers':[x[0] for x in layers],'ready':True})
    (OUT/'authored.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    unreal.log('EMBER VFX AUTHORED')


if __name__ == '__main__':
    main()
