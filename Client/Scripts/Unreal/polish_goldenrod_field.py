"""Editor authoring: Nanite city geometry without fog. No PIE."""
import unreal

LEVEL = "/Game/Environments/Goldenrod_R03/Maps/L_Goldenrod"


def save_nanite_materials(mesh):
    saved = set()
    for slot in mesh.get_editor_property("static_materials"):
        material = slot.get_editor_property("material_interface")
        if not material:
            continue
        base = material if isinstance(material, unreal.Material) else material.get_base_material()
        if base.get_path_name() in saved:
            continue
        unreal.MaterialEditingLibrary.set_base_material_usage(base, unreal.MaterialUsage.MATUSAGE_NANITE, True)
        if not unreal.MaterialEditingLibrary.has_material_usage(base, unreal.MaterialUsage.MATUSAGE_NANITE):
            raise RuntimeError("Nanite usage was not enabled: " + base.get_path_name())
        if not unreal.EditorAssetLibrary.save_loaded_asset(base, only_if_is_dirty=False):
            raise RuntimeError("Could not save Nanite material: " + base.get_path_name())
        saved.add(base.get_path_name())
    return sorted(saved)


def main():
    if not unreal.EditorLevelLibrary.load_level(LEVEL):
        raise RuntimeError("Goldenrod level could not be loaded")
    assets = unreal.EditorAssetLibrary
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()
    city = next(a for a in actors if isinstance(a, unreal.UEGoldenrodCity))
    mesh = city.get_editor_property("city_mesh").get_editor_property("static_mesh")
    save_nanite_materials(mesh)
    settings = mesh.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", True)
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh, settings, True)
    assets.save_loaded_asset(mesh, only_if_is_dirty=False)

    from remove_goldenrod_fog import main as remove_fog
    remove_fog()


if __name__ == "__main__":
    main()
