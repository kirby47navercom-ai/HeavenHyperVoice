"""Separate Goldenrod land from one continuous, independently editable ocean. No PIE."""
import json
import shutil
from pathlib import Path
import unreal

CITY = '/Game/Environments/Goldenrod_R03'
LIB = unreal.EditorAssetLibrary
ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT/'Saved/Codex/SeparateOcean'
TAG = 'HHV_GoldenrodOcean'


def save(asset):
    if not LIB.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Save failed: '+asset.get_path_name())


def backup(package, extension='.uasset'):
    relative=package.removeprefix('/Game/')+extension
    source=ROOT/'Content'/relative
    target=OUT/'Before'/relative
    if source.exists() and not target.exists():
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level=CITY+'/Maps/L_Goldenrod'
    backup(level,'.umap')
    backup(CITY+'/Blueprints/BP_GoldenrodCity')
    source=LIB.load_asset(CITY+'/Meshes/SM_Goldenrod_City_R03')
    bounds=source.get_bounding_box()
    land_path=CITY+'/Meshes/SM_Goldenrod_City_Land_R03'
    fresh=not LIB.does_asset_exist(land_path)
    land=LIB.duplicate_asset(source.get_path_name(),land_path) if fresh else LIB.load_asset(land_path)
    removed=unreal.UEGoldenrodEditorLibrary.remove_sea_surface(land,-175.)
    if removed<0 or (fresh and removed==0):
        raise RuntimeError('Expected horizontal Water faces at z=-175; source mesh kept intact')
    save(land)
    # Independent material instance and one single-slot mesh for the whole sea.
    water_path=CITY+'/Materials/MI_Goldenrod_Ocean'
    old_plane=LIB.load_asset(CITY+'/Meshes/SM_Goldenrod_OceanPlane')
    if not LIB.does_asset_exist(water_path):
        water=LIB.duplicate_asset(old_plane.get_material(0).get_path_name(),water_path)
        save(water)
    else:water=LIB.load_asset(water_path)
    plane_path=CITY+'/Meshes/SM_Goldenrod_Ocean'
    plane=LIB.load_asset(plane_path) if LIB.does_asset_exist(plane_path) else LIB.duplicate_asset(old_plane.get_path_name(),plane_path)
    plane.set_material(0,water)
    # A single shadowless plane needs no Nanite; allow translucent replacement water materials.
    nanite=plane.get_editor_property('nanite_settings')
    if nanite.get_editor_property('enabled'):
        nanite.set_editor_property('enabled',False)
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(plane,nanite,True)
    save(plane)
    bp=LIB.load_asset(CITY+'/Blueprints/BP_GoldenrodCity')
    unreal.get_default_object(bp.generated_class()).city_mesh.set_static_mesh(land)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp)
    if not levels.load_level(level):raise RuntimeError('Cannot load Goldenrod')
    cities=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.UEGoldenrodCity)]
    if len(cities)!=1:raise RuntimeError('Expected one Goldenrod city')
    city=cities[0]
    city.city_mesh.set_static_mesh(land)
    # Native OceanExtension was removed; discard any serialized legacy component too.
    for component in city.get_components_by_class(unreal.InstancedStaticMeshComponent):
        if component.get_name()=='OceanExtension':
            component.clear_instances()
            component.set_static_mesh(None)
    oceans=[a for a in actors.get_all_level_actors() if a.actor_has_tag(TAG)]
    if len(oceans)>1:raise RuntimeError('Duplicate independent ocean actors')
    if oceans:
        ocean=oceans[0]  # Preserve later hand-edited transforms/material overrides.
    else:
        ocean=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
        ocean.set_actor_label('Goldenrod_Ocean')
        ocean.set_editor_property('tags',[TAG])
        center=unreal.Vector((bounds.min.x+bounds.max.x)*.5,(bounds.min.y+bounds.max.y)*.5,-175.)
        location=unreal.MathLibrary.transform_location(city.get_actor_transform(),center)
        ocean.set_actor_location(location,False,False)
        ocean.set_actor_rotation(city.get_actor_rotation(),False)
        scale=city.get_actor_scale3d()
        ocean.set_actor_scale3d(unreal.Vector((bounds.max.x-bounds.min.x+200000.)/100.*scale.x,
                                            (bounds.max.y-bounds.min.y+200000.)/100.*scale.y,scale.z))
    component=ocean.static_mesh_component
    component.set_static_mesh(plane)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name('NoCollision')
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_cast_shadow(False)
    component.set_editor_property('generate_overlap_events',False)
    if not levels.save_current_level():raise RuntimeError('Goldenrod save failed')
    report={'land':land.get_path_name(),'removed_sea_triangles_this_run':removed,
            'ocean_actor':ocean.get_actor_label(),'ocean_mesh':plane.get_path_name(),
            'ocean_material_slots':len(plane.get_editor_property('static_materials')),
            'ocean_transform':str(ocean.get_actor_transform()),
            'native_collision_boxes':len(city.get_components_by_class(unreal.BoxComponent)),
            'legacy_ocean_instances':sum(c.get_instance_count() for c in city.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()=='OceanExtension')}
    (OUT/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('GOLDENROD OCEAN SEPARATED '+json.dumps(report))
    return report


if __name__=='__main__':main()
