from __future__ import annotations

import json
import sys
from pathlib import Path

import bpy
import numpy as np
from bpy_extras.io_utils import axis_conversion


def load_export_helpers(tool_root: Path):
    sys.path.insert(0, str(tool_root))
    from blender_export_unreal_skeletal import assign_weights, make_armature

    return assign_weights, make_armature


def material_values(material_path: Path) -> dict:
    data = json.loads(material_path.read_text(encoding="utf-8"))
    return next(iter(data.values())) if data else {}


def make_material(item: dict, values: dict):
    material = bpy.data.materials.new(
        name=f"M_{item['semantic']}_{item['gender']}_{item['style']}"
    )
    color = values.get("color", [1.0, 1.0, 1.0, 1.0])
    material.diffuse_color = tuple(float(value) for value in color)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    principled = next(
        (node for node in nodes if node.type == "BSDF_PRINCIPLED"), None
    )
    if principled is None:
        return material
    principled.inputs["Base Color"].default_value = material.diffuse_color
    principled.inputs["Roughness"].default_value = 0.72

    root = Path(item["materials"]).parent
    main_name = values.get("main_texture", "")
    if main_name:
        source = root / main_name
        if source.exists():
            image = bpy.data.images.load(str(source), check_existing=True)
            texture = nodes.new("ShaderNodeTexImage")
            texture.image = image
            links.new(texture.outputs["Color"], principled.inputs["Base Color"])

    normal_name = values.get("normal_texture", "")
    if normal_name:
        source = root / normal_name
        if source.exists():
            image = bpy.data.images.load(str(source), check_existing=True)
            image.colorspace_settings.name = "Non-Color"
            texture = nodes.new("ShaderNodeTexImage")
            texture.image = image
            normal = nodes.new("ShaderNodeNormalMap")
            normal.inputs["Strength"].default_value = float(
                values.get("normal_strength", 1.0)
            )
            links.new(texture.outputs["Color"], normal.inputs["Color"])
            links.new(normal.outputs["Normal"], principled.inputs["Normal"])
    return material


def remove_objects(objects) -> None:
    for obj in objects:
        mesh = obj.data
        bpy.data.objects.remove(obj, do_unlink=True)
        if mesh and mesh.users == 0:
            bpy.data.meshes.remove(mesh)


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1 :]
    manifest_path = Path(args[0]).resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    items = manifest.get("items", [])
    if not items:
        raise RuntimeError("Outfit manifest has no items")

    package_root = next(
        path
        for path in (Path.home() / "OneDrive").rglob(
            "Unreal_Customization_Package_20260808_215024"
        )
        if path.is_dir()
    )
    assign_weights, make_armature = load_export_helpers(
        package_root / "10_Customizer_EXE"
    )
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    conversion = axis_conversion(from_forward="-Z", from_up="Y").to_4x4()
    current_skeleton_path = None
    armature_object = None
    skeleton = None
    exported = 0

    for item in items:
        skeleton_path = Path(item["skeleton"])
        if current_skeleton_path != skeleton_path:
            if armature_object is not None:
                bpy.data.objects.remove(armature_object, do_unlink=True)
            skeleton = json.loads(skeleton_path.read_text(encoding="utf-8"))
            armature_object = make_armature(skeleton, conversion)
            current_skeleton_path = skeleton_path

        before = set(bpy.context.scene.objects)
        bpy.ops.wm.obj_import(
            filepath=str(Path(item["obj"]).resolve()),
            use_split_groups=True,
            use_split_objects=True,
        )
        imported = [
            obj
            for obj in bpy.context.scene.objects
            if obj not in before and obj.type == "MESH"
        ]
        if not imported:
            raise RuntimeError(f"OBJ import produced no mesh: {item['obj']}")

        values = material_values(Path(item["materials"]))
        material = make_material(item, values)
        skin = np.load(item["weights"])
        for obj in imported:
            obj.name = item["semantic"]
            obj.data.materials.clear()
            obj.data.materials.append(material)
            assign_weights(
                obj,
                item["semantic"],
                skin,
                skeleton,
                conversion,
                armature_object,
            )
        skin.close()

        target = Path(item["output"])
        target.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.object.select_all(action="DESELECT")
        armature_object.select_set(True)
        for obj in imported:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = armature_object
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
        remove_objects(imported)
        if material.users == 0:
            bpy.data.materials.remove(material)
        exported += 1
        print(
            f"OUTFIT_EXPORT {exported}/{len(items)} "
            f"{item['gender']} {item['category']} {item['style']}",
            flush=True,
        )

    report = manifest_path.with_name("outfit_export_report.json")
    report.write_text(
        json.dumps({"exported": exported}, indent=2), encoding="utf-8"
    )


if __name__ == "__main__":
    main()
