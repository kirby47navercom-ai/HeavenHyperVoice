from pathlib import Path
import sys

import bpy


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1 :]
    obj_path = Path(args[0])
    output_path = Path(args[1])
    output_path.parent.mkdir(parents=True, exist_ok=True)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    bpy.ops.wm.obj_import(
        filepath=str(obj_path), use_split_groups=True, use_split_objects=True
    )
    for obj in list(bpy.context.scene.objects):
        if obj.type == "MESH" and "Face_" not in obj.name:
            bpy.data.objects.remove(obj, do_unlink=True)

    camera_data = bpy.data.cameras.new("SourceFaceCamera")
    camera = bpy.data.objects.new("SourceFaceCamera", camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = (0.0, -0.50, 1.61)
    camera.rotation_euler = (1.57079633, 0.0, 0.0)
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = 0.30
    bpy.context.scene.camera = camera

    light_data = bpy.data.lights.new("SourceFaceLight", type="AREA")
    light = bpy.data.objects.new("SourceFaceLight", light_data)
    bpy.context.collection.objects.link(light)
    light.location = (0.0, -0.35, 1.61)
    light.rotation_euler = (1.57079633, 0.0, 0.0)
    light_data.energy = 250.0
    light_data.shape = "DISK"
    light_data.size = 0.4

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 768
    scene.render.resolution_y = 768
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(output_path)
    scene.world.color = (0.03, 0.03, 0.03)
    scene.view_settings.look = "AgX - Medium High Contrast"
    bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    main()
