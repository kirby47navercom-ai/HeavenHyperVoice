from __future__ import annotations

import importlib.util
import io
import re
import struct
import subprocess
import sys
from dataclasses import replace
from pathlib import Path

import numpy as np
import UnityPy
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
OUTPUT_ROOT = Path(__file__).resolve().parents[1] / "Intermediate" / "VRoidHairCatalog" / "BaseHair"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
STYLE_ORDER = {"Default": 0, "001": 1, "Initial": 2}
# The BaseHair preset is a dark grayscale mask. This multiplier matches the
# visible mean of VRoid's default Hair001 texture before runtime color tinting.
HAIR_COLOR = np.array((104, 45, 23), dtype=np.float32)
BASE_HAIR_PART = "M_N00_000_00_HairBack.part"


def load_customizer():
    sys.path.insert(0, str(TOOL_ROOT))
    spec = importlib.util.spec_from_file_location("vroid_customizer", CUSTOMIZER_PATH)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def embedded_pngs(raw: bytes) -> list[bytes]:
    images: list[bytes] = []
    search_from = 0
    while True:
        start = raw.find(PNG_SIGNATURE, search_from)
        if start < 0:
            return images
        cursor = start + len(PNG_SIGNATURE)
        while cursor + 12 <= len(raw):
            length = struct.unpack(">I", raw[cursor : cursor + 4])[0]
            chunk_type = raw[cursor + 4 : cursor + 8]
            end = cursor + 12 + length
            if end > len(raw):
                break
            cursor = end
            if chunk_type == b"IEND":
                images.append(raw[start:cursor])
                break
        search_from = max(cursor, start + len(PNG_SIGNATURE))


def extract_styles() -> dict[int, Image.Image]:
    bundle = PACKAGE_ROOT.parent / "data.unity3d"
    env = UnityPy.load(str(bundle))
    transferables: dict[str, bytes] = {}
    groups: list[tuple[str, list[str]]] = []
    for obj in env.objects:
        if obj.type.name != "TextAsset":
            continue
        data = obj.read()
        raw = obj.get_raw_data()
        name = data.m_Name
        if name.endswith(".transferable"):
            transferables[name.split(".", 1)[0]] = raw
        elif name.endswith(".transferablegroup") and b"Level1.BaseHair" in raw:
            path_match = re.search(rb"pixiv/VRoid/Hair/N00/Base/([^/\x00]+)", raw)
            if not path_match:
                continue
            refs = sorted(set(re.findall(rb"([0-9a-f]{40})\.transferable", raw)))
            groups.append((path_match.group(1).decode("ascii"), [ref.decode("ascii") for ref in refs]))

    result: dict[int, Image.Image] = {}
    for style_name, refs in groups:
        if style_name not in STYLE_ORDER:
            continue
        png = next(
            (images[0] for ref in refs if (images := embedded_pngs(transferables.get(ref, b"")))),
            None,
        )
        if png is None:
            raise RuntimeError(f"Base Hair texture is missing for {style_name}")
        result[STYLE_ORDER[style_name]] = Image.open(io.BytesIO(png)).convert("RGBA")
    if set(result) != set(STYLE_ORDER.values()):
        raise RuntimeError(f"Expected three Base Hair presets, found {sorted(result)}")
    return result


def tint_texture(source: Image.Image) -> Image.Image:
    array = np.asarray(source, dtype=np.uint8).copy()
    luminance = array[..., :3].astype(np.float32).mean(axis=2, keepdims=True) / 255.0
    shade = 0.38 + 0.88 * luminance
    array[..., :3] = np.clip(HAIR_COLOR * shade, 0, 255).astype(np.uint8)
    return Image.fromarray(array, "RGBA")


def normal_from_texture(source: Image.Image) -> Image.Image:
    height = np.asarray(source.convert("L"), dtype=np.float32) / 255.0
    grad_y, grad_x = np.gradient(height)
    normal = np.dstack((-grad_x * 4.0, -grad_y * 4.0, np.ones_like(height)))
    normal /= np.maximum(np.linalg.norm(normal, axis=2, keepdims=True), 1e-6)
    encoded = np.clip((normal * 0.5 + 0.5) * 255.0, 0, 255).astype(np.uint8)
    alpha = np.full((*height.shape, 1), 255, dtype=np.uint8)
    return Image.fromarray(np.concatenate((encoded, alpha), axis=2), "RGBA")


def write_obj(style_root: Path, texture: Image.Image, mesh) -> Path:
    texture_path = style_root / "HairBase.png"
    normal_path = style_root / "HairBase_Normal.png"
    tint_texture(texture).save(texture_path)
    normal_from_texture(texture).save(normal_path)

    mtl_path = style_root / "HairBase.mtl"
    mtl_path.write_text(
        "newmtl HairBase\n"
        "Kd 1.000000 1.000000 1.000000\n"
        "d 1.000000\n"
        f"map_Kd {texture_path.name}\n"
        f"map_Bump -bm 0.350000 {normal_path.name}\n",
        encoding="ascii",
    )
    if len(mesh.vertices) != len(mesh.uvs):
        raise RuntimeError("Base Hair requires one VRoid UV per vertex")
    out_lines = [f"mtllib {mtl_path.name}", "o HairBase"]
    out_lines.extend(f"v {x:.9f} {y:.9f} {z:.9f}" for x, y, z in mesh.vertices)
    out_lines.extend(f"vt {u:.9f} {v:.9f}" for u, v in mesh.uvs)
    out_lines.extend(("g HairBase_N00_000_00_HairBack_00_HAIR", "usemtl HairBase"))
    for submesh in mesh.submeshes:
        indices = submesh["indices"]
        if len(indices) % 3:
            raise RuntimeError(f"Invalid Base Hair triangle index count: {len(indices)}")
        for index in range(0, len(indices), 3):
            triangle = [int(indices[index + offset]) + 1 for offset in range(3)]
            out_lines.append("f " + " ".join(f"{vertex}/{vertex}" for vertex in triangle))
    obj_path = style_root / "HairBase.obj"
    obj_path.write_text("\n".join(out_lines) + "\n", encoding="utf-8")
    return obj_path


def write_skin_weights(path: Path, mesh, skeleton_path: Path) -> None:
    skeleton = __import__("json").loads(skeleton_path.read_text(encoding="utf-8"))
    bone_lookup = {entry["name"]: index for index, entry in enumerate(skeleton["bones"])}
    missing = [name for name in mesh.bones if name not in bone_lookup]
    if missing:
        raise RuntimeError(f"Base Hair skeleton is missing VRoid bones: {missing}")
    local_to_global = np.asarray([bone_lookup[name] for name in mesh.bones], dtype=np.int32)
    np.savez(
        path,
        HairBase__vertices=np.asarray(mesh.vertices, dtype=np.float32),
        HairBase__weights=np.asarray(mesh.weights, dtype=np.float32),
        HairBase__bone_indices=local_to_global[np.asarray(mesh.bone_indices, dtype=np.int32)],
    )


def generate() -> None:
    if not BLENDER.exists():
        raise FileNotFoundError(BLENDER)
    customizer = load_customizer()
    assets = customizer.AssetModel(PACKAGE_ROOT)
    if assets.deformer is None:
        raise RuntimeError("VRoid deformation data is missing")
    exporter = Path(__file__).with_name("blender_export_hair_catalog.py")
    flags = subprocess.CREATE_NO_WINDOW if hasattr(subprocess, "CREATE_NO_WINDOW") else 0

    styles = extract_styles()
    for gender in ("Female", "Male"):
        root = assets.preferred_root_for_gender(gender)
        combo = replace(
            assets.combo_for_root(root),
            body_gender=gender,
            body_shape={},
            clothing_presets={},
        )
        shared = OUTPUT_ROOT / gender / "Shared"
        shared.mkdir(parents=True, exist_ok=True)
        _skin_path, skeleton_path = customizer.write_unreal_skin_data(
            assets, combo, shared
        )
        male = gender == "Male"
        parameters = assets.deformer.parameter_values(gender, {})
        mesh = assets.deformer.deform_part(
            BASE_HAIR_PART,
            {"BlendShape.N00_BaseHair.Male": 100.0 if male else 0.0},
            parameters,
        )
        bounds = np.asarray(mesh.vertices, dtype=np.float32)
        print(f"{gender} Base Hair bounds={bounds.min(axis=0)}..{bounds.max(axis=0)}")
        for style, texture in sorted(styles.items()):
            style_root = OUTPUT_ROOT / gender / f"Style_{style}"
            output_dir = style_root / "FBX"
            mesh_id = f"{gender}_{style}"
            expected = output_dir / f"SK_HairBase_{mesh_id}.fbx"
            style_root.mkdir(parents=True, exist_ok=True)
            obj_path = write_obj(style_root, texture, mesh)
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
                    mesh_id,
                ],
                cwd=str(TOOL_ROOT),
                check=True,
                creationflags=flags,
            )
            if not expected.exists():
                raise RuntimeError(f"Base Hair FBX was not generated: {expected}")
            print(f"generated {gender} Base Hair {style}")


if __name__ == "__main__":
    generate()
