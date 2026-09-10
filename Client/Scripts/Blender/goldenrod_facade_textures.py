"""Window appearance on approved wall polygons. Adds UVs/materials, never geometry."""
import math

import bpy
from mathutils import Vector


def apply_facade(ob, base, texture_dir, index):
    building = ob.get('building_id', '')
    name = ob.name
    crown = building == 'RadioTower' and 'BroadcastCrown' in name
    vault = building == 'Station' and 'Vault' in name
    entry = 'Entry' in name
    if not crown and not vault and any(s in name for s in ('Roof', 'Vault', 'Shoulder', 'Mast', 'Terrace', 'Spire', 'Canopy')):
        return None
    if building == 'ShoppingArcade':
        return None
    points = [ob.matrix_world @ Vector(v) for v in ob.bound_box]
    low, high = min(p.z for p in points), max(p.z for p in points)
    if high - low < 4:
        return None
    cx = (min(p.x for p in points) + max(p.x for p in points)) / 2
    cy = (min(p.y for p in points) + max(p.y for p in points)) / 2
    width = max(p.x for p in points) - min(p.x for p in points)
    depth = max(p.y for p in points) - min(p.y for p in points)
    hero = bool(ob.get('landmark')) or building == 'GlobalTerminal'
    commercial = hero or ob.get('district') not in ('D7', 'D9') or index % 4 == 0
    storefront = (low < .1 and commercial and not ob.get('context_only')) or entry or crown
    start = 5.8 if low < .1 else low + 2
    step = 3.1 if building == 'PokemonCenter' else 3.6

    # Shared blockout meshes need independent UV layers, but retain every vertex/face.
    if ob.data.users > 1:
        ob.data = ob.data.copy()
    mesh = ob.data
    layers = {key: mesh.uv_layers.get(key) or mesh.uv_layers.new(name=key)
              for key in ('FacadeUV', 'StoreUV', 'FacadePosition', 'FacadeSurface')}
    for face in mesh.polygons:
        normal = ob.matrix_world.to_3x3() @ face.normal
        side = abs(normal.z) < .25 and (max(abs(normal.x), abs(normal.y)) > .9 or crown)
        if vault:
            side = abs(normal.y) > .99
        use_x = abs(normal.y) >= abs(normal.x)
        face_points = [ob.matrix_world @ mesh.vertices[i].co for i in face.vertices]
        face_u = [(p.x - cx) if use_x else (p.y - cy) for p in face_points]
        face_mid = (max(face_u) + min(face_u)) / 2
        across = max(max(face_u) - min(face_u), .001)
        count = max(1, math.floor((across - 3) / (5 if ob.get('context_only') else 4)))
        shop_count = max(1, math.floor(across / 5))
        angles = []
        if crown:
            angles = [math.atan2((ob.matrix_world @ mesh.vertices[mesh.loops[li].vertex_index].co).y - cy,
                                 (ob.matrix_world @ mesh.vertices[mesh.loops[li].vertex_index].co).x - cx)
                      for li in face.loop_indices]
            if max(angles) - min(angles) > math.pi:
                angles = [a + 2 * math.pi if a < 0 else a for a in angles]
        for j, li in enumerate(face.loop_indices):
            p = ob.matrix_world @ mesh.vertices[mesh.loops[li].vertex_index].co
            u = (p.x - cx) if use_x else (p.y - cy)
            regular_uv = (((u - face_mid) / across + .5) * count, (p.z - start) / step + .5)
            store_uv = (((u - face_mid) / across + .5) * shop_count, (p.z - low) / (high - low if entry else 4.4))
            if crown:
                store_uv = (angles[j] / (2 * math.pi) * 24, (p.z - low) / (high - low))
            if vault:
                regular_uv = ((p.x - cx) / 34 + .5, (p.z - 21.8) / 6 + .5)
            layers['FacadeUV'].data[li].uv = regular_uv
            layers['StoreUV'].data[li].uv = store_uv
            layers['FacadePosition'].data[li].uv = (u, p.z)
            layers['FacadeSurface'].data[li].uv = (1 if side else 0, count)

    mat = base.copy()
    mat.name = 'M_GR_Facade_' + name.removeprefix('SM_GR_')
    mat['window_representation'] = 'RGBA facade textures on approved polygons; no window geometry'
    mat['window_transmission'] = 0.0
    mat['window_shader'] = 'Opaque dielectric with texture roughness, normal and reflective clear coat'
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    out = next(n for n in nodes if n.type == 'OUTPUT_MATERIAL')
    surface = out.inputs['Surface'].links[0].from_socket

    def math_node(op, a, b=None):
        node = nodes.new('ShaderNodeMath'); node.operation = op
        for i, val in enumerate((a, b)):
            if val is None:
                continue
            if isinstance(val, (float, int)):
                node.inputs[i].default_value = val
            else:
                links.new(val, node.inputs[i])
        return node.outputs[0]

    def uv(name):
        node = nodes.new('ShaderNodeUVMap'); node.uv_map = name
        return node.outputs['UV']

    position = nodes.new('ShaderNodeSeparateXYZ'); links.new(uv('FacadePosition'), position.inputs[0])
    guard = nodes.new('ShaderNodeSeparateXYZ'); links.new(uv('FacadeSurface'), guard.inputs[0])
    z = position.outputs['Y']

    def layer(family, uv_name, mask):
        nonlocal surface
        channels = {}
        coord = uv(uv_name)
        for channel in ('BaseColor', 'Roughness', 'Normal', 'GlassMask'):
            path = texture_dir / 'Facades' / (family + '_' + channel + '.png')
            if not path.exists():
                raise FileNotFoundError(path)
            im = bpy.data.images.load(str(path), check_existing=True)
            im.colorspace_settings.name = 'sRGB' if channel == 'BaseColor' else 'Non-Color'
            im.alpha_mode = 'STRAIGHT'
            if not im.packed_file:
                im.pack()
            tex = nodes.new('ShaderNodeTexImage'); tex.image = im
            tex.label = family + ' / ' + channel; tex.extension = 'REPEAT'
            links.new(coord, tex.inputs['Vector']); channels[channel] = tex
        bs = nodes.new('ShaderNodeBsdfPrincipled'); bs.label = 'Texture window / opaque reflective glazing'
        links.new(channels['BaseColor'].outputs['Color'], bs.inputs['Base Color'])
        links.new(channels['Roughness'].outputs['Color'], bs.inputs['Roughness'])
        links.new(math_node('SUBTRACT', 1, channels['GlassMask'].outputs['Color']), bs.inputs['Metallic'])
        links.new(math_node('MULTIPLY', channels['GlassMask'].outputs['Color'], .45), bs.inputs['Coat Weight'])
        bs.inputs['Coat Roughness'].default_value = .12
        bs.inputs['IOR'].default_value = 1.5
        bs.inputs['Transmission Weight'].default_value = 0
        normal = nodes.new('ShaderNodeNormalMap'); normal.uv_map = uv_name
        normal.inputs['Strength'].default_value = .35
        links.new(channels['Normal'].outputs['Color'], normal.inputs['Color'])
        links.new(normal.outputs[0], bs.inputs['Normal'])
        coverage = math_node('MULTIPLY', channels['BaseColor'].outputs['Alpha'], mask)
        mix = nodes.new('ShaderNodeMixShader')
        links.new(coverage, mix.inputs[0]); links.new(surface, mix.inputs[1]); links.new(bs.outputs[0], mix.inputs[2])
        surface = mix.outputs[0]

    if vault:
        mask = math_node('MULTIPLY', guard.outputs['X'], math_node('GREATER_THAN', z, 19.9))
        mask = math_node('MULTIPLY', mask, math_node('LESS_THAN', z, 23.7))
        layer('station_round', 'FacadeUV', mask)
    elif not entry and not crown:
        minimum = start - step / 2 + .06
        # End between rows so a top window is never cut in half by the roof.
        half_pane = 2.35 / 3.6 * step / 2
        last_row = math.floor((high - .45 - half_pane - start) / step)
        maximum = start + last_row * step + half_pane + .08
        mask = math_node('MULTIPLY', guard.outputs['X'], math_node('GREATER_THAN', z, minimum))
        mask = math_node('MULTIPLY', mask, math_node('LESS_THAN', z, maximum))
        if building == 'PokemonCenter':
            bays = nodes.new('ShaderNodeSeparateXYZ'); links.new(uv('FacadeUV'), bays.inputs[0])
            bay_center = math_node('ADD', math_node('FLOOR', bays.outputs['X']), .5)
            center_distance = math_node('SUBTRACT', bay_center, math_node('MULTIPLY', guard.outputs['Y'], .5))
            # Omit complete middle bays behind the clinic symbol, never half a window.
            mask = math_node('MULTIPLY', mask, math_node('GREATER_THAN', math_node('ABSOLUTE', center_distance), .75))
        layer('window_hero' if hero else 'window_regular', 'FacadeUV', mask)
    if storefront:
        mask = guard.outputs['X']
        if not crown and not entry:
            mask = math_node('MULTIPLY', mask, math_node('LESS_THAN', z, 4.15))
            mask = math_node('MULTIPLY', mask, math_node('GREATER_THAN', z, .1))
        layer('storefront', 'StoreUV', mask)
    links.new(surface, out.inputs['Surface'])
    ob.material_slots[0].link = 'OBJECT'; ob.material_slots[0].material = mat
    ob['facade_texture_only'] = True
    ob['lookdev_material'] = mat.name
    return mat
