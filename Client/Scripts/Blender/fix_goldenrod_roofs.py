"""Apply the R03 roof/viewport correction to the verified R02 native model."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import bpy

SCRIPT = Path(__file__).parent / 'rebuild_goldenrod_reference_city.py'
spec = importlib.util.spec_from_file_location('goldenrod_builder', SCRIPT)
city = importlib.util.module_from_spec(spec)
spec.loader.exec_module(city)
OLD = city.OUT
OUT = OLD / 'R03'
OUT.mkdir(exist_ok=True)
(OUT / 'Textures').mkdir(exist_ok=True)
(OUT / 'Unreal').mkdir(exist_ok=True)
city.OUT = OUT
city.EXPORT = OUT / 'Unreal'


def geometry_digest():
    digest = hashlib.sha256()
    for ob in sorted((o for o in bpy.context.scene.objects if o.type == 'MESH'), key=lambda o:o.name):
        digest.update(ob.name.encode())
        digest.update(str([list(row) for row in ob.matrix_world]).encode())
        digest.update(str([tuple(v.co) for v in ob.data.vertices]).encode())
        digest.update(str([tuple(p.vertices) for p in ob.data.polygons]).encode())
        digest.update(str([tuple(u.uv) for u in ob.data.uv_layers.active.data]).encode())
    return digest.hexdigest()


def fix():
    before = geometry_digest()
    source = bpy.data.filepath
    source_sha = hashlib.sha256(Path(source).read_bytes()).hexdigest()
    city.MATS.update({m['texture_prefix']:m for m in bpy.data.materials if m.get('texture_prefix')})
    teal = city.material('RoofTeal')
    changes = []
    for ob in bpy.context.scene.objects:
        if ob.type != 'MESH' or ob.name == 'SM_R02_Ground':
            continue
        for slot in ob.material_slots:
            if slot.material and slot.material.get('texture_prefix') == 'GoldenSidewalk':
                target = city.MATS['RoofRed'] if ob.name == 'SM_R02_DepartmentStore' else teal
                changes.append({'object':ob.name,'old_material':slot.material.name,'new_material':target.name})
                slot.material = target
    assert len(changes) == 12, changes
    after = geometry_digest()
    assert before == after, 'This correction must preserve coordinates, topology and UVs'
    # Large-city inspection uses a useful near plane, instead of the default 1cm.
    for screen in bpy.data.screens:
        for area in screen.areas:
            for space in area.spaces:
                if space.type == 'VIEW_3D':
                    space.clip_start = .5
                    space.clip_end = 2000
                    space.overlay.show_extras = False
                    space.shading.type = 'MATERIAL'
                    space.region_3d.view_distance = 460
                    space.region_3d.view_location = (-35,-18,0)
    bpy.context.scene.eevee.taa_samples = 64
    # Reduce the thin seam's specular contribution; retain the original image maps.
    normal_strengths = {'RoofRed':.12,'RoofTeal':.12,'RoofCream':.12,
                        'Promenade':.18,'GoldenSidewalk':.18,'PaleKerb':.18}
    records = []
    used = set(m for ob in bpy.context.scene.objects if ob.type == 'MESH' for m in ob.data.materials if m)
    for mat in sorted(used,key=lambda m:m.name):
        prefix = mat['texture_prefix']
        normal = next(n for n in mat.node_tree.nodes if n.type == 'NORMAL_MAP')
        if prefix in normal_strengths:
            normal.inputs['Strength'].default_value = normal_strengths[prefix]
        textures = {}
        for channel in ('BaseColor','Roughness','Normal','Metallic'):
            src = OLD / 'Textures' / f'{prefix}_{channel}.png'
            dst = OUT / 'Textures' / src.name
            shutil.copy2(src,dst)
            textures[channel] = 'Textures/' + dst.name
            for node in mat.node_tree.nodes:
                if node.type == 'TEX_IMAGE' and node.image.name == src.name:
                    node.image.filepath = str(dst)
        bs = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        records.append({'material':mat.name,'prefix':prefix,'tile_m':mat['tile_m'],
                        'emission':mat['emission_strength'],'normal_strength':normal.inputs['Strength'].default_value,
                        'coat_weight':bs.inputs['Coat Weight'].default_value,'textures':textures})
    bpy.context.scene.name = 'Goldenrod_Reference_R03'
    bpy.context.scene['revision'] = 'R03: roof materials corrected; large-city viewport clip .5m; restrained seam normals'
    bpy.ops.object.select_all(action='DESELECT')
    for im in bpy.data.images:
        if im.source == 'FILE': im.pack()
    (OUT/'Goldenrod_R03_Materials.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
    native = OUT / 'Goldenrod_City_R03.blend'
    bpy.ops.wm.save_as_mainfile(filepath=str(native),compress=True)
    report = {'source':source,'source_sha256':source_sha,'native':str(native),
              'native_sha256':hashlib.sha256(native.read_bytes()).hexdigest(),
              'geometry_and_uv_digest_before':before,'geometry_and_uv_digest_after':after,
              'geometry_and_uv_unchanged':before==after,'roof_changes':changes,
              'mesh_count':len([o for o in bpy.context.scene.objects if o.type=='MESH']),
              'material_count':len(records),'texture_count':len(records)*4,
              'viewport_clip_start_m':.5,'viewport_clip_end_m':2000,
              'viewport_shading':'MATERIAL','eevee_viewport_samples':64,
              'normal_strengths':normal_strengths,
              'temporal_shimmer_fully_resolved':False}
    (OUT/'Goldenrod_R03_Fix_QA.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('R03_FIX',json.dumps(report),flush=True)
    city.export()


if '--render' in sys.argv:
    city.render()
else:
    fix()
