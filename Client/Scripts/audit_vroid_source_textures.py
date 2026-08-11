from __future__ import annotations

import json
from collections import Counter
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_PATH = PROJECT_ROOT / "Saved" / "Diagnostics" / "VRoidSourceTextureAudit.txt"
IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".tga", ".bmp"}


def find_package_root() -> Path:
    for path in (Path.home() / "OneDrive").rglob("Unreal_Customization_Package_20260808_215024"):
        if path.is_dir():
            return path
    raise FileNotFoundError("Could not locate Unreal_Customization_Package_20260808_215024")


def material_values(material_path: Path) -> list[dict]:
    data = json.loads(material_path.read_text(encoding="utf-8"))
    return list(data.values())


def style_label(material_path: Path, package_root: Path) -> str:
    try:
        return str(material_path.relative_to(package_root))
    except ValueError:
        return str(material_path)


def main() -> int:
    package_root = find_package_root()
    clothing_root = package_root / "12_VRoid_Clothing_Presets"
    missing_with_local_images: list[str] = []
    missing_without_images: list[str] = []
    category_counts: Counter[str] = Counter()
    textured = 0
    total = 0

    for material_path in sorted(clothing_root.rglob("*.materials.json")):
        total += 1
        values = material_values(material_path)
        has_main_texture = any(value.get("main_texture") for value in values)
        has_normal_texture = any(value.get("normal_texture") for value in values)
        if has_main_texture:
            textured += 1
            continue

        category = material_path.parent.parent.name
        category_counts[category] += 1
        local_images = sorted(
            image.name
            for image in material_path.parent.iterdir()
            if image.is_file() and image.suffix.lower() in IMAGE_SUFFIXES
        )
        label = (
            f"{style_label(material_path, package_root)} "
            f"normal={'yes' if has_normal_texture else 'no'} "
            f"local_images={local_images}"
        )
        if local_images:
            missing_with_local_images.append(label)
        else:
            missing_without_images.append(label)

    lines = [
        "VRoid source texture audit",
        f"package={package_root}",
        f"materials={total}",
        f"with_main_texture={textured}",
        f"without_main_texture={len(missing_with_local_images) + len(missing_without_images)}",
        f"missing_by_category={dict(sorted(category_counts.items()))}",
        f"missing_with_local_images={len(missing_with_local_images)}",
        f"missing_without_local_images={len(missing_without_images)}",
    ]
    if missing_with_local_images:
        lines.append("")
        lines.append("Missing main_texture but local image candidates exist:")
        lines.extend(missing_with_local_images[:200])
    if missing_without_images:
        lines.append("")
        lines.append("Missing main_texture and no local image candidates:")
        lines.extend(missing_without_images[:200])

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines[:8]))
    return 1 if missing_with_local_images else 0


if __name__ == "__main__":
    raise SystemExit(main())
