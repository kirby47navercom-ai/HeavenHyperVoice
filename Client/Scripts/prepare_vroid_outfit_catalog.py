from __future__ import annotations

import argparse
import importlib.util
import json
import shutil
import subprocess
import sys
from dataclasses import replace
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_ROOT = PROJECT_ROOT / "Intermediate" / "VRoidOutfitCatalog"
BLENDER = Path(r"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe")
CATEGORIES = ("Tops", "Bottoms", "Onepiece", "Shoes")
SEMANTICS = {
    "Tops": "Top",
    "Bottoms": "Bottom",
    "Onepiece": "Onepiece",
    "Shoes": "Shoes",
}


def find_package_root() -> Path:
    matches = [
        path
        for path in (Path.home() / "OneDrive").rglob(
            "Unreal_Customization_Package_20260808_215024"
        )
        if path.is_dir()
    ]
    if not matches:
        raise FileNotFoundError("Could not locate the extracted VRoid package")
    return matches[0]


def load_customizer(tool_root: Path):
    module_path = tool_root / "vroid_customizer.py"
    sys.path.insert(0, str(tool_root))
    spec = importlib.util.spec_from_file_location("vroid_customizer", module_path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def clean_style(model: str, style: str) -> str:
    value = f"{model}_{style}"
    return "".join(character if character.isalnum() else "_" for character in value)


def material_values(material_path: Path) -> dict:
    data = json.loads(material_path.read_text(encoding="utf-8"))
    return next(iter(data.values())) if data else {}


def copy_texture(material_root: Path, item_root: Path, texture_name: str) -> str:
    if not texture_name:
        return ""
    source = material_root / texture_name
    if not source.exists():
        return ""
    target = item_root / source.name
    if source.resolve() != target.resolve():
        shutil.copy2(source, target)
    return target.name


def write_clothing_obj(obj_data, target: Path, semantic: str, values: dict, material_root: Path) -> Path:
    material_name = f"M_{semantic}"
    mtl_path = target.with_suffix(".mtl")
    color = values.get("color", [1.0, 1.0, 1.0, 1.0])
    alpha = float(color[3]) if len(color) > 3 else 1.0
    main_texture = copy_texture(material_root, target.parent, values.get("main_texture", ""))
    normal_texture = copy_texture(material_root, target.parent, values.get("normal_texture", ""))
    shade_texture = copy_texture(material_root, target.parent, values.get("shade_texture", ""))
    lines = [
        f"newmtl {material_name}",
        "Ka 0 0 0",
        f"Kd {float(color[0]):.9g} {float(color[1]):.9g} {float(color[2]):.9g}",
        "Ks 0 0 0",
        f"d {alpha:.9g}",
        "illum 2",
    ]
    if main_texture:
        lines.append(f"map_Kd {main_texture}")
    if normal_texture:
        lines.append(f"map_Bump {normal_texture}")
        lines.append(f"bump {normal_texture}")
    if shade_texture and shade_texture != main_texture:
        lines.append(f"# VRoid shade_texture {shade_texture}")
    mtl_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    with target.open("w", encoding="utf-8", newline="\n") as output:
        output.write(f"mtllib {mtl_path.name}\n")
        for x, y, z in obj_data.verts:
            output.write(f"v {x:.9g} {y:.9g} {z:.9g}\n")
        for u, v in obj_data.uvs:
            output.write(f"vt {u:.9g} {v:.9g}\n")
        output.write(f"g Clothing_{semantic}\nusemtl {material_name}\n")
        for triangles in obj_data.groups.values():
            for triangle in triangles:
                tokens = []
                for vertex, uv in triangle:
                    if 0 <= uv < len(obj_data.uvs):
                        tokens.append(f"{vertex + 1}/{uv + 1}")
                    else:
                        tokens.append(str(vertex + 1))
                output.write("f " + " ".join(tokens) + "\n")
    return target


def prepare(limit_per_category: int, genders: tuple[str, ...]) -> Path:
    package_root = find_package_root()
    tool_root = package_root / "10_Customizer_EXE"
    customizer = load_customizer(tool_root)
    assets = customizer.AssetModel(package_root)
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    manifest_items = []

    for gender in genders:
        shared_skeleton = OUTPUT_ROOT / "Shared" / gender / "VRoid_Skeleton.json"
        gender_root = assets.preferred_root_for_gender(gender)
        base_combo = replace(
            assets.combo_for_root(gender_root),
            body_gender=gender,
            body_shape={},
            clothing_presets={},
        )
        for category in CATEGORIES:
            presets = list(assets.clothing_presets(category))
            if limit_per_category > 0:
                presets = presets[:limit_per_category]
            for preset in presets:
                style = clean_style(preset.model, preset.style)
                item_root = OUTPUT_ROOT / gender / category / f"Style_{style}"
                item_root.mkdir(parents=True, exist_ok=True)
                combo = replace(base_combo, clothing_presets={category: preset})
                npz_path, skeleton_path = customizer.write_unreal_skin_data(
                    assets, combo, item_root
                )
                shared_skeleton.parent.mkdir(parents=True, exist_ok=True)
                if not shared_skeleton.exists():
                    shutil.copy2(skeleton_path, shared_skeleton)
                deformed = assets.deformed_clothing(combo, preset)
                if deformed is None:
                    raise RuntimeError(f"Could not deform {gender} {preset.label}")
                source_materials = preset.obj_for_gender(gender).with_name(
                    f"{gender.lower()}.materials.json"
                )
                values = material_values(source_materials)
                source_obj = write_clothing_obj(
                    deformed,
                    item_root / f"{SEMANTICS[category]}_{gender}_{style}.obj",
                    SEMANTICS[category],
                    values,
                    source_materials.parent,
                )
                target_name = f"SK_{SEMANTICS[category]}_{gender}_{style}.fbx"
                manifest_items.append(
                    {
                        "gender": gender,
                        "category": category,
                        "semantic": SEMANTICS[category],
                        "style": style,
                        "label": preset.label,
                        "obj": str(source_obj),
                        "materials": str(source_materials),
                        "weights": str(npz_path),
                        "skeleton": str(shared_skeleton),
                        "output": str(item_root / target_name),
                    }
                )
                print(f"prepared {gender} {category} {style}", flush=True)

    manifest_path = OUTPUT_ROOT / "outfit_manifest.json"
    manifest_path.write_text(
        json.dumps({"items": manifest_items}, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    return manifest_path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--limit-per-category", type=int, default=0)
    parser.add_argument(
        "--gender", choices=("Male", "Female", "Both"), default="Both"
    )
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args()
    genders = ("Male", "Female") if args.gender == "Both" else (args.gender,)
    manifest_path = prepare(args.limit_per_category, genders)
    if args.prepare_only:
        print(manifest_path)
        return
    if not BLENDER.exists():
        raise FileNotFoundError(BLENDER)
    exporter = Path(__file__).with_name("blender_export_vroid_outfit_catalog.py")
    creationflags = (
        subprocess.BELOW_NORMAL_PRIORITY_CLASS
        if sys.platform == "win32"
        else 0
    )
    result = subprocess.run(
        [
            str(BLENDER),
            "--background",
            "--python",
            str(exporter),
            "--",
            str(manifest_path),
        ],
        cwd=str(PROJECT_ROOT),
        check=True,
        creationflags=creationflags,
    )
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
