from __future__ import annotations

import json
import sys
from pathlib import Path

import bpy
import numpy as np
from bpy_extras.io_utils import axis_conversion
from mathutils import Matrix, Vector
from mathutils.kdtree import KDTree


SEMANTICS = ("HairBase", "HairFront", "HairSide", "HairBack", "HairExtra")


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


def assign_vroid_weights(obj, skin, skeleton: dict, conversion: Matrix) -> None:
    source_vertices = skin["HairBase__vertices"]
    source_weights = skin["HairBase__weights"]
    source_indices = skin["HairBase__bone_indices"]
    bone_names = [entry["name"] for entry in skeleton["bones"]]
    tree = KDTree(len(source_vertices))
    for index, value in enumerate(source_vertices):
        tree.insert(conversion @ Vector(tuple(float(axis) for axis in value)), index)
    tree.balance()
    groups = {}
    max_distance = 0.0
    for vertex in obj.data.vertices:
        point = obj.matrix_world @ vertex.co
        _nearest, source_index, distance = tree.find(point)
        max_distance = max(max_distance, float(distance))
        for influence in range(4):
            weight = float(source_weights[source_index, influence])
            if weight <= 1e-6:
                continue
            bone_name = bone_names[int(source_indices[source_index, influence])]
            group = groups.get(bone_name)
            if group is None:
                group = obj.vertex_groups.new(name=bone_name)
                groups[bone_name] = group
            group.add([vertex.index], weight, "REPLACE")
    if max_distance > 0.0001:
        raise RuntimeError(f"Base Hair weight transfer mismatch: {max_distance:.6f}")


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1 :]
    obj_path, skeleton_path, output_dir = map(Path, args[:3])
    mesh_id = args[3]
    skin_path = Path(args[4]) if len(args) > 4 else None
    output_dir.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    bpy.ops.wm.obj_import(filepath=str(obj_path), use_split_groups=True, use_split_objects=True)
    conversion = axis_conversion(from_forward="-Z", from_up="Y").to_4x4()
    skeleton = json.loads(skeleton_path.read_text(encoding="utf-8"))
    skin = np.load(skin_path) if skin_path and skin_path.exists() else None
    armature_object = make_armature(skeleton, conversion)
    objects_by_semantic = {}

    for obj in list(bpy.context.scene.objects):
        if obj.type != "MESH":
            continue
        semantic = semantic_for_object(obj.name)
        if semantic is None:
            bpy.data.objects.remove(obj, do_unlink=True)
            continue
        if semantic == "HairBase" and skin is not None:
            assign_vroid_weights(obj, skin, skeleton, conversion)
        else:
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
        target = output_dir / f"SK_{semantic}_{mesh_id}.fbx"
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
            bake_anim=False,
            path_mode="COPY",
            embed_textures=False,
        )
        outputs[semantic] = target.name

    report = {"mesh_id": mesh_id, "modules": outputs, "bone_count": len(skeleton["bones"])}
    (output_dir / "Hair_Export_Report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"
    )


if __name__ == "__main__":
    main()
