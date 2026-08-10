import csv
import re
from pathlib import Path

import unreal


DESTINATION_ROOT = "/Game/VRoidCatalog/Face"

CATEGORY_DESTINATIONS = {
    "FaceSkin": "Skin",
    "EyeWhite": "Scleras",
    "Iris": "Irises",
    "EyeHighlight": "EyeHighlights",
    "Eyebrow": "Eyebrows",
    "Eyelash": "Eyelashes",
    "Eyeline": "Eyeliner",
    "Mouth": "MouthInside",
    "Lip": "Lips",
    "MouthLine": "MouthLines",
}


def find_face_root() -> Path:
    search_root = Path.home() / "OneDrive"
    matches = [
        path.parent
        for path in search_root.rglob("face_preset_index.csv")
        if "Unreal_Customization_Package_20260808_215024" in str(path)
    ]
    if not matches:
        raise FileNotFoundError("Could not locate the extracted VRoid face preset index")
    return matches[0]


FACE_ROOT = find_face_root()
FACE_INDEX = FACE_ROOT / "face_preset_index.csv"


def clean_name(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", value).strip("_") or "Unknown"


def unique_face_presets() -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    with FACE_INDEX.open("r", encoding="utf-8-sig", newline="") as handle:
        rows.extend(csv.DictReader(handle))

    normals = {
        (row.get("category", ""), row.get("group_hash", "")): row
        for row in rows
        if row.get("category") == "FaceSkin" and row.get("image_index") == "1"
    }
    selected: dict[tuple[str, str], dict[str, str]] = {}
    for row in rows:
        category = row.get("category", "")
        if category not in CATEGORY_DESTINATIONS or row.get("image_index", "0") != "0":
            continue
        style_id = row.get("style_id", "unknown") or "unknown"
        source = Path(row.get("file", ""))
        if not source.exists():
            source = FACE_ROOT / category / source.name
        if not source.exists():
            unreal.log_warning(f"Missing VRoid preset texture: {source}")
            continue
        key = (category, style_id)
        if key in selected and str(source) >= selected[key]["source"]:
            continue
        normal_row = normals.get((category, row.get("group_hash", "")))
        normal_source = Path(normal_row.get("file", "")) if normal_row else None
        selected[key] = {
            "category": category,
            "style_id": style_id,
            "vroid_path": row.get("vroid_path", ""),
            "group_hash": row.get("group_hash", ""),
            "transferable_hash": row.get("transferable_hash", ""),
            "source": str(source),
            "normal_source": str(normal_source) if normal_source and normal_source.exists() else "",
        }
    return sorted(
        selected.values(),
        key=lambda item: (
            item["category"],
            not item["style_id"].isdigit(),
            int(item["style_id"]) if item["style_id"].isdigit() else item["vroid_path"],
        ),
    )


def add_import_task(tasks, source: str, destination: str, asset_name: str) -> None:
    task = unreal.AssetImportTask()
    task.filename = source
    task.destination_path = destination
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    tasks.append(task)


def import_face_presets() -> None:
    presets = unique_face_presets()
    tasks: list[unreal.AssetImportTask] = []
    for preset in presets:
        category = preset["category"]
        style_id = clean_name(preset["style_id"])
        generated_skin = FACE_ROOT / "GeneratedCatalog" / "FaceSkin" / f"FaceSkin_{preset['style_id']}.png"
        diffuse_source = str(generated_skin) if category == "FaceSkin" and generated_skin.exists() else preset["source"]
        destination = f"{DESTINATION_ROOT}/{CATEGORY_DESTINATIONS[category]}"
        add_import_task(tasks, diffuse_source, destination, f"T_{category}_{style_id}")
        if preset["normal_source"]:
            add_import_task(tasks, preset["normal_source"], destination, f"T_{category}_{style_id}_N")

    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    imported = 0
    for preset in presets:
        category = preset["category"]
        style_id = clean_name(preset["style_id"])
        destination = f"{DESTINATION_ROOT}/{CATEGORY_DESTINATIONS[category]}"
        for suffix, role in (("", "MainImage"), ("_N", "NormalMapImage")):
            if suffix and not preset["normal_source"]:
                continue
            texture = unreal.load_asset(f"{destination}/T_{category}_{style_id}{suffix}")
            if not isinstance(texture, unreal.Texture2D):
                unreal.log_warning(f"Failed to import {category} {style_id} {role}")
                continue
            is_normal = role == "NormalMapImage"
            texture.set_editor_property("srgb", not is_normal)
            texture.set_editor_property(
                "compression_settings",
                unreal.TextureCompressionSettings.TC_NORMALMAP
                if is_normal
                else unreal.TextureCompressionSettings.TC_DEFAULT,
            )
            texture.set_editor_property(
                "lod_group",
                unreal.TextureGroup.TEXTUREGROUP_CHARACTER_NORMAL_MAP
                if is_normal
                else unreal.TextureGroup.TEXTUREGROUP_CHARACTER,
            )
            unreal.EditorAssetLibrary.set_metadata_tag(texture, "VRoidCategory", category)
            unreal.EditorAssetLibrary.set_metadata_tag(texture, "VRoidStyleId", preset["style_id"])
            unreal.EditorAssetLibrary.set_metadata_tag(texture, "VRoidPath", preset["vroid_path"])
            unreal.EditorAssetLibrary.set_metadata_tag(texture, "VRoidGroupHash", preset["group_hash"])
            unreal.EditorAssetLibrary.set_metadata_tag(texture, "VRoidMapRole", role)
            unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
            imported += 1

    unreal.EditorAssetLibrary.save_directory("/Game/VRoidCatalog")
    unreal.log(f"VRoid catalog ready: {imported} source-mapped textures")


import_face_presets()
