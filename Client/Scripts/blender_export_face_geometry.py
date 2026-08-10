from __future__ import annotations

import json
import sys
from pathlib import Path

import bpy
import bmesh
from bpy_extras.io_utils import axis_conversion
from mathutils import Matrix, Vector


SEMANTICS = (
    "EyeHighlight",
    "EyelineOverlay",
    "EyeWhite",
    "EyeIris",
    "EyeExtra",
    "Eyelash",
    "Mouth",
    "Brow",
    "Skin",
)


def semantic_for_object(name: str) -> str | None:
    normalized = name.replace(".", "_")
    for semantic in SEMANTICS:
        if f"_{semantic}_" in normalized or normalized.startswith(f"{semantic}_"):
            return semantic
    return None


def make_armature(skeleton: dict, conversion: Matrix):
    armature = bpy.data.armatures.new("VRoidSkeleton")
    armature_object = bpy.data.objects.new("VRoidSkeleton", armature)
    bpy.context.collection.objects.link(armature_object)
    bpy.context.view_layer.objects.active = armature_object
    armature_object.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    positions = {}
    children = {}
    for entry in skeleton["bones"]:
        matrix = conversion @ Matrix(entry["world_matrix"]) @ conversion.inverted()
        positions[entry["name"]] = matrix.translation
        children.setdefault(entry.get("parent"), []).append(entry["name"])
    edit_bones = {}
    for entry in skeleton["bones"]:
        name = entry["name"]
        bone = armature.edit_bones.new(name)
        head = positions[name]
        child_names = children.get(name, [])
        if child_names:
            direction = positions[child_names[0]] - head
        else:
            parent_name = entry.get("parent")
            direction = head - positions[parent_name] if parent_name in positions else Vector((0.0, 0.02, 0.0))
        if direction.length < 1e-5:
            direction = Vector((0.0, 0.02, 0.0))
        length = max(min(direction.length * 0.45, 0.08), 0.008)
        bone.head = head
        bone.tail = head + direction.normalized() * length
        edit_bones[name] = bone
    for entry in skeleton["bones"]:
        parent_name = entry.get("parent")
        if parent_name in edit_bones:
            edit_bones[entry["name"]].parent = edit_bones[parent_name]
    bpy.ops.object.mode_set(mode="OBJECT")
    armature_object.select_set(False)
    return armature_object


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1 :]
    obj_path, skeleton_path, output_dir = map(Path, args[:3])
    output_dir.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    bpy.ops.wm.obj_import(
        filepath=str(obj_path), use_split_groups=True, use_split_objects=True
    )
    conversion = axis_conversion(from_forward="-Z", from_up="Y").to_4x4()
    skeleton = json.loads(skeleton_path.read_text(encoding="utf-8"))
    armature_object = make_armature(skeleton, conversion)
    objects_by_semantic = {}

    for obj in list(bpy.context.scene.objects):
        if obj.type != "MESH":
            continue
        semantic = semantic_for_object(obj.name)
        if semantic is None:
            bpy.data.objects.remove(obj, do_unlink=True)
            continue
        if semantic == "EyeWhite":
            mesh = bmesh.new()
            mesh.from_mesh(obj.data)
            bmesh.ops.reverse_faces(mesh, faces=list(mesh.faces))
            mesh.to_mesh(obj.data)
            mesh.free()
            obj.data.update()
        for polygon in obj.data.polygons:
            polygon.use_smooth = True
        group = obj.vertex_groups.new(name="J_Bip_C_Head")
        group.add([vertex.index for vertex in obj.data.vertices], 1.0, "REPLACE")
        obj.parent = armature_object
        modifier = obj.modifiers.new(name="VRoidArmature", type="ARMATURE")
        modifier.object = armature_object
        objects_by_semantic.setdefault(semantic, []).append(obj)

    outputs = {}
    for semantic, objects in sorted(objects_by_semantic.items()):
        bpy.ops.object.select_all(action="DESELECT")
        armature_object.select_set(True)
        for obj in objects:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = armature_object
        target = output_dir / f"SK_{semantic}.fbx"
        bpy.ops.export_scene.fbx(
            filepath=str(target),
            use_selection=True,
            object_types={"ARMATURE", "MESH"},
            apply_unit_scale=True,
            apply_scale_options="FBX_SCALE_UNITS",
            axis_forward="-Z",
            axis_up="Y",
            add_leaf_bones=False,
            use_armature_deform_only=True,
            mesh_smooth_type="FACE",
            use_tspace=True,
            bake_anim=False,
            path_mode="COPY",
            embed_textures=False,
        )
        outputs[semantic] = target.name
    (output_dir / "Face_Export_Report.json").write_text(
        json.dumps({"modules": outputs, "bone_count": len(skeleton["bones"])}, indent=2),
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
