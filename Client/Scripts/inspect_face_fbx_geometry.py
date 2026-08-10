from pathlib import Path
import sys

import bpy


def main() -> None:
    directory = Path(sys.argv[sys.argv.index("--") + 1])
    for fbx_path in sorted(directory.glob("SK_*.fbx")):
        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.object.delete(use_global=False)
        bpy.ops.import_scene.fbx(filepath=str(fbx_path))
        points = [
            obj.matrix_world @ vertex.co
            for obj in bpy.context.scene.objects
            if obj.type == "MESH"
            for vertex in obj.data.vertices
        ]
        if not points:
            print(f"FACE_FBX {fbx_path.stem} empty")
            continue
        minimum = tuple(min(point[index] for point in points) for index in range(3))
        maximum = tuple(max(point[index] for point in points) for index in range(3))
        normals = [
            (obj.matrix_world.to_3x3() @ polygon.normal).normalized()
            for obj in bpy.context.scene.objects
            if obj.type == "MESH"
            for polygon in obj.data.polygons
        ]
        average_normal = tuple(
            sum(normal[index] for normal in normals) / len(normals)
            for index in range(3)
        )
        print(
            f"FACE_FBX {fbx_path.stem} vertices={len(points)} "
            f"min={minimum} max={maximum} average_normal={average_normal}"
        )


if __name__ == "__main__":
    main()
