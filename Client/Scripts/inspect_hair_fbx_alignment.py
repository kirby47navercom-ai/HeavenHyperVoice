from __future__ import annotations

import json
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)


def bounds(objects) -> dict[str, list[float]]:
    points = [obj.matrix_world @ vertex.co for obj in objects for vertex in obj.data.vertices]
    minimum = [min(point[index] for point in points) for index in range(3)]
    maximum = [max(point[index] for point in points) for index in range(3)]
    return {"min": minimum, "max": maximum}


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1 :]
    obj_path, fbx_path, semantic = Path(args[0]), Path(args[1]), args[2]
    clear_scene()
    bpy.ops.wm.obj_import(
        filepath=str(obj_path), use_split_groups=True, use_split_objects=True
    )
    obj_meshes = [
        obj for obj in bpy.context.scene.objects
        if obj.type == "MESH" and semantic in obj.name.replace(".", "_")
    ]
    obj_names = [obj.name for obj in obj_meshes]
    obj_bounds = bounds(obj_meshes)

    clear_scene()
    bpy.ops.import_scene.fbx(filepath=str(fbx_path), automatic_bone_orientation=False)
    armatures = [obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"]
    for armature in armatures:
        armature.data.pose_position = "REST"
    bpy.context.view_layer.update()
    fbx_meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    fbx_rest_bounds = bounds(fbx_meshes)
    for armature in armatures:
        armature.data.pose_position = "POSE"
    bpy.context.view_layer.update()
    fbx_pose_bounds = bounds(fbx_meshes)
    report = {
        "semantic": semantic,
        "obj_objects": obj_names,
        "fbx_objects": [obj.name for obj in fbx_meshes],
        "obj": obj_bounds,
        "fbx_rest": fbx_rest_bounds,
        "fbx_pose": fbx_pose_bounds,
    }
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
