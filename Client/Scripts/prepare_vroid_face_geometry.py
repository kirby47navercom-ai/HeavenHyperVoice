from pathlib import Path
import subprocess


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CATALOG_ROOT = PROJECT_ROOT / "Intermediate" / "VRoidHairCatalog"
OUTPUT_ROOT = PROJECT_ROOT / "Intermediate" / "VRoidFaceGeometry"
BLENDER = Path(r"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe")
EXPORTER = Path(__file__).with_name("blender_export_face_geometry.py")


def export_gender(gender: str, source_root: Path, skeleton_path: Path) -> None:
    obj_paths = sorted(source_root.glob("VRoidCombo_*.obj"))
    if len(obj_paths) != 1:
        raise RuntimeError(f"Expected one {gender} source OBJ in {source_root}, found {obj_paths}")
    if not skeleton_path.exists():
        raise FileNotFoundError(skeleton_path)
    output_dir = OUTPUT_ROOT / gender / "FBX"
    output_dir.mkdir(parents=True, exist_ok=True)
    flags = subprocess.CREATE_NO_WINDOW if hasattr(subprocess, "CREATE_NO_WINDOW") else 0
    subprocess.run(
        [
            str(BLENDER),
            "--background",
            "--python",
            str(EXPORTER),
            "--",
            str(obj_paths[0]),
            str(skeleton_path),
            str(output_dir),
        ],
        check=True,
        creationflags=flags,
    )
    required = ("Skin", "EyeWhite", "EyeIris", "EyeHighlight", "Brow", "Eyelash", "EyelineOverlay", "Mouth")
    missing = [semantic for semantic in required if not (output_dir / f"SK_{semantic}.fbx").exists()]
    if missing:
        raise RuntimeError(f"{gender} face export is missing {missing}")
    print(f"generated smooth {gender} face geometry")


def main() -> None:
    if not BLENDER.exists():
        raise FileNotFoundError(BLENDER)
    export_gender(
        "Male",
        CATALOG_ROOT / "Male" / "Style_2388",
        CATALOG_ROOT / "BaseHair" / "Male" / "Shared" / "VRoid_Skeleton.json",
    )
    export_gender(
        "Female",
        CATALOG_ROOT / "Style_2388",
        CATALOG_ROOT / "BaseHair" / "Female" / "Shared" / "VRoid_Skeleton.json",
    )


if __name__ == "__main__":
    main()
