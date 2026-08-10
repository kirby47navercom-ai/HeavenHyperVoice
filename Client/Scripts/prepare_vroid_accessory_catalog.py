from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_ROOT = PROJECT_ROOT / "Intermediate" / "VRoidAccessoryCatalog"
BLENDER = Path(r"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe")
SKELETON_PATH = (
    PROJECT_ROOT
    / "Intermediate"
    / "VRoidOutfitCatalog"
    / "Shared"
    / "Male"
    / "VRoid_Skeleton.json"
)

CATEGORY_RULES = {
    "BaseballCap": ("HeadAccessory", "J_Bip_C_Head"),
    "SilkHat": ("HeadAccessory", "J_Bip_C_Head"),
    "StrawHat": ("HeadAccessory", "J_Bip_C_Head"),
    "WitchHat": ("HeadAccessory", "J_Bip_C_Head"),
    "WorkCap": ("HeadAccessory", "J_Bip_C_Head"),
    "GlassesHi": ("FaceAccessory", "$Additional:J_Opt_C_Glasses"),
    "GlassesLow": ("FaceAccessory", "$Additional:J_Opt_C_Glasses"),
    "CatEar": ("EarAccessory", "J_Bip_C_Head"),
    "RabbitEar": ("EarAccessory", "J_Bip_C_Head"),
    "CatTail": ("TailAccessory", "J_Bip_C_Hips"),
    "FoxTail": ("TailAccessory", "J_Bip_C_Hips"),
    "RabbitTail": ("TailAccessory", "J_Bip_C_Hips"),
    "N00_001_01_Accessory_Tie": ("NeckAccessory", "J_Bip_C_UpperChest"),
    "N00_001_02_Accessory_Tie": ("NeckAccessory", "J_Bip_C_UpperChest"),
    "N00_007_01_Accessory_Tie": ("NeckAccessory", "J_Bip_C_UpperChest"),
    "N00_007_02_Accessory_Tie": ("NeckAccessory", "J_Bip_C_UpperChest"),
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


def display_name(stem: str) -> str:
    value = stem.removeprefix("M_Accessory_").removeprefix("M_")
    return value.replace("_Accessory_", " ").replace("_", " ")


def main() -> None:
    package_root = find_package_root()
    source_root = package_root / "11_VRoid_Original_Parts" / "Accessory"
    if not SKELETON_PATH.exists():
        raise FileNotFoundError(SKELETON_PATH)
    items = []
    for obj_path in sorted(source_root.glob("*.obj")):
        stem = obj_path.stem.removeprefix("M_Accessory_").removeprefix("M_")
        rule = next(
            (value for key, value in CATEGORY_RULES.items() if key == stem),
            None,
        )
        if rule is None:
            raise RuntimeError(f"Unclassified VRoid accessory: {obj_path.name}")
        category, bone = rule
        safe_name = "".join(c if c.isalnum() else "_" for c in stem)
        item_root = OUTPUT_ROOT / category / safe_name
        item_root.mkdir(parents=True, exist_ok=True)
        items.append(
            {
                "category": category,
                "bone": bone,
                "label": display_name(obj_path.stem),
                "obj": str(obj_path),
                "output": str(item_root / f"SK_{category}_{safe_name}.fbx"),
            }
        )
    manifest_path = OUTPUT_ROOT / "accessory_manifest.json"
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(
        json.dumps(
            {"skeleton": str(SKELETON_PATH), "items": items},
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )
    if not BLENDER.exists():
        raise FileNotFoundError(BLENDER)
    exporter = Path(__file__).with_name("blender_export_vroid_accessory_catalog.py")
    creationflags = (
        subprocess.BELOW_NORMAL_PRIORITY_CLASS
        if sys.platform == "win32"
        else 0
    )
    subprocess.run(
        [str(BLENDER), "--background", "--python", str(exporter), "--", str(manifest_path)],
        cwd=str(PROJECT_ROOT),
        check=True,
        creationflags=creationflags,
    )


if __name__ == "__main__":
    main()
