from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "Intermediate" / "VRoidFaceGeometry"
SKELETON_PATH = "/Game/VRoidGenerated/SK_BodySkin_Skeleton"
SEMANTICS = (
    "Skin",
    "EyeWhite",
    "EyeIris",
    "EyeHighlight",
    "EyeExtra",
    "Brow",
    "Eyelash",
    "EyelineOverlay",
    "Mouth",
)


def import_skeletal(path: Path, destination: str, skeleton: unreal.Skeleton):
    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = destination
    task.destination_name = path.stem
    task.automated = True
    task.replace_existing = True
    task.save = True
    options = unreal.FbxImportUI()
    options.import_as_skeletal = True
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.skeleton = skeleton
    options.skeletal_mesh_import_data.normal_import_method = (
        unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    )
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(f"{destination}/{path.stem}")


def run() -> None:
    skeleton = unreal.load_asset(SKELETON_PATH)
    if not isinstance(skeleton, unreal.Skeleton):
        raise RuntimeError(f"Missing common VRoid skeleton: {SKELETON_PATH}")
    imported = 0
    for gender, destination in (
        ("Male", "/Game/VRoidGenerated"),
        ("Female", "/Game/VRoidGenerated/Female"),
    ):
        for semantic in SEMANTICS:
            fbx = SOURCE_ROOT / gender / "FBX" / f"SK_{semantic}.fbx"
            if not fbx.exists():
                raise FileNotFoundError(fbx)
            if import_skeletal(fbx, destination, skeleton) is None:
                raise RuntimeError(f"Failed to import {fbx}")
            imported += 1
        unreal.EditorAssetLibrary.save_directory(destination)
    unreal.log(f"VRoid face geometry ready: {imported} smooth gender-specific submeshes")


if __name__ == "__main__":
    run()
