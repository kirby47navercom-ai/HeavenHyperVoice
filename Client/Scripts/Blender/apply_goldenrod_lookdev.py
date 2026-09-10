"""Goldenrod L01: textured facades, street color, image-based PBR and Cycles review.

blender -b Goldenrod_Graybox.blend --python this.py
blender -b Lookdev/Goldenrod_ColorStudy.blend --python this.py -- --render 02,04,05,09,11
The approved source scene and city masterplan are never overwritten.
"""
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
import bmesh
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from goldenrod_facade_textures import apply_facade

ROOT = Path(__file__).resolve().parents[2]
CITY = ROOT / 'SourceArt/Environments/Goldenrod'
OUT = CITY / 'Lookdev'
TEX = OUT / 'Textures'
PLAN = json.loads((CITY / 'Goldenrod_Masterplan.json').read_text(encoding='utf-8'))
M = {}
BATCHES = {}


def rgba(code):
    values = [int(code.lstrip('#')[i:i+2], 16)/255 for i in (0, 2, 4)]
    return tuple(v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4 for v in values)+(1,)


def material(key, color, family='concrete', roughness=.6, metallic=0, transmission=0,
             emission=0, coat=0):
    mat = bpy.data.materials.new('M_GR_'+key)
    mat.use_nodes = True
    mat.diffuse_color = rgba(color)
    mat['base_color_srgb'] = color
    mat['surface_family'] = family or 'optical / uniform manufactured surface'
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    bs = nodes.new('ShaderNodeBsdfPrincipled'); bs.location = (530, 80)
    bs.inputs['Base Color'].default_value = rgba(color)
    bs.inputs['Roughness'].default_value = roughness
    bs.inputs['Metallic'].default_value = metallic
    bs.inputs['IOR'].default_value = 1.5
    bs.inputs['Transmission Weight'].default_value = transmission
    bs.inputs['Coat Weight'].default_value = coat
    bs.inputs['Coat Roughness'].default_value = .25
    bs.inputs['Emission Color'].default_value = rgba(color)
    bs.inputs['Emission Strength'].default_value = emission
    output = nodes.new('ShaderNodeOutputMaterial'); output.location = (850, 80)
    links.new(bs.outputs['BSDF'], output.inputs['Surface'])
    if family:
        uv=nodes.new('ShaderNodeTexCoord');uv.location=(-1000,200)
        scale=nodes.new('ShaderNodeVectorMath');scale.operation='SCALE';scale.location=(-800,200)
        tile=3.84 if family=='brick' else 4
        scale.inputs[3].default_value=4/tile
        links.new(uv.outputs['UV'],scale.inputs[0])
        for index, channel in enumerate(('BaseColor', 'Roughness', 'Normal', 'Metallic')):
            path = TEX / f'{family}_{channel}.png'
            if not path.exists():
                raise FileNotFoundError(path)
            image = bpy.data.images.load(str(path), check_existing=True)
            image.colorspace_settings.name = 'sRGB' if channel == 'BaseColor' else 'Non-Color'
            image.pack()
            tex = nodes.new('ShaderNodeTexImage'); tex.image = image
            tex.label = f'{channel} / tileable 4m'; tex.location = (-500, 450-index*270)
            tex.extension = 'REPEAT'
            links.new(scale.outputs[0],tex.inputs['Vector'])
            if channel == 'BaseColor':
                tint = nodes.new('ShaderNodeMixRGB'); tint.blend_type = 'MULTIPLY'
                tint.inputs[0].default_value = 1
                tint.inputs[2].default_value = rgba(color); tint.location = (40, 440)
                links.new(tex.outputs['Color'], tint.inputs[1])
                links.new(tint.outputs[0], bs.inputs['Base Color'])
            elif channel == 'Normal':
                normal = nodes.new('ShaderNodeNormalMap'); normal.inputs['Strength'].default_value = .35
                normal.location = (30, -90)
                links.new(tex.outputs['Color'], normal.inputs['Color'])
                links.new(normal.outputs[0], bs.inputs['Normal'])
            elif channel == 'Roughness':
                mult = nodes.new('ShaderNodeMath'); mult.operation = 'MULTIPLY'; mult.use_clamp = True
                mult.inputs[1].default_value = roughness/.6; mult.location = (30, 190)
                links.new(tex.outputs['Color'], mult.inputs[0])
                links.new(mult.outputs[0], bs.inputs['Roughness'])
            else:
                if metallic == 0:links.new(tex.outputs['Color'], bs.inputs['Metallic'])
        mat['texture_tile_m'] = tile
    M[key] = mat
    return mat


def assign(ob, key):
    # Per-object slots preserve shared geometry with independent district colors.
    if not ob.data.materials:ob.data.materials.append(M[key])
    ob.material_slots[0].link = 'OBJECT'; ob.material_slots[0].material = M[key]
    ob.color = M[key].diffuse_color
    ob['lookdev_material'] = key


def uv_project(mesh, tile=4):
    uv = mesh.uv_layers.get('UVMap') or mesh.uv_layers.new(name='UVMap')
    for face in mesh.polygons:
        n = face.normal
        axis = max(range(3), key=lambda i: abs(n[i]))
        for li in face.loop_indices:
            p = mesh.vertices[mesh.loops[li].vertex_index].co
            if axis == 2:u, v = p.x, p.y
            elif axis == 1:u, v = p.x * (-1 if n.y > 0 else 1), p.z
            else:u, v = p.y * (1 if n.x > 0 else -1), p.z
            uv.data[li].uv = (u/tile, v/tile)


def mesh_object(name, verts, faces, mat, collection, origin=(0, 0, 0)):
    mesh = bpy.data.meshes.new(name+'_Geo'); mesh.from_pydata(verts, [], faces); mesh.update()
    bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
    ob = bpy.data.objects.new(name, mesh); bpy.data.collections[collection].objects.link(ob)
    ob.location = origin; assign(ob, mat); uv_project(mesh)
    ob['stage'] = 'L01 / basic architectural color and materials'
    return ob


def box_data(center, size, basis=None):
    w, d, h = size; x, y, z = center
    verts = [(a*w/2, b*d/2, c*h/2) for c in (-1, 1) for a, b in ((-1,-1),(1,-1),(1,1),(-1,1))]
    if basis:
        u, n = basis
        verts = [(x+a*u[0]+b*n[0], y+a*u[1]+b*n[1], z+c) for a, b, c in verts]
    else:verts = [(x+a,y+b,z+c) for a,b,c in verts]
    return verts, [(3,2,1,0),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7),(4,5,6,7)]


def batch_box(group, mat, center, size, collection='04_MODULAR_BUILDINGS', basis=None):
    # Batch only actual street props and substantial architectural accents.
    verts, faces = box_data(center, size, basis)
    dst_v, dst_f = BATCHES.setdefault((group,mat,collection), ([], []))
    offset = len(dst_v); dst_v.extend(verts)
    dst_f.extend(tuple(i+offset for i in face) for face in faces)


def flush_batches():
    for (group,mat,collection),(verts,faces) in BATCHES.items():
        # Center each architectural batch at ground level for a predictable local pivot.
        ox = (min(v[0] for v in verts)+max(v[0] for v in verts))/2
        oy = (min(v[1] for v in verts)+max(v[1] for v in verts))/2
        mesh_object('SM_GR_L01_'+group+'_'+mat,[(x-ox,y-oy,z) for x,y,z in verts],faces,mat,collection,(ox,oy,0))
    BATCHES.clear()


SIDES = {'S':((1,0),(0,-1)), 'E':((0,1),(1,0)),
         'N':((-1,0),(0,1)), 'W':((0,-1),(-1,0))}


def wall_box(group, mat, center, side, u, z, width, height, depth=.12, offset=0,
             collection='04_MODULAR_BUILDINGS'):
    tangent, normal = SIDES[side]
    x, y = center
    batch_box(group, mat, (x+tangent[0]*u+normal[0]*offset,
                          y+tangent[1]*u+normal[1]*offset,z),
              (width,depth,height),collection,(tangent,normal))


def text_sign(name, text, center, side, width, height, background, illuminated=True):
    # Dimensions are authored to fit a physical sign panel, not viewport overlays.
    x,y,z = center; u,n = SIDES[side]
    wall_box(name,background,(x,y),side,0,z,width,height,.18,collection='05_PROPS')
    curve = bpy.data.curves.new('TXT_GR_'+name,'FONT'); curve.body = text
    curve.align_x = 'CENTER'; curve.align_y = 'CENTER'; curve.size = 1
    curve.extrude = .006; curve.resolution_u = 2
    ob = bpy.data.objects.new('SM_GR_SignText_'+name,curve)
    bpy.data.collections['05_PROPS'].objects.link(ob)
    ob.location = (x+n[0]*.11,y+n[1]*.11,z)
    ob.rotation_euler = (math.pi/2,0,{'S':0,'E':math.pi/2,'N':math.pi,'W':-math.pi/2}[side])
    curve.materials.append(M['SignLetterLight' if illuminated else 'Ivory'])
    bpy.context.view_layer.update()
    local_width = max(p[0] for p in ob.bound_box)-min(p[0] for p in ob.bound_box)
    local_height = max(p[1] for p in ob.bound_box)-min(p[1] for p in ob.bound_box)
    factor = min(width*.87/max(local_width,.1),height*.60/max(local_height,.1))
    curve.size *= factor
    ob['role'] = 'physical early signage / not final typography'


def round_icon(name, center, side, diameter):
    u,n = SIDES[side]; x,y,z = center; r=diameter/2
    # Solid circular medallion sectors, with the central ring supplied by smaller disks.
    def disk(label,radius,mat,start=0,end=2*math.pi):
        count=32; verts=[]
        if end-start>=2*math.pi-.001:
            cross=[(radius*math.cos(i*2*math.pi/count),radius*math.sin(i*2*math.pi/count)) for i in range(count)]
        else:
            cross=[(radius*math.cos(start+(end-start)*i/count),radius*math.sin(start+(end-start)*i/count)) for i in range(count+1)]
        for depth in (-.06,.06):
            verts.extend((x+u[0]*a+n[0]*depth,y+u[1]*a+n[1]*depth,z+b) for a,b in cross)
        k=len(cross); faces=[tuple(reversed(range(k))),tuple(range(k,2*k))]
        faces.extend((i,(i+1)%k,(i+1)%k+k,i+k) for i in range(k))
        return mesh_object('SM_GR_'+name+'_'+label,verts,faces,mat,'03_HERO_BUILDINGS')
    disk('Red',r,'CenterRed',0,math.pi);disk('White',r,'Ivory',math.pi,2*math.pi)
    wall_box(name,'Charcoal',(x,y),side,0,z,diameter,.16,.18,.06,'03_HERO_BUILDINGS')
    x+=n[0]*.16;y+=n[1]*.16
    disk('CenterRing',r*.28,'Charcoal')
    x+=n[0]*.14;y+=n[1]*.14
    disk('CenterLight',r*.17,'SignLetterLight')


def build_materials():
    # Hex values are sRGB art direction; shader colors are converted to linear.
    for key,color in [('Ivory','#EEE7D8'),('Cream','#D8C9AF'),('WarmYellow','#DCC477'),
                      ('PaleBlue','#ABC8D1'),('MutedGreen','#B2C8B9'),('LightBrown','#C4A98B'),
                      ('Ochre','#BE894C'),('StationCream','#E6D7B7'),('Concrete','#C5CDC9')]:
        material(key,color,'concrete',.62)
    for key,color in [('DepartmentBlue','#1C5582'),('CenterRed','#B92717'),('WarmRed','#B4311C'),
                      ('StationTeal','#286D70'),('GymOrange','#B73E22'),('GymBlue','#356B88'),
                      ('TerminalTeal','#247F81'),('SkyCyan','#68C9CC'),('Charcoal','#303A3D'),
                      ('SignGold','#DCA72B'),('SignOrange','#DF7735'),('SignGreen','#4C957B')]:
        material(key,color,'painted_metal',.35,coat=.25)
    material('Bronze','#A96E3F',None,.29,1)
    material('Aluminum','#C2D1D3',None,.26,1)
    material('RoofBrown','#583E31','roof_metal',.5)
    material('RoofRed','#B94B38','roof_metal',.46)
    material('RoofCream','#E6D7B7','roof_metal',.40)
    material('RoofBlue','#466D80','roof_metal',.44)
    material('Brick','#FFFFFF','brick',.64)
    material('CeramicIvory','#F2E8D4','ceramic',.38,coat=.15)
    material('Asphalt','#FFFFFF','asphalt',.65)
    material('GoldenPavers','#EAC66A','golden_pavers',.57)
    material('SignLetterLight','#FFF3CE',None,.28,emission=2)
    material('SignCyanLight','#56D1DD',None,.3,emission=1.8)
    material('Grass','#829E64',None,.9)
    material('RoadPaint','#EEDFB2',None,.63)
    material('Water','#248997',None,.12,transmission=.5,coat=.3)
    material('ProxySuit','#DB713B',None,.74)
    material('ProxyDark','#253C50',None,.73)
    material('ProxyHead','#E6BF98',None,.68)


def body_palette(ob, index):
    name=ob.name; building=ob.get('building_id','')
    if building=='RadioTower':
        return 'SkyCyan' if 'BroadcastCrown' in name else 'Bronze' if 'Shaft' in name or 'Shoulder' in name else 'Charcoal'
    if building=='Station':return 'RoofCream' if 'Vault' in name else 'StationTeal' if 'Hall' in name else 'Ochre'
    if building=='DepartmentStore':return 'WarmRed' if 'Roofline' in name else 'DepartmentBlue'
    if building=='PokemonCenter':return 'CenterRed' if 'Roof' in name else 'SkyCyan' if 'Entry' in name else 'CeramicIvory'
    if building=='Gym':return 'RoofBrown' if 'Vault' in name else 'GymBlue' if 'Entry' in name else 'Ochre'
    if building=='GlobalTerminal':return 'TerminalTeal' if any(t in name for t in ('Terrace','Spire','Mast')) else 'PaleBlue'
    if building=='FlowerShop':return 'WarmRed' if 'Roof' in name else 'CeramicIvory'
    if building=='ShoppingArcade':return 'DepartmentBlue' if 'Canopy' in name else 'WarmYellow'
    if 'HipRoof' in name:return 'RoofRed' if index%3 else 'RoofBrown'
    if 'House' in name:return 'Brick' if index%3==0 else 'Cream'
    return ['Ivory','PaleBlue','Cream','MutedGreen','WarmYellow','LightBrown'][index%6]


def add_facade(ob, index):
    # The wall itself carries window alpha/color/roughness/normal images.
    mat = apply_facade(ob, ob.material_slots[0].material, TEX, index)
    if mat is None:
        return
    building = ob.get('building_id')
    if ob.get('landmark') or building == 'GlobalTerminal' or ob.get('context_only'):
        return
    points = [ob.matrix_world @ Vector(v) for v in ob.bound_box]
    if min(p.z for p in points) > .1:
        return
    if ob.get('district') in ('D7', 'D9') and index % 4 != 0:
        return
    x = (max(p.x for p in points) + min(p.x for p in points)) / 2
    y = (max(p.y for p in points) + min(p.y for p in points)) / 2
    w = max(p.x for p in points) - min(p.x for p in points)
    d = max(p.y for p in points) - min(p.y for p in points)
    accent = ['StationTeal','SignGold','WarmRed','DepartmentBlue','SignGreen'][index % 5]
    titles = ['GOLDEN CAFE','MART','BOOKS','BICYCLE','BAKERY','TRAINER GOODS','RECORDS','FLOWERS']
    for side in ('S', 'E' if x < 0 else 'W'):
        across, out = (w, d/2) if side == 'S' else (d, w/2)
        tangent, n = SIDES[side]
        center = (x+n[0]*out, y+n[1]*out)
        wall_box(building+'_ShopCanopy', accent, center, side, 0, 3.55,
                 across-2, .2, 1.2, .5)
        text_sign(building+'_Store_'+side, titles[index % len(titles)],
                  (center[0]+n[0]*.22, center[1]+n[1]*.22, 4.2),
                  side, min(across-3, 13), .85, accent)
        if index % 5 == 0:
            text_sign(building+'_Blade_'+side, 'S\nH\nO\nP',
                      (center[0]+tangent[0]*(-across/2+1)+n[0]*.6,
                       center[1]+tangent[1]*(-across/2+1)+n[1]*.6, 7),
                      side, 1.25, 4.6, accent)


def hero_accents():
    # Center east-facing portal and familiar red/white symbol.
    text_sign('Center','POKEMON CENTER',(-16.22,-85,4.3),'E',14,.9,'CenterRed')
    round_icon('CenterEmblem',(-16.2,-85,7.35),'E',2.0)
    # Department retail signage and upper architectural red-orange color anchors.
    text_sign('Department','GOLDENROD DEPT.',(60,-113.35,9.5),'S',48,2.7,'WarmRed')
    text_sign('DepartmentWest','DEPARTMENT',(27.65,-85,9.5),'W',38,2.7,'WarmRed')
    text_sign('DepartmentBlade','D\nE\nP\nT',(33,-113.5,27),'S',4,16,'WarmRed')
    # Station entry sign and colored fascia. Round windows are roof-face textures.
    text_sign('Station','GOLDENROD STATION',(-83,-.35+146,16.6),'S',64,2.4,'StationTeal')
    wall_box('StationCanopy','SignGold',(-83,145.5),'S',0,4.6,34,.35,3,collection='03_HERO_BUILDINGS')
    # Gym's primary red-orange ribs follow its real barrel profile; no tiny trim.
    for y in (230,253,278):
        pts=[(83+22.5*math.cos(i*math.pi/24),y,9+8*math.sin(i*math.pi/24)+.18) for i in range(25)]
        curve=bpy.data.curves.new('GymPrimaryRib','CURVE');curve.dimensions='3D'
        curve.bevel_depth=.40;curve.bevel_resolution=2;curve.resolution_u=2;curve.use_fill_caps=True
        spline=curve.splines.new('POLY');spline.points.add(len(pts)-1)
        for p,co in zip(spline.points,pts):p.co=(*co,1)
        ob=bpy.data.objects.new('SM_GR_Gym_PrimaryRib_'+str(y),curve)
        bpy.data.collections['03_HERO_BUILDINGS'].objects.link(ob);curve.materials.append(M['GymOrange'])
        for x in (60.5,105.5):batch_box('GymRibLegs','GymOrange',(x,y,4.5),(.8,.8,9),'03_HERO_BUILDINGS')
    text_sign('Gym','GOLDENROD GYM',(83,224.2,6.6),'S',21,1.5,'GymBlue')
    text_sign('FlowerShop','FLOWERS',(129,239.65,5.4),'S',16,1.3,'WarmRed')
    # Crown glass and frames are textured directly on the approved cylinder.
    text_sign('Radio','GOLDENROD RADIO',(-218,142.7,11.3),'S',39,2.3,'Charcoal')
    text_sign('Terminal','GLOBAL TERMINAL',(-366,123.7,8),'S',32,2,'TerminalTeal')
    text_sign('Arcade','GOLDENROD SHOPPING',(74,-241.3,6.1),'S',21,1.2,'DepartmentBlue')
    text_sign('SouthGate','GOLDENROD',(0,-267.7,10.7),'S',18,1.15,'SignGold')


def street_color():
    # Broad markings and a handful of full-size street objects establish pedestrian color.
    for y in (180,198):
        batch_box('ViaductFascia','StationTeal',(155.5,y,9.6),(369,.32,.55),'02_ROADS')
    for y in (-130,40,210):
        for crossing_y in (y-17,y+17):
            for x in range(-6,7,2):batch_box('Crossings','RoadPaint',(x,crossing_y,.025),(1,4,.018),'02_ROADS')
    for y in range(-285,300,12):
        if any(abs(y-v)<23 for v in (-305,-130,40,210,305)):continue
        batch_box('CenterLine','RoadPaint',(0,y,.026),(.13,4,.018),'02_ROADS')
    # Readable scale objects kept outside the four approved route envelopes.
    for i,(x,y,side) in enumerate([(-43,-103,'E'),(104,-107,'S'),(-140,115,'E'),(119,237,'S'),(82,-240,'S')]):
        c=['CenterRed','DepartmentBlue','StationTeal'][i%3]
        batch_box('Vending_'+str(i),c,(x,y,1.05),(1.05,.82,2.1),'05_PROPS')
    for i,(x,y) in enumerate([(-15,-106),(27,-117),(-142,140),(-27,140),(111,230)]):
        batch_box('Wayfinding','StationTeal',(x,y,1.4),(.14,.14,2.8),'05_PROPS')
        text_sign('Wayfinding_'+str(i),['CENTER','STATION','RADIO','PLATFORMS','GYM'][i],(x,y-.13,2.5),'S',2.4,.55,'StationTeal')
    # Broad advertisement/banners, no intricate props or utility clutter.
    for i,x in enumerate((38,82)):
        text_sign('RetailBanner_'+str(i),['NEW SEASON','TRAINER FAIR'][i],(x,-113.4,18),'S',5.5,6.5,'SignGold' if i else 'StationTeal')


def lighting():
    scene=bpy.context.scene
    scene.render.engine='CYCLES';scene.cycles.samples=40;scene.cycles.use_denoising=True
    scene.cycles.max_bounces=6;scene.cycles.transmission_bounces=4
    prefs=bpy.context.preferences.addons['cycles'].preferences
    prefs.compute_device_type='OPTIX';prefs.get_devices()
    for device in prefs.devices:device.use=device.type=='OPTIX'
    scene.cycles.device='GPU'
    world=bpy.data.worlds.new('WORLD_GR_ClearGoldenrodDay');world.use_nodes=True;scene.world=world
    nodes=world.node_tree.nodes;nodes.clear()
    sky=nodes.new('ShaderNodeTexSky');sky.sky_type='MULTIPLE_SCATTERING'
    sky.sun_elevation=math.radians(42);sky.sun_rotation=math.radians(225)
    sky.sun_disc=True;sky.sun_size=math.radians(.6);sky.sun_intensity=.8
    sky.air_density=1;sky.ozone_density=1
    bg=nodes.new('ShaderNodeBackground');bg.inputs['Strength'].default_value=.32
    output=nodes.new('ShaderNodeOutputWorld')
    world.node_tree.links.new(sky.outputs['Color'],bg.inputs['Color']);world.node_tree.links.new(bg.outputs[0],output.inputs[0])
    scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast'
    scene.view_settings.exposure=-1.3
    scene.render.resolution_x=1500;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
    scene['stage']='L01 COLOR / MATERIAL DEVELOPMENT — approved B02 layout retained'
    scene['PBR_export_note']='Image maps and UVs supplied. Recreate tint/roughness scale/glass/emission in UE; Blender nodes do not auto-transfer.'
    scene['reference_status']='Local six reference JPEGs and showcase video; Goldenrod_Palette.md'


def close_camera():
    scene=bpy.context.scene;original=bpy.data.objects['CAM_GR_02_CenterDepartment']
    camera=original.copy();camera.data=original.data.copy();camera.name='CAM_GR_11_CenterStreet'
    bpy.data.collections['00_REFERENCE'].objects.link(camera)
    camera.location=(9.5,-121,1.8);camera.data.lens=25
    direction=Vector((-29,-85,6))-camera.location
    camera.rotation_euler=direction.to_track_quat('-Z','Y').to_euler()
    # Move a copy of the approved proxy, then snap its feet onto the local ground.
    source=[o for o in scene.objects if o.get('camera_proxy')=='02_CenterDepartment']
    f=Vector((direction.x,direction.y,0)).normalized()
    src_center=sum((o.location for o in source if '_Leg' in o.name),Vector())/2
    dest=Vector((camera.location.x+f.x*4.2,camera.location.y+f.y*4.2,0))
    old_heading=math.atan2(-(PLAN['cameras'][1]['target'][0]-PLAN['cameras'][1]['pos'][0]),
                           PLAN['cameras'][1]['target'][1]-PLAN['cameras'][1]['pos'][1])
    heading=math.atan2(-f.x,f.y);delta=heading-old_heading
    feet=min((o.matrix_world@Vector(v)).z for o in source for v in o.bound_box)
    for ob in source:
        new=ob.copy();new.name=ob.name.replace('02_CenterDepartment','11_CenterStreet')
        bpy.data.collections['00_REFERENCE'].objects.link(new)
        off=ob.location-src_center
        new.location=(dest.x+off.x*math.cos(delta)-off.y*math.sin(delta),
                      dest.y+off.x*math.sin(delta)+off.y*math.cos(delta),ob.location.z-feet+.01)
        new.rotation_euler.z+=delta;new['camera_proxy']='11_CenterStreet'


def validate(source_hash):
    scene=bpy.context.scene;bpy.context.view_layer.update()
    meshes=[o for o in scene.objects if o.type=='MESH']
    missing=[o.name for o in meshes if not o.material_slots or not o.material_slots[0].material]
    no_uv=[o.name for o in meshes if not o.data.uv_layers]
    hero_bounds={}
    for b in PLAN['buildings']:
        if not b['hero']:continue
        parts=[o for o in meshes if o.get('building_id')==b['name']]
        pts=[o.matrix_world@Vector(v) for o in parts for v in o.bound_box]
        dimensions=[round(max(v[i] for v in pts)-min(v[i] for v in pts),3) for i in range(3)]
        assert dimensions==[b['w'],b['d'],b['h']],(b['name'],dimensions)
        hero_bounds[b['name']]=dimensions
    assert not missing and not no_uv,(missing,no_uv)
    assert all(mat.use_nodes for mat in bpy.data.materials if mat.users)
    refs=[im for im in bpy.data.images if im.source=='FILE']
    assert all(im.packed_file for im in refs)
    report={'stage':'L01','source_B02_sha256':source_hash,'mesh_objects':len(meshes),
            'used_meshes':len({o.data.name for o in meshes}),'materials':len([m for m in bpy.data.materials if m.users]),
            'packed_texture_images':len(refs),'unmaterialed_objects':missing,'missing_uv_objects':no_uv,
            'approved_hero_mass_dimensions_unchanged':hero_bounds,
            'actual_renderer':'Cycles / OptiX GPU / image texture PBR / AgX',
            'scope':'Initial color, material, facade glazing and pedestrian signage; no final interiors or UE integration.'}
    (OUT/'Goldenrod_Lookdev_QA.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('LOOKDEV_QA',json.dumps(report),flush=True)


def build():
    OUT.mkdir(exist_ok=True,parents=True)
    source_hash=hashlib.sha256((CITY/'Goldenrod_Graybox.blend').read_bytes()).hexdigest()
    if bpy.context.scene.name!='GR_Graybox_B02':raise ValueError('Open the approved B02 source scene for a clean build.')
    build_materials()
    scene=bpy.context.scene;scene.name='GR_ColorStudy_L01'
    originals=[o for o in scene.objects if o.type=='MESH']
    ids={b['name']:i for i,b in enumerate(PLAN['buildings'])}
    for ob in originals:
        name=ob.name
        if ob.get('building_id'):key=body_palette(ob,ids[ob['building_id']])
        elif ob.get('camera_proxy'):
            key='ProxyHead' if 'Head' in name or 'Neck' in name else 'ProxyDark' if 'Leg' in name else 'ProxySuit'
        elif 'water_rect' in name:key='Water'
        elif 'RoadNetwork' in name:key='Asphalt'
        elif 'RailViaduct' in name:key='Concrete'
        elif 'Sidewalk' in name or 'Reserve' in name:key='Grass' if 'Park' in name or 'Garden' in name else 'GoldenPavers'
        elif 'Ground' in name or 'Coast' in name:key='Concrete'
        elif 'SouthGate' in name:key='SignGold' if 'Arch' in name else 'Ochre'
        else:key='Concrete'
        assign(ob,key)
    for mesh in {o.data for o in originals}:uv_project(mesh)
    bpy.context.view_layer.update()
    for ob in originals:
        if ob.get('building_id'):add_facade(ob,ids[ob['building_id']])
    hero_accents();street_color();flush_batches()
    lighting();close_camera()
    for ob in scene.objects:
        if ob.get('camera_proxy'):ob.hide_render=True
    scene.camera=bpy.data.objects['CAM_GR_07_Aerial']
    note=bpy.data.texts.new('README_GOLDENROD_L01')
    note.write('Approved B02 massing retained. L01 uses facade texture windows and initial signage.\n'
               'No added window, glass-pane, backing, frame, mullion or floor-band geometry.\n'
               'Cycles PBR renders; every image map is packed. No external 3D generation.\n'
               'Surface UVs repeat at 4m; dedicated facade UVs and masks place texture-only windows.\n'
               'Normal maps use OpenGL +Y; flip green for Unreal DirectX normals.\n'
               'Blender shader graphs are not an Unreal material importer. See material manifest and palette.\n'
               'NPCs, full interiors, final collision and UE integration are not implemented in this stage.\n')
    (OUT/'Goldenrod_Material_Manifest.json').write_text(json.dumps([
        {'name':mat.name,'color_srgb':mat.get('base_color_srgb'),'family':mat.get('surface_family'),
         'roughness_multiplier':next((n.inputs[1].default_value for n in mat.node_tree.nodes if n.type=='MATH'),None),
         'metallic':next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED').inputs['Metallic'].default_value,
         'transmission':next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED').inputs['Transmission Weight'].default_value,
         'window_representation':mat.get('window_representation'),
         'texture_images':sorted({n.image.name for n in mat.node_tree.nodes if n.type=='TEX_IMAGE' and n.image})}
        for mat in bpy.data.materials if mat.users],indent=2),encoding='utf-8')
    validate(source_hash)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Goldenrod_ColorStudy.blend'),compress=True)
    print('LOOKDEV_SAVED',flush=True)


def render():
    scene=bpy.context.scene; lighting()
    # After --render an optional comma-separated camera list limits the preview pass.
    at=sys.argv.index('--render'); chosen=sys.argv[at+1].split(',') if at+1<len(sys.argv) else ['02','04','05','07','09','10','11']
    (OUT/'Previews').mkdir(exist_ok=True)
    for cam in sorted((o for o in scene.objects if o.type=='CAMERA'),key=lambda o:o.name):
        short=cam.name.replace('CAM_GR_','')
        if short[:2] not in chosen:continue
        for ob in scene.objects:
            if ob.get('camera_proxy'):ob.hide_render=ob['camera_proxy']!=short
        scene.camera=cam;scene.render.filepath=str(OUT/'Previews'/('GR_L01_'+short+'.png'))
        bpy.ops.render.render(write_still=True)
        print('LOOKDEV_RENDERED',short,flush=True)


if __name__=='__main__':
    if '--render' in sys.argv:render()
    else:build()
