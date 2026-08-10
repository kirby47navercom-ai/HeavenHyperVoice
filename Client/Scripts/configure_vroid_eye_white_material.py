import json
from pathlib import Path

import unreal


SOURCE_PATH = "/Game/VRoidCatalog/FaceGeometryDetermined/Skin"
MATERIAL_PATH = "/Game/VRoidCatalog/FaceGeometryDetermined/EyeWhiteOpaque"
REPORT_PATH = (
    Path(unreal.Paths.project_saved_dir())
    / "Diagnostics"
    / "VRoidEyeWhiteMaterial.json"
)


material = unreal.load_asset(MATERIAL_PATH)
if material is None:
    material = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_PATH, MATERIAL_PATH)
if not isinstance(material, unreal.MaterialInstanceConstant):
    raise RuntimeError(f"Unable to create eye-white material: {MATERIAL_PATH}")

overrides = material.get_editor_property("base_property_overrides")
overrides.set_editor_property("override_two_sided", True)
overrides.set_editor_property("two_sided", True)
material.set_editor_property("base_property_overrides", overrides)
unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)

saved_overrides = material.get_editor_property("base_property_overrides")
REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(
    json.dumps(
        {
            "material": material.get_path_name(),
            "parent": material.get_editor_property("parent").get_path_name(),
            "override_two_sided": saved_overrides.get_editor_property(
                "override_two_sided"
            ),
            "two_sided": saved_overrides.get_editor_property("two_sided"),
        },
        indent=2,
    ),
    encoding="utf-8",
)
