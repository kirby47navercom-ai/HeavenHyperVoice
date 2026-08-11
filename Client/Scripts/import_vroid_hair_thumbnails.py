from __future__ import annotations

from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
SOURCE_ROOT = PROJECT_ROOT / "Saved" / "Screenshots" / "Customization" / "HairOptionThumbnails"
DESTINATION = "/Game/CharacterCustomization/UI/HairThumbnails"
HAIR_INDICES = (0, 1, 3)
GENDERS = ("Male", "Female")


def import_texture(source: Path, asset_name: str) -> unreal.Texture2D:
    if not source.exists():
        raise FileNotFoundError(source)

    object_path = f"{DESTINATION}/{asset_name}.{asset_name}"
    existing = unreal.load_asset(object_path)
    if isinstance(existing, unreal.Texture2D):
        texture = existing
    else:
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DESTINATION
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = True
        task.save = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(object_path)

    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Failed to import hair thumbnail: {source}")

    texture.set_editor_property("srgb", True)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


def main() -> None:
    unreal.EditorAssetLibrary.make_directory(DESTINATION)
    imported = []
    for gender in GENDERS:
        for raw_index in HAIR_INDICES:
            source = SOURCE_ROOT / f"{gender}_Hair_{raw_index}.png"
            asset_name = f"T_{gender}_Hair_{raw_index}"
            texture = import_texture(source, asset_name)
            imported.append(texture.get_path_name())

    unreal.EditorAssetLibrary.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True)
    unreal.log_warning(
        "HAIR_THUMBNAIL_IMPORT imported="
        f"{len(imported)} destination={DESTINATION} assets={imported}"
    )


if __name__ == "__main__":
    main()
