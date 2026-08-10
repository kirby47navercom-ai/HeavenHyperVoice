import csv
import os
from pathlib import Path

import numpy as np
from PIL import Image


PACKAGE_ROOT = Path(
    r"C:\Users\kirby\OneDrive\바탕 화면\새 폴더"
    r"\Unreal_Customization_Package_20260808_215024"
)
FACE_ROOT = (
    PACKAGE_ROOT
    / "10_Customizer_EXE"
    / "Restored_Textures"
    / "VRoid_Face_Presets"
)
INDEX_PATH = FACE_ROOT / "face_preset_index.csv"
OUTPUT_ROOT = FACE_ROOT / "GeneratedCatalog" / "FaceSkin"
DEFAULT_SKIN_SRGB = np.array([239, 185, 151], dtype=np.float32)


def generate_skin_catalog() -> None:
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    selected: dict[str, Path] = {}
    with INDEX_PATH.open("r", encoding="utf-8-sig", newline="") as handle:
        for row in csv.DictReader(handle):
            if row.get("category") != "FaceSkin" or row.get("image_index", "0") != "0":
                continue
            style_id = row.get("style_id", "unknown") or "unknown"
            source = Path(row.get("file", ""))
            if source.exists() and style_id not in selected:
                selected[style_id] = source

    for style_id, source in selected.items():
        skin = np.array(
            Image.open(source).convert("RGBA").resize((1024, 1024), Image.Resampling.LANCZOS)
        )
        gray = skin[..., :3].astype(np.float32).mean(axis=2, keepdims=True)
        neutral = float(np.median(gray))
        modulation = np.clip(1.0 + ((gray - neutral) / 255.0) * 0.55, 0.78, 1.10)
        output = np.empty((1024, 1024, 4), dtype=np.uint8)
        output[..., :3] = np.clip(DEFAULT_SKIN_SRGB * modulation, 0, 255).astype(np.uint8)
        output[..., 3] = 255
        target = OUTPUT_ROOT / f"FaceSkin_{style_id}.png"
        Image.fromarray(output, "RGBA").save(target)
        print(target)


if __name__ == "__main__":
    generate_skin_catalog()
