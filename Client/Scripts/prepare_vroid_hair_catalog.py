from __future__ import annotations

import importlib.util
import shutil
import subprocess
import sys
from dataclasses import replace
from pathlib import Path

from PIL import Image


PACKAGE_ROOT = next(
    path
    for path in (Path.home() / "OneDrive").rglob(
        "Unreal_Customization_Package_20260808_215024"
    )
    if path.is_dir()
)
TOOL_ROOT = PACKAGE_ROOT / "10_Customizer_EXE"
CUSTOMIZER_PATH = TOOL_ROOT / "vroid_customizer.py"
BLENDER = Path(r"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe")
OUTPUT_ROOT = Path(__file__).resolve().parents[1] / "Intermediate" / "VRoidHairCatalog"
SEMANTICS = ("HairFront", "HairSide", "HairBack", "HairExtra")


def load_customizer():
    sys.path.insert(0, str(TOOL_ROOT))
    spec = importlib.util.spec_from_file_location("vroid_customizer", CUSTOMIZER_PATH)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def generate() -> None:
    if not BLENDER.exists():
        raise FileNotFoundError(BLENDER)
    customizer = load_customizer()
    assets = customizer.AssetModel(PACKAGE_ROOT)
    rows = {row.mesh_id: row for row in assets.slot_rows("HairFront")}
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    exporter = Path(__file__).with_name("blender_export_hair_catalog.py")
    preview = Image.new("RGB", (16, 16), "white")
    flags = subprocess.CREATE_NO_WINDOW if hasattr(subprocess, "CREATE_NO_WINDOW") else 0

    for gender in ("Male", "Female"):
        gender_root = assets.preferred_root_for_gender(gender)
        base_combo = replace(
            assets.combo_for_root(gender_root),
            body_gender=gender,
            body_shape={},
            clothing_presets={},
        )
        shared = OUTPUT_ROOT / "Shared" / gender
        shared.mkdir(parents=True, exist_ok=True)
        _npz_path, skeleton_path = customizer.write_unreal_skin_data(
            assets, base_combo, shared
        )

        for mesh_id, row in sorted(rows.items()):
            style_root = OUTPUT_ROOT / gender / f"Style_{mesh_id}"
            output_dir = style_root / "FBX"
            export_id = f"{gender}_{mesh_id}"
            expected = [
                output_dir / f"SK_{semantic}_{export_id}.fbx"
                for semantic in SEMANTICS
            ]
            if all(path.exists() for path in expected[:3]):
                print(f"skip {gender} {mesh_id}")
                continue
            if style_root.exists():
                shutil.rmtree(style_root)
            style_root.mkdir(parents=True)
            combo = replace(
                base_combo,
                hair=row,
                hair_parts={semantic: row for semantic in SEMANTICS},
                clothing_presets={},
            )
            obj_path = customizer.write_combined_obj(
                assets, combo, style_root, preview, {}, auto_align=True
            )
            subprocess.run(
                [
                    str(BLENDER),
                    "--background",
                    "--python",
                    str(exporter),
                    "--",
                    str(obj_path),
                    str(skeleton_path),
                    str(output_dir),
                    export_id,
                ],
                cwd=str(TOOL_ROOT),
                check=True,
                creationflags=flags,
            )
            if not (output_dir / "Hair_Export_Report.json").exists():
                raise RuntimeError(f"Blender did not finish hair export {gender} {mesh_id}")
            if not all(path.exists() for path in expected[:3]):
                raise RuntimeError(f"Required hair modules are missing for {gender} {mesh_id}")
            print(f"generated hair {gender} {mesh_id}")


if __name__ == "__main__":
    generate()
