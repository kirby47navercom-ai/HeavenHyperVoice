"""Replace panel-texture water with continuous world-space ripples; save native prop collision. No PIE."""
import unreal
import json
from pathlib import Path

CITY = '/Game/Environments/Goldenrod_R03'
LIB = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def make_water():
    material = LIB.load_asset(CITY+'/Materials/M_R02_Accent_Water')
    MEL.delete_all_material_expressions(material)
    material.set_editor_property('tangent_space_normal', False)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    def node(cls, x, y, **values):
        item = MEL.create_material_expression(material, cls, x, y)
        for key, value in values.items(): item.set_editor_property(key, value)
        return item
    def wire(src, dst, pin, out=''):
        if not MEL.connect_material_expressions(src, out, dst, pin):
            raise RuntimeError('Could not connect '+pin)
    # Original water's central teal, without the painted-metal panel border or normal map.
    base = node(unreal.MaterialExpressionVectorParameter, -220, -460,
                parameter_name='WaterColor', default_value=unreal.LinearColor(.017, .165, .215, 1))
    rough = node(unreal.MaterialExpressionScalarParameter, -220, -320,
                 parameter_name='WaterRoughness', default_value=.3)
    pos = node(unreal.MaterialExpressionWorldPosition, -1100, 80)
    waves = []
    # World-space phase and normals are shared by imported water and every extension strip.
    for index, direction in enumerate(((.0017,.0009,0),(-.0008,.0023,0))):
        y = index*260
        axis = node(unreal.MaterialExpressionConstant3Vector, -1100, y+150,
                    constant=unreal.LinearColor(*direction,1))
        dot = node(unreal.MaterialExpressionDotProduct, -860, y+70)
        wire(pos,dot,'A'); wire(axis,dot,'B')
        sine = node(unreal.MaterialExpressionSine,-660,y+70)
        wire(dot,sine,'')
        strength = node(unreal.MaterialExpressionMultiply,-460,y+70,const_b=.025)
        wire(sine,strength,'A')
        waves.append(strength)
    xy = node(unreal.MaterialExpressionAppendVector,-220,100)
    wire(waves[0],xy,'A'); wire(waves[1],xy,'B')
    up = node(unreal.MaterialExpressionConstant,-220,280,r=1.)
    xyz = node(unreal.MaterialExpressionAppendVector,0,140)
    wire(xy,xyz,'A'); wire(up,xyz,'B')
    normal = node(unreal.MaterialExpressionNormalize,200,140)
    wire(xyz,normal,'')
    for expression, prop in ((base,unreal.MaterialProperty.MP_BASE_COLOR),
                              (rough,unreal.MaterialProperty.MP_ROUGHNESS),
                              (normal,unreal.MaterialProperty.MP_NORMAL)):
        if not MEL.connect_material_property(expression,'',prop):
            raise RuntimeError('Could not connect material output')
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_NANITE, True)
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
    MEL.recompile_material(material)
    if not LIB.save_loaded_asset(material,only_if_is_dirty=False):
        raise RuntimeError('Water save failed')
    return material


def main():
    material = make_water()
    bp = LIB.load_asset(CITY+'/Blueprints/BP_GoldenrodCity')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not LIB.save_loaded_asset(bp,only_if_is_dirty=False):
        raise RuntimeError('City BP save failed')
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name().split('.')[0] != CITY+'/Maps/L_Goldenrod':
        if not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True):
            raise RuntimeError('Save open work before switching levels')
        levels.load_level(CITY+'/Maps/L_Goldenrod')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    cities = [a for a in actors if isinstance(a,unreal.UEGoldenrodCity)]
    report = {'water':material.get_path_name(),'cities':[]}
    for city in cities:
        props = [c for c in city.get_components_by_class(unreal.BoxComponent) if c.get_name().startswith('Prop_')]
        report['cities'].append({'name':city.get_actor_label(),'prop_boxes':len(props),
            'server_wall_boxes':sum(c.component_has_tag('ServerWall') for c in props)})
        if len(props) != 274:
            raise RuntimeError(f'Expected 274 native prop boxes, got {len(props)}; build/restart first')
    for actor in actors:
        if actor.actor_has_tag('HHV_GoldenrodOcean'):
            actor.static_mesh_component.set_material(0,material)
    if not cities or not levels.save_current_level():
        raise RuntimeError('Goldenrod level save failed')
    report['saved'] = True
    output=Path(unreal.Paths.project_saved_dir())/'Codex/Goldenrod/water_props_saved.json'
    output.write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log('GOLDENROD WATER/PROPS '+json.dumps(report))


if __name__ == '__main__':
    main()
