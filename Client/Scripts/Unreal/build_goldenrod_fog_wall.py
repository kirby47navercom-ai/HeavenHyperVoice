"""Author animated fog walls around the complete map mesh, including its sea. No PIE."""
import json
from pathlib import Path
import unreal

ROOT = "/Game/Environments/Goldenrod_R03"
LEVEL = ROOT + "/Maps/L_Goldenrod"


def make_material():
    assets = unreal.EditorAssetLibrary
    path = ROOT + "/Materials/M_Goldenrod_FogWall"
    material = assets.load_asset(path) if assets.does_asset_exist(path) else None
    if material:
        return material
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_Goldenrod_FogWall", ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    library = unreal.MaterialEditingLibrary

    def node(cls, x, y, **properties):
        result = library.create_material_expression(material, cls, x, y)
        for key, value in properties.items():
            result.set_editor_property(key, value)
        return result

    def link(source, target, name, output=""):
        if not library.connect_material_expressions(source, output, target, name):
            raise RuntimeError("Could not connect material input " + name)

    world = node(unreal.MaterialExpressionWorldPosition, -1200, 0)
    time = node(unreal.MaterialExpressionTime, -1200, 230)
    wind = node(unreal.MaterialExpressionVectorParameter, -1200, 400,
                parameter_name="WindSpeed", default_value=unreal.LinearColor(12, 4, 2, 0))
    motion = node(unreal.MaterialExpressionMultiply, -950, 250)
    link(time, motion, "A"); link(wind, motion, "B", "RGB")
    position = node(unreal.MaterialExpressionAdd, -700, 0)
    link(world, position, "A"); link(motion, position, "B")
    scale = node(unreal.MaterialExpressionScalarParameter, -700, 220,
                 parameter_name="NoiseScale", default_value=0.0006)
    noise_position = node(unreal.MaterialExpressionMultiply, -460, 0)
    link(position, noise_position, "A"); link(scale, noise_position, "B")
    noise = node(unreal.MaterialExpressionNoise, -210, 0, scale=1.0, levels=2,
                 quality=1, output_min=0.0, output_max=1.0)
    link(noise_position, noise, "")
    fog_color = node(unreal.MaterialExpressionVectorParameter, -210, 230,
                     parameter_name="FogColor", default_value=unreal.LinearColor(0.35, 0.45, 0.51, 1))
    cloud_color = node(unreal.MaterialExpressionVectorParameter, -210, 420,
                       parameter_name="CloudColor", default_value=unreal.LinearColor(0.57, 0.66, 0.71, 1))
    clouds = node(unreal.MaterialExpressionLinearInterpolate, 60, 100)
    link(fog_color, clouds, "A", "RGB"); link(cloud_color, clouds, "B", "RGB")
    link(noise, clouds, "Alpha")
    height = node(unreal.MaterialExpressionComponentMask, -700, -300, r=False, g=False, b=True, a=False)
    link(world, height, "")
    height_scale = node(unreal.MaterialExpressionMultiply, -460, -300, const_b=1.0/18000.0)
    link(height, height_scale, "A")
    height_alpha = node(unreal.MaterialExpressionClamp, -210, -300)
    link(height_scale, height_alpha, "")
    upper = node(unreal.MaterialExpressionVectorParameter, 60, -320,
                 parameter_name="UpperColor", default_value=unreal.LinearColor(0.68, 0.77, 0.83, 1))
    final = node(unreal.MaterialExpressionLinearInterpolate, 360, 0)
    link(clouds, final, "A"); link(upper, final, "B", "RGB"); link(height_alpha, final, "Alpha")
    if not library.connect_material_property(final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Could not connect fog wall color")
    library.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_NANITE, True)
    library.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
    library.recompile_material(material)
    assets.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def main():
    assets = unreal.EditorAssetLibrary
    material = make_material()
    instance_path = ROOT + "/Materials/MI_Goldenrod_FogWall"
    instance = assets.load_asset(instance_path) if assets.does_asset_exist(instance_path) else None
    if not instance:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MI_Goldenrod_FogWall", ROOT + "/Materials", unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance, material)
        assets.save_loaded_asset(instance, only_if_is_dirty=False)
    mesh_path = ROOT + "/Meshes/SM_Goldenrod_FogWall"
    mesh = assets.load_asset(mesh_path) if assets.does_asset_exist(mesh_path) else None
    if not mesh:
        mesh = assets.duplicate_asset("/Engine/BasicShapes/Cube", mesh_path)
        mesh.set_material(0, instance)
        settings = mesh.get_editor_property("nanite_settings")
        settings.set_editor_property("enabled", True)
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh, settings, True)
        assets.save_loaded_asset(mesh, only_if_is_dirty=False)
    bp = assets.load_asset(ROOT + "/Blueprints/BP_GoldenrodCity")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    defaults = unreal.get_default_object(bp.generated_class())
    walls = defaults.get_editor_property("boundary_walls")
    walls.set_static_mesh(mesh)
    walls.set_material(0, instance)
    defaults.rebuild_boundary_walls()
    assets.save_loaded_asset(bp, only_if_is_dirty=False)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name().split(".")[0] != LEVEL:
        if not unreal.EditorLevelLibrary.load_level(LEVEL):
            raise RuntimeError("Cannot load Goldenrod")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    removed = 0
    count = 0
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.LocalFogVolume) and actor.get_actor_label().startswith("Goldenrod Coast Fog "):
            actors.destroy_actor(actor)
            removed += 1
        elif isinstance(actor, unreal.ExponentialHeightFog):
            actor.get_component_by_class(unreal.ExponentialHeightFogComponent).set_visibility(False)
        elif isinstance(actor, unreal.UEGoldenrodCity):
            component = actor.get_editor_property("boundary_walls")
            component.set_static_mesh(mesh)
            component.set_material(0, instance)
            actor.rebuild_boundary_walls()
            count += component.get_instance_count()
    if count != 4:
        raise RuntimeError(f"Expected 4 full-map wall instances, found {count}")
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError("Cannot save fog wall level")
    out = Path(unreal.Paths.project_saved_dir()) / "Codex/CityPolish/fog_wall_applied.json"
    out.write_text(json.dumps({"walls": count, "removed_fog_volumes": removed,
        "material": instance_path, "boundary": "complete map mesh bounds + 200 cm", "shadow_casting": False}, indent=2), encoding="utf-8")
    unreal.log("[FOG WALL] Saved four full-map walls enclosing both land and sea")


if __name__ == "__main__":
    main()
