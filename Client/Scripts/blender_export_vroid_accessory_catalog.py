from __future__ import annotations

import json
import os
import sys
from pathlib import Path

import bpy
from bpy_extras.io_utils import axis_conversion


def import_shared_helpers(project_root: Path):
    package_candidates = list(
        (Path.home() / "OneDrive").rglob(
            "Unreal_Customization_Package_20260808_215024/10_Customizer_EXE"
        )
    )
    if not package_candidates:
        raise FileNotFoundError("VRoid customizer tools were not found")
    sys.path.insert(0, str(package_candidates[0]))
    from blender_export_unreal_skeletal import make_armature

    return make_armature


def material_color(category: str):
    return {
        "HeadAccessory": (0.12, 0.055, 0.025, 1.0),
        "FaceAccessory": (0.025, 0.025, 0.025, 1.0),
        "EarAccessory": (0.30, 0.08, 0.045, 1.0),
        "TailAccessory": (0.30, 0.08, 0.045, 1.0),
        "NeckAccessory": (0.32, 0.035, 0.055, 1.0),
    }[category]


def import_accessory_mesh(item: dict) -> bpy.types.Object:
    before = set(bpy.context.scene.objects)
    split_for_submeshes = item["category"] == "FaceAccessory" and item["label"].startswith("Glasses")
    bpy.ops.wm.obj_import(
        filepath=str(Path(item["obj"]).resolve()),
        use_split_groups=split_for_submeshes,
        use_split_objects=split_for_submeshes,
    )
    meshes = [obj for obj in bpy.context.scene.objects if obj not in before and obj.type == "MESH"]
    if not meshes:
        raise RuntimeError(f"No mesh imported for {item['obj']}")

    if split_for_submeshes and len(meshes) > 1:
        # VRoid glasses are stored as frame + lens submeshes. The lens is a clear
        # accessory layer, so keep the largest frame mesh and drop the opaque lens
        # instead of baking it into the frame.
        meshes.sort(key=lambda obj: len(obj.data.vertices), reverse=True)
        frame = meshes[0]
        for lens in meshes[1:]:
            bpy.data.objects.remove(lens, do_unlink=True)
        return frame

    if len(meshes) > 1:
        bpy.ops.object.select_all(action="DESELECT")
        for obj in meshes:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        bpy.ops.object.join()
        return bpy.context.object

    return meshes[0]


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1 :]
    manifest_path = Path(args[0]).resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    project_root = manifest_path.parents[2]
    make_armature = import_shared_helpers(project_root)
    conversion = axis_conversion(from_forward="-Z", from_up="Y").to_4x4()
    skeleton = json.loads(Path(manifest["skeleton"]).read_text(encoding="utf-8"))
    label_filter = {
        label.strip()
        for label in os.environ.get("VROID_ACCESSORY_LABEL_FILTER", "").split(",")
        if label.strip()
    }
    items = [
        item for item in manifest["items"]
        if not label_filter or item["label"] in label_filter
    ]

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    armature = make_armature(skeleton, conversion)

    exported = 0
    for number, item in enumerate(items, start=1):
        mesh = import_accessory_mesh(item)
        mesh.name = item["category"]
        material = bpy.data.materials.new(name=f"M_{item['category']}")
        material.use_nodes = True
        principled = material.node_tree.nodes.get("Principled BSDF")
        principled.inputs["Base Color"].default_value = material_color(item["category"])
        principled.inputs["Roughness"].default_value = 0.65
        mesh.data.materials.clear()
        mesh.data.materials.append(material)

        group = mesh.vertex_groups.new(name=item["bone"])
        group.add([vertex.index for vertex in mesh.data.vertices], 1.0, "REPLACE")
        mesh.parent = armature
        modifier = mesh.modifiers.new(name="VRoidArmature", type="ARMATURE")
        modifier.object = armature

        output = Path(item["output"]).resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.object.select_all(action="DESELECT")
        armature.select_set(True)
        mesh.select_set(True)
        bpy.context.view_layer.objects.active = armature
        bpy.ops.export_scene.fbx(
            filepath=str(output),
            use_selection=True,
            object_types={"ARMATURE", "MESH"},
            apply_unit_scale=True,
            apply_scale_options="FBX_SCALE_UNITS",
            axis_forward="-Z",
            axis_up="Y",
            add_leaf_bones=False,
            use_armature_deform_only=True,
            bake_anim=False,
            path_mode="COPY",
            embed_textures=False,
        )
        bpy.data.objects.remove(mesh, do_unlink=True)
        bpy.data.materials.remove(material, do_unlink=True)
        exported += 1
        print(f"ACCESSORY_EXPORT {number}/{len(items)} {item['label']}", flush=True)

    report = manifest_path.with_name("accessory_export_report.json")
    report.write_text(json.dumps({"exported": exported}, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
