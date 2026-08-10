import json
from pathlib import Path

import unreal


MATERIAL_PATH = "/Game/VRoidCatalog/FaceGeometryDetermined/Brow"
REPORT_PATH = (
    Path(unreal.Paths.project_saved_dir())
    / "Diagnostics"
    / "VRoidFaceLayerMaterial.json"
)


material = unreal.load_asset(MATERIAL_PATH)
if not isinstance(material, unreal.MaterialInstanceConstant):
    raise RuntimeError(f"Missing face layer material instance: {MATERIAL_PATH}")

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
            "override_two_sided": saved_overrides.get_editor_property(
                "override_two_sided"
            ),
            "two_sided": saved_overrides.get_editor_property("two_sided"),
        },
        indent=2,
    ),
    encoding="utf-8",
)
