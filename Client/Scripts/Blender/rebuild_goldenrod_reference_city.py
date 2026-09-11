"""R02: reference-led Goldenrod geometry, single ground surface, texture facades, FBX.

Run after the three generate_goldenrod_r02_*.py texture generators.
blender -b --python this.py
blender -b Rebuild/Goldenrod_City_R02.blend --python this.py -- --render
"""
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'SourceArt/Environments/Goldenrod/Rebuild'
TEX = OUT / 'Textures'
EXPORT = OUT / 'Unreal'
MATS = {}
GROUP = 'City'
GROUND = .12


def material(key, tile=2, emission=0):
    if key in MATS:
        return MATS[key]
    mat = bpy.data.materials.new('M_R02_' + key)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    bs = nodes.new('ShaderNodeBsdfPrincipled'); bs.location = (220, 0)
    bs.inputs['IOR'].default_value = 1.5
    bs.inputs['Coat Weight'].default_value = .03 if 'Facade' in key or key == 'DoorBlue' else 0
    bs.inputs['Coat Roughness'].default_value = .2
    output = nodes.new('ShaderNodeOutputMaterial'); output.location = (500, 0)
    links.new(bs.outputs[0], output.inputs[0])
    for i, channel in enumerate(('BaseColor', 'Roughness', 'Normal', 'Metallic')):
        path = TEX / (key + '_' + channel + '.png')
        if not path.exists():
            raise FileNotFoundError(path)
        im = bpy.data.images.load(str(path), check_existing=True)
        im.colorspace_settings.name = 'sRGB' if channel == 'BaseColor' else 'Non-Color'
        tex = nodes.new('ShaderNodeTexImage'); tex.image = im; tex.location = (-550, 380-i*230)
        tex.interpolation = 'Linear'
        tex.extension = 'EXTEND' if (key.startswith(('Facade_', 'Sign_')) or key=='DoorBlue') and key!='Facade_RadioCrown' else 'REPEAT'
        if channel == 'Normal':
            normal = nodes.new('ShaderNodeNormalMap')
            normal.inputs['Strength'].default_value = .12 if key.startswith('Roof') else .18 if key in ('Promenade','GoldenSidewalk','PaleKerb') else .3
            normal.location = (-120, -100)
            links.new(tex.outputs['Color'], normal.inputs['Color'])
            links.new(normal.outputs[0], bs.inputs['Normal'])
        else:
            links.new(tex.outputs['Color'], bs.inputs[{'BaseColor':'Base Color'}.get(channel, channel)])
        if channel == 'BaseColor' and emission:
            links.new(tex.outputs['Color'], bs.inputs['Emission Color'])
            bs.inputs['Emission Strength'].default_value = emission
    mat['texture_prefix'] = key; mat['tile_m'] = tile; mat['emission_strength'] = emission
    MATS[key] = mat
    return mat


def uv_mesh(mesh, materials, normalized=None):
    uv = mesh.uv_layers.get('UVMap') or mesh.uv_layers.new(name='UVMap')
    for face in mesh.polygons:
        axis = max(range(3), key=lambda i: abs(face.normal[i]))
        coords = []
        for li in face.loop_indices:
            p = mesh.vertices[mesh.loops[li].vertex_index].co
            if axis == 2: coords.append((p.x, p.y))
            elif axis == 1: coords.append((p.x if face.normal.y < 0 else -p.x, p.z))
            else: coords.append((p.y if face.normal.x > 0 else -p.y, p.z))
        mat = materials[face.material_index]
        key = mat['texture_prefix']
        atlas = key.startswith(('Facade_', 'Sign_')) or key == 'DoorBlue'
        if atlas:
            bounds = normalized.get(face.index) if normalized else None
            u0, u1, v0, v1 = bounds or (min(p[0] for p in coords), max(p[0] for p in coords),
                                       min(p[1] for p in coords), max(p[1] for p in coords))
            coords = [((u-u0)/max(u1-u0,.001), (v-v0)/max(v1-v0,.001)) for u,v in coords]
        else:
            coords = [(u/mat['tile_m'],v/mat['tile_m']) for u,v in coords]
        for li, xy in zip(face.loop_indices, coords): uv.data[li].uv = xy


def mesh_obj(name, verts, faces, keys, face_mats=None, origin=(0,0,0), bevel=0, normalized=None):
    mesh = bpy.data.meshes.new(name+'_Geo'); mesh.from_pydata(verts, [], faces); mesh.update()
    bm = bmesh.new(); bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces)); bm.to_mesh(mesh); bm.free()
    materials = [material(k) for k in keys]
    for mat in materials: mesh.materials.append(mat)
    if face_mats:
        for face, idx in zip(mesh.polygons, face_mats): face.material_index = idx
    uv_mesh(mesh, materials, normalized)
    ob = bpy.data.objects.new('SM_R02_'+name, mesh)
    bpy.context.scene.collection.objects.link(ob); ob.location = origin
    ob['city_group'] = GROUP
    ob['window_policy'] = 'Windows, frames, doors and facade lines are image textures on building polygons.'
    if bevel:
        spans = [max(v[i] for v in verts)-min(v[i] for v in verts) for i in range(3)]
        bevel = min(bevel, min(spans)*.45)
        mod = ob.modifiers.new('Construction_edge_radius', 'BEVEL'); mod.width = bevel; mod.segments = 3
        mod.affect = 'EDGES'
        mod = ob.modifiers.new('Weighted_surface_normals', 'WEIGHTED_NORMAL'); mod.keep_sharp = True
    return ob


def box(name, x,y,z,w,d,h,key, bevel=.08, front=None):
    verts = [(a*w/2,b*d/2,c*h/2) for c in (-1,1) for a,b in ((-1,-1),(1,-1),(1,1),(-1,1))]
    faces = [(3,2,1,0),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7),(4,5,6,7)]
    keys = [key] + ([front] if front else [])
    return mesh_obj(name, verts, faces, keys, [0,1,0,0,0,0] if front else None,
                    (x,y,z+h/2), bevel)


def outline(w,d,cut):
    a,b = w/2,d/2; c=min(cut,a*.45,b*.45)
    return [(-a+c,-b),(a-c,-b),(a,-b+c),(a,b-c),(a-c,b),(-a+c,b),(-a,b-c),(-a,-b+c)]


def loft(name,x,y,z,w,d,h,key,cut=1,top=None,facade=None,bevel=.1):
    tw,td = top or (w,d)
    verts = [(a,b,c) for c,(ww,dd) in ((0,(w,d)),(h,(tw,td))) for a,b in outline(ww,dd,cut)]
    faces = [tuple(reversed(range(8))),tuple(range(8,16))] + [(i,(i+1)%8,(i+1)%8+8,i+8) for i in range(8)]
    keys = [key]+([facade] if facade else [])
    mats = [0,0]+[1 if facade and i%2==0 else 0 for i in range(8)]
    return mesh_obj(name,verts,faces,keys,mats,(x,y,z),bevel)


def cylinder(name,x,y,z,r,h,key,r_top=None,segments=32,side=None):
    rt = r if r_top is None else r_top
    verts = [(rad*math.cos(i*2*math.pi/segments),rad*math.sin(i*2*math.pi/segments),zz)
             for zz,rad in ((0,r),(h,rt)) for i in range(segments)]
    faces = [tuple(reversed(range(segments))),tuple(range(segments,2*segments))]
    faces += [(i,(i+1)%segments,(i+1)%segments+segments,i+segments) for i in range(segments)]
    ob = mesh_obj(name,verts,faces,[key]+([side] if side else []),
                  [0,0]+[1 if side else 0]*segments,(x,y,z),.04)
    if side:
        uv = ob.data.uv_layers.active
        for i,face in enumerate(ob.data.polygons[2:]):
            for li,(u,v) in zip(face.loop_indices,((i/segments*3,0),((i+1)/segments*3,0),((i+1)/segments*3,1),(i/segments*3,1))):
                uv.data[li].uv=(u,v)
    return ob


def profile_prism(name, x,y,z, profile, depth, key, axis='Y', cap=None, bevel=.08):
    count=len(profile)
    verts=[(a,b,c) if axis=='Y' else (b,a,c) for b in (-depth/2,depth/2) for a,c in profile]
    faces=[tuple(reversed(range(count))),tuple(range(count,2*count))]
    faces += [(i,(i+1)%count,(i+1)%count+count,i+count) for i in range(count)]
    return mesh_obj(name,verts,faces,[key]+([cap] if cap else []),
                    [1 if cap else 0]*2+[0]*count,(x,y,z),bevel)


def barrel(name,x,y,z,width,depth,rise,key,axis='Y',cap=None):
    profile=[(width/2*math.cos(i*math.pi/24),rise*math.sin(i*math.pi/24)) for i in range(25)]
    return profile_prism(name,x,y,z,profile,depth,key,axis,cap,.10)


def tube(name,points,r,key):
    curve=bpy.data.curves.new(name+'_Curve','CURVE');curve.dimensions='3D'
    curve.bevel_depth=r;curve.bevel_resolution=3;curve.resolution_u=1;curve.use_fill_caps=True
    spline=curve.splines.new('POLY');spline.points.add(len(points)-1)
    for point,co in zip(spline.points,points):point.co=(*co,1)
    ob=bpy.data.objects.new('SM_R02_'+name,curve);bpy.context.scene.collection.objects.link(ob)
    curve.materials.append(material(key));ob['city_group']=GROUP
    return ob


def sphere(name,x,y,z,r,key):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=8,radius=r,location=(x,y,z))
    ob=bpy.context.object;ob.name='SM_R02_'+name;ob['city_group']=GROUP
    ob.data.materials.append(material(key))
    for p in ob.data.polygons:p.use_smooth=True
    return ob


def panel(name,x,y,z,w,h,key,depth=.2):
    return box(name,x,y,z-h/2,w,depth,h,'Accent_Navy',.04,front=key)


def trim_ring(name,x,y,z,w,d,height,width,key,cut=1):
    outer=outline(w,d,cut);inner=outline(w-2*width,d-2*width,max(.1,cut-width*.5))
    rings=[outer,inner,outer,inner]
    verts=[(a,b,zz) for ring,zz in zip(rings,(0,0,height,height)) for a,b in ring]
    faces=[]
    for i in range(8):
        j=(i+1)%8
        faces.extend(((i,j,16+j,16+i),(8+j,8+i,24+i,24+j),
                      (16+i,16+j,24+j,24+i),(j,i,8+i,8+j)))
    return mesh_obj(name,verts,faces,[key],origin=(x,y,z),bevel=.06)


def roof_terrace(name,x,y,z,w,d):
    loft(name+'_Deck',x,y,z,w,d,.32,'RoofTeal',cut=1.4,bevel=.06)
    trim_ring(name+'_Parapet',x,y,z+.32,w+1,d+1,.55,.45,'Accent_Teal',1.8)
    box(name+'_Vent',x-w*.23,y+d*.2,z+.34,2.7,2.4,.9,'Accent_Navy',.2)
    box(name+'_VentCap',x-w*.23,y+d*.2,z+1.24,3.1,2.8,.16,'Accent_Aluminum',.06)


def center():
    global GROUP
    GROUP='PokemonCenter';x,y=-22,-96
    loft('Center_RoundClinic',x,y,GROUND,24,24,8,'Accent_Cream',cut=3,facade='Facade_CenterWhite',bevel=.3)
    loft('Center_RedRoofCap',x,y,GROUND+8,26,26,1.3,'RoofRed',cut=4,bevel=.45)
    # The emblem is an architectural sign; its circular artwork is a surface image.
    icon=cylinder('Center_Emblem',x,y-12.4,GROUND+7,1.45,.28,'Accent_Cream',segments=48)
    icon.rotation_euler.x=math.pi/2
    icon.data.materials.append(material('Sign_Center'))
    icon.data.polygons[1].material_index=1;uv_mesh(icon.data,list(icon.data.materials))
    box('Center_EntryPortal',x,y-12.4,GROUND,4,1.2,3.4,'Accent_Cobalt',.22,front='DoorBlue')
    box('Center_EntryCanopy',x,y-13,GROUND+4.2,7,3,.5,'RoofRed',.25)


def department():
    global GROUP
    GROUP='DepartmentStore';x,y=32,-96
    loft('Department_BlueRetail',x,y,GROUND,32,30,28,'Accent_Cobalt',cut=2,facade='Facade_DeptBlue',bevel=.16)
    loft('Department_RedRoofline',x,y,GROUND+28,34,32,.8,'RoofRed',cut=2.5,bevel=.25)
    loft('Department_Terrace',x,y,GROUND+28.8,31,29,.2,'RoofRed',cut=2,bevel=.05)
    trim_ring('Department_RedParapet',x,y,GROUND+29,34,32,.65,.55,'RoofRed',2.5)
    box('Department_RoofMarker',x,y,GROUND+29.2,5,4,3.2,'Accent_Cobalt',.3,front='Sign_Gym')
    panel('Department_PinkBlade',x-13,y-15.6,GROUND+19,4,16,'Sign_Dept',.7)
    box('Department_EntryPortal',x,y-15.3,GROUND,5,1.2,3.6,'Accent_Cobalt',.16,front='DoorBlue')


def radio():
    global GROUP
    GROUP='RadioTower';x,y=-113,26
    loft('Radio_BroadcastPodium',x,y,GROUND,32,28,12,'Accent_Charcoal',cut=1.5,facade='Facade_RadioDark',bevel=.16)
    trim_ring('Radio_PodiumParapet',x,y,GROUND+12,33,29,.7,.6,'Accent_Navy')
    cylinder('Radio_ExposedShaft',x,y,GROUND+12.2,3.4,28,'Accent_Bronze')
    for z in (18,25,32,38):cylinder('Radio_ShaftCollar_'+str(z),x,y,GROUND+z,3.57,.32,'Accent_Gold')
    cylinder('Radio_CrownBowl',x,y,GROUND+40.2,7.4,3.1,'Accent_Bronze',r_top=12.5)
    cylinder('Radio_GlassCrown',x,y,GROUND+43.3,12.3,4.2,'Accent_Navy',side='Facade_RadioCrown',segments=48)
    cylinder('Radio_CrownRoof',x,y,GROUND+47.5,13.3,1.1,'Accent_Navy',r_top=12.6,segments=48)
    cylinder('Radio_AntennaHousing',x,y,GROUND+48.6,1.5,3.5,'Accent_Bronze')
    cylinder('Radio_Antenna',x,y,GROUND+52.1,.28,5,'Accent_Aluminum',segments=16)
    panel('Radio_StationSign',x,y-14.22,8.5,7.2,2.4,'Sign_Radio')


def station():
    global GROUP
    GROUP='Station';x,y=-32,22
    loft('Station_FrontHall',x,y,GROUND,32,22,14,'Accent_Ochre',cut=.6,facade='Facade_StationOchre',bevel=.12)
    barrel('Station_FrontArch',x,y,GROUND+14,32,22,8,'RoofCream',cap='Facade_StationArch')
    # Orthogonal rear train shed: a single front arch remains the dominant street silhouette.
    barrel('Station_PlatformShed',x,45,GROUND+12,24,62,7,'RoofCream',axis='X')
    for xx in (x-30,x+30):
        for yy in (35,55):box('Station_ShedSupport',xx,yy,GROUND,1.4,1.4,12,'Accent_Cream',.08)
    box('Station_Entry',x,y-11.4,GROUND,5,1.4,3.6,'Accent_Cobalt',.2,front='DoorBlue')
    panel('Station_Nameplate',x,y-11.35,6,7.2,1.8,'Sign_Station')
    box('Station_EntryAwning',x,y-12,GROUND+4.5,10,3.5,.45,'Accent_Teal',.16)
    GROUP='Railway'
    box('Railway_Deck',77,45,7.8,156,12,.9,'Accent_Cream',.14)
    for yy in (38.65,51.35):
        box('Railway_Parapet',77,yy,8.7,156,.55,1.5,'Accent_Cream',.08)
        box('Railway_TealEdge',77,yy,10.2,156,.66,.2,'Accent_Teal',.05)
    for xx in range(16,153,25):
        for yy in (40,50):box('Railway_Pier',xx,yy,GROUND,2.1,2.1,7.8-GROUND,'Accent_Cream',.14)
    for yy in (43.9,46.1):box('Railway_Rail',77,yy,8.71,156,.14,.16,'Accent_Aluminum',.02)


def gym():
    global GROUP
    GROUP='Gym';x,y=43,88
    loft('Gym_CapsuleBase',x,y,GROUND,26,32,7,'Accent_RoofBrown',cut=4,facade='Facade_GymFront',bevel=.4)
    barrel('Gym_CurvedShell',x,y,GROUND+7,32,26,8,'Accent_RoofBrown',axis='X')
    for z in (.5,3.2):
        trim_ring('Gym_PinkSkirt_'+str(z),x,y,GROUND+z,26.8,32.8,.6,.38,'Accent_Pink',4)
    for xx in (x-9,x+9):
        points=[(xx,y-16,GROUND+.5),(xx,y-16,GROUND+7)]
        points += [(xx,y-16*math.cos(i*math.pi/32),GROUND+7+8*math.sin(i*math.pi/32)) for i in range(1,33)]
        points.append((xx,y+16,GROUND+.5))
        tube('Gym_OrangeArch',points,.55,'Accent_Orange')
    box('Gym_BluePortal',x,y-16.6,GROUND,5.5,2,3.5,'Accent_Cobalt',.24,front='DoorBlue')
    barrel('Gym_PortalCap',x,y-16.6,GROUND+3.5,5.5,2,1.8,'Accent_Cobalt')
    panel('Gym_Badge',x,y-17.75,GROUND+4.25,1.7,1.7,'Sign_Gym')
    GROUP='FlowerShop'
    loft('FlowerShop_Body',76,85,GROUND,15,16,8,'Accent_Cream',cut=1,facade='Facade_HouseOchre',bevel=.16)
    loft('FlowerShop_PinkCornice',76,85,GROUND+8,16,17,.8,'Accent_Pink',cut=1.2,bevel=.25)
    box('FlowerShop_Entry',76,76.65,GROUND,4,1.2,3.2,'Accent_CyanGlow',.12,front='DoorBlue')
    for i in range(9):box('FlowerShop_Awning',70+i*1.5,76.4,4.7,1.5,2.3,.35,'RoofRed' if i%2==0 else 'Accent_Cream',.04)
    sphere('FlowerShop_FlowerHeart',76,75.5,8.5,.62,'Accent_Gold')
    for i in range(6):
        a=i*math.pi/3;sphere('FlowerShop_Petal',76+math.cos(a)*1.1,75.55,8.5+math.sin(a)*1.1,.66,'Accent_Pink')


def terminal():
    global GROUP
    GROUP='GlobalTerminal';x,y=-215,31
    for i,(z,w,d,h,tw,td) in enumerate(((0,34,36,20,28,30),(21.3,28,30,18,24,26),(40.6,22,24,14,18,20))):
        loft('Terminal_Tier_'+str(i),x,y,GROUND+z,w,d,h,'Accent_Teal',cut=2,top=(tw,td),facade='Facade_TerminalGlass',bevel=.12)
        loft('Terminal_TealBelt_'+str(i),x,y,GROUND+z+h,w+2,d+2,1.3,'Accent_Teal',cut=2,bevel=.14)
    loft('Terminal_Spire',x,y,GROUND+55.9,8,9,10,'Accent_Teal',cut=.5,top=(5,5),bevel=.1)
    cylinder('Terminal_Beacon',x,y,GROUND+65.9,2.8,1.2,'Accent_CyanGlow',segments=24)
    cylinder('Terminal_Antenna',x,y,GROUND+67.1,.26,4.9,'Accent_Aluminum',segments=16)
    # Major front portal follows the stepped facade; it is not a window overlay.
    levels=[(GROUND,-18.45),(GROUND+20,-15.45),(GROUND+21.3,-15.45),(GROUND+39.3,-13.45),(GROUND+40.6,-12.45),(GROUND+49,-11.25)]
    verts=[(x+dx,y+yy+offset,zz) for offset in (0,.32) for zz,yy in levels for dx in (-3.6,3.6)]
    n=len(levels);faces=[]
    for j in range(n-1):
        a=2*j;b=2*(j+1)
        faces.extend(((a,a+1,b+1,b),(2*n+a+1,2*n+a,2*n+b,2*n+b+1),
                      (a,b,2*n+b,2*n+a),(a+1,2*n+a+1,2*n+b+1,b+1)))
    faces += [(1,0,2*n,2*n+1),(2*n-2,2*n-1,4*n-1,4*n-2)]
    mesh_obj('Terminal_PortalSpine',verts,faces,['Accent_Charcoal'],bevel=.08)
    for dx in (-3.8,3.8):tube('Terminal_PinkPortalEdge',[(x+dx,y+yy-.03,zz) for zz,yy in levels],.24,'Accent_Pink')
    box('Terminal_Entry',x,y-18.8,GROUND,4.5,1.2,3.6,'Accent_Cobalt',.12,front='DoorBlue')
    panel('Terminal_Wireless',x,y-18.82,10.4,4.4,11,'Sign_Terminal')


def south_gate():
    global GROUP
    GROUP='SouthGate';y=-145
    for x in (-11,11):
        loft('Gate_Foot',x,y,GROUND,3,3.2,.7,'Accent_Gold',cut=.4,top=(2.3,2.5),bevel=.1)
        box('Gate_Pier',x,y,GROUND+.7,1.8,2,9.5,'Accent_Cream',.14)
        cylinder('Gate_Finial',x,y,GROUND+10.2,.22,1.8,'Accent_Bronze',segments=12)
        sphere('Gate_Light',x,y,GROUND+12,.32,'Accent_Glow')
    lower=[(-11,7.3),(-8,8.5),(-4,9.55),(0,10),(4,9.55),(8,8.5),(11,7.3)]
    upper=[(x,z+2.6) for x,z in reversed(lower)]
    profile_prism('Gate_Arch',0,y,GROUND,lower+upper,1.5,'Accent_Orange',bevel=.16)
    tube('Gate_CreamEdge',[(x,y-.81,GROUND+z) for x,z in lower],.19,'Accent_Cream')


def supporting_buildings():
    global GROUP
    buildings=[(-62,-141,0),(-82,-141,1),(-108,-140,2),(-132,-140,1),
               (-63,-106,1),(-84,-101,0),(-112,-98,2),(-131,-88,0),
               (-63,-76,0),(-86,-76,1),(-111,-76,0),
               (-63,-36,2),(-88,-33,1),(-112,-32,0),(-132,-31,1),
               (-75,26,2),(-128,80,0),(-107,83,1),(-82,83,2),
               (70,-141,4),(121,-142,2),(140,-139,1),
               (72,-98,2),(118,-99,1),(140,-96,0),
               (65,-75,0),(121,-76,2),(141,-76,1),
               (65,-35,3),(123,-36,2),(138,-12,1),(66,-9,0),
               (77,24,1),(125,26,0),(106,91,2),(137,90,1)]
    for i,(x,y,kind) in enumerate(buildings):
        GROUP='DistrictBuilding_%02d'%i
        if kind in (0,1):
            w,d,h=(14,16,9) if kind==0 else (12,15,9)
            key='Facade_HouseOchre' if kind==0 else 'Facade_HouseRed'
            loft(GROUP+'_Home',x,y,GROUND,w,d,h,'Accent_Ochre' if kind==0 else 'Accent_BrickRed',cut=.8,facade=key,bevel=.14)
            barrel(GROUP+'_CurvedRoof',x,y,GROUND+h,w+1,d+1,2.2,'Accent_RoofBrown')
            if i%3==0:box(GROUP+'_EntryHood',x,y-d/2-1,GROUND+3.8,5,2.2,.42,'RoofRed',.2)
        elif kind in (2,3):
            w,d,h=(20,19,16) if kind==2 else (18,18,16)
            loft(GROUP+'_Shop',x,y,GROUND,w,d,h,'Accent_Cream',cut=2,facade='Facade_ShopCream' if kind==2 else 'Facade_ShopGold',bevel=.16)
            roof_terrace(GROUP,x,y,GROUND+h,w,d)
        else:
            loft('Arcade_Main',x,y,GROUND,22,23,13,'Accent_Cream',cut=1,facade='Facade_ShopCream',bevel=.15)
            barrel('Arcade_BlueCanopy',x,y-12,GROUND+5,16,4,5,'Accent_Cobalt',cap='Sign_Arcade')
            for j in range(10):box('Arcade_StripedAwning',x-7.2+j*1.6,y-14,GROUND+4.6,1.6,2,.3,'Accent_Gold' if j%2 else 'Accent_Green',.035)
    return buildings


def ground_mesh():
    global GROUP
    GROUP='Ground'
    # Partition the union into disjoint cells. There is exactly one upper surface per XY.
    horizontal=[(-120,-150,154),(-60,-150,154),(5,-150,154),(64,-150,154)]
    vertical=[(0,-170,118),(100,-154,105),(-143,-154,106)]
    inlays=[(-153,-130),(-112,-68),(-52,-3),(13,56),(72,110)]
    xs={-252,-175,-150,147,154,-4.5,-4.32,4.32,4.5}
    ys={-170,-158,-20,12,26,85,108,118}
    for a,b in inlays:ys.update((a,a+.18,b-.18,b))
    for y,a,b in horizontal:xs.update((a,b));ys.update((y-6.4,y-6,y+6,y+6.4))
    for x,a,b in vertical:ys.update((a,b));xs.update((x-6.4,x-6,x+6,x+6.4))
    xs=sorted(xs);ys=sorted(ys)
    cells={}
    def inside(x,y):
        return (-150<x<154 and -170<y<118) or (-252<x<-175 and -20<y<85) or (-175<=x<=-150 and 12<y<26)
    def style(x,y):
        inner=any(a<x<b and abs(y-v)<6 for v,a,b in horizontal) or any(a<y<b and abs(x-v)<6 for v,a,b in vertical)
        edge=any(a<x<b and abs(y-v)<6.4 for v,a,b in horizontal) or any(a<y<b and abs(x-v)<6.4 for v,a,b in vertical)
        if -175<x<-150 and 12<y<26:return 0,0
        if inner and abs(x)<4.5:
            for a,b in inlays:
                if a<y<b and (abs(x)>4.32 or y<a+.18 or y>b-.18):return 1,0
        return (0,0) if inner else (1,GROUND) if edge else (3,GROUND) if x>147 or y>108 or y<-158 else (2,GROUND)
    for i in range(len(xs)-1):
        for j in range(len(ys)-1):
            x,y=(xs[i]+xs[i+1])/2,(ys[j]+ys[j+1])/2
            if inside(x,y):cells[i,j]=style(x,y)
    verts=[];faces=[];mats=[];lookup={}
    def face(coords,mat):
        ids=[]
        for co in coords:
            if co not in lookup:lookup[co]=len(verts);verts.append(co)
            ids.append(lookup[co])
        faces.append(tuple(ids));mats.append(mat)
    for (i,j),(mat,z) in cells.items():
        a,b,c,d=xs[i],ys[j],xs[i+1],ys[j+1]
        face([(a,b,z),(c,b,z),(c,d,z),(a,d,z)],mat)
        # Lower/boundary risers only; neighboring cells never duplicate a vertical face.
        for di,dj,p,q in ((-1,0,(a,d),(a,b)),(1,0,(c,b),(c,d)),(0,-1,(a,b),(c,b)),(0,1,(c,d),(a,d))):
            nz=cells.get((i+di,j+dj),(None,-.65))[1]
            if nz<z:
                levels=[nz]+([0] if nz<0<z else [])+[z]
                for low,high in zip(levels,levels[1:]):face([(*p,low),(*q,low),(*q,high),(*p,high)],1)
        face([(a,d,-.65),(c,d,-.65),(c,b,-.65),(a,b,-.65)],1)
    ob=mesh_obj('Ground_ContinuousSurface',verts,faces,['Promenade','PaleKerb','GoldenSidewalk','Accent_Green'],mats)
    ob['upper_surface_policy']='Disjoint XY partition; no coplanar terrain, bridge or road overlays'
    # Water is below the island, with no coplanar island or bridge overlap.
    GROUP='Water'
    box('Water_Basin',-60,-22,-2,470,340,.25,'Accent_Water',0)


def tree(x,y,size=1,number=0):
    cylinder('Tree_Trunk',x,y,GROUND,.25*size,2.1*size,'Accent_RoofBrown',segments=10)
    for i,(z,r,h) in enumerate(((1.2,2.1,3.1),(3.1,1.7,2.8),(4.8,1.15,2.7))):
        cylinder('Tree_Crown',x,y,GROUND+z*size,r*size,h*size,'Accent_Leaf' if (number+i)%3 else 'Accent_Green',r_top=.07*size,segments=12)


def lamp(x,y):
    cylinder('Lamp_Base',x,y,GROUND,.34,.45,'Accent_Teal',segments=12)
    cylinder('Lamp_Post',x,y,GROUND+.45,.10,3.35,'Accent_Teal',segments=12)
    cylinder('Lamp_Collar',x,y,GROUND+3.7,.32,.17,'Accent_Gold',segments=16)
    sphere('Lamp_Globe',x,y,GROUND+4.2,.57,'Accent_Glow')
    cylinder('Lamp_Top',x,y,GROUND+4.73,.24,.12,'Accent_Teal',segments=12)
    data=bpy.data.lights.new('Goldenrod_Lantern','POINT');data.energy=60;data.color=(1,.65,.23);data.shadow_soft_size=.55
    ob=bpy.data.objects.new('LGT_Goldenrod_Lantern',data);bpy.context.scene.collection.objects.link(ob);ob.location=(x,y,GROUND+4.15)


def planter(x,y,w=5,d=2.2):
    box('Planter_Stone',x,y,GROUND,w,d,.7,'Accent_Cream',.12)
    box('Planter_Foliage',x,y,GROUND+.68,w-.35,d-.35,.5,'Accent_Green',.25)
    for i in range(6):
        xx=x-w*.36+(i%3)*w*.36;yy=y+(-.4 if i<3 else .4)
        sphere('Planter_Flowers',xx,yy,GROUND+1.23,.22,'Accent_Pink' if i%2 else 'Accent_Orange')


def street_props():
    global GROUP
    GROUP='StreetLamps'
    for y in (-137,-112,-81,-44,-12,20,58,84,108):
        for x in (-8.7,8.7):lamp(x,y)
    for x in (-140,-105,-83,-42,30,67,103,143):lamp(x,-3)
    for y in (-12,18,49,75):lamp(-246,y)
    GROUP='Planters'
    for x,y in ((-9,-137),(9,-137),(-10,-74),(10,-74),(-12,9),(-53,9),(24,69),(80,69),(-239,-14),(-190,-14)):
        planter(x,y)
    GROUP='Benches'
    for x,y in ((-14,-47),(14,22),(-56,8),(-192,62),(-240,62),(89,69)):
        box('Bench_Seat',x,y,GROUND+.45,2.4,.6,.14,'Accent_RoofBrown',.06)
        box('Bench_Back',x,y+.27,GROUND+.55,2.4,.12,.55,'Accent_Teal',.04)
        for dx in (-.9,.9):box('Bench_Leg',x+dx,y,GROUND,.12,.5,.45,'Accent_Charcoal',.02)
    GROUP='CoastBollards'
    for x in range(-249,-177,5):
        for y in (-18,83):cylinder('Coast_Bollard',x,y,GROUND,.38,1.1,'Accent_RoofBrown',segments=10)
    for y in range(-13,83,5):cylinder('Coast_Bollard',-250,y,GROUND,.38,1.1,'Accent_RoofBrown',segments=10)
    GROUP='Trees'
    for i,x in enumerate(range(-144,151,7)):
        if abs(x)>15:tree(x,113,.95,i)
    for i,y in enumerate(range(-162,109,7)):tree(150,y,.85,i)
    for i,x in enumerate(range(-144,149,12)):
        if abs(x)>17:tree(x,-165,.8,i)
    for i,(x,y) in enumerate(((-144,-108),(-143,-45),(-141,57),(-137,107),(112,108),(8,101),(-6,69),(13,109))):tree(x,y,1,i)
    GROUP='Fountains'
    for x in (-237,-191):
        y=-5
        cylinder('Fountain_PlazaRing',x,y,GROUND,4.2,.18,'Accent_RoofBrown',segments=48)
        cylinder('Fountain_StoneBowl',x,y,GROUND+.18,2.55,.8,'Accent_Cream',r_top=2.8,segments=48)
        cylinder('Fountain_Water',x,y,GROUND+1,2.5,.05,'Accent_Water',segments=48)
        cylinder('Fountain_Pedestal',x,y,GROUND+1,.55,1.1,'Accent_Cream',segments=24)
        sphere('Fountain_Jet',x,y,GROUND+2.5,.45,'Accent_CyanGlow')
    GROUP='PokeTent'
    cylinder('Tent_Base',-17,94,GROUND,7.5,4.6,'Accent_Gold',segments=8)
    cylinder('Tent_Roof',-17,94,GROUND+4.6,8.3,7,'Accent_Pink',r_top=.1,segments=8)
    for z,r in ((5.6,7.1),(7.6,4.8),(9.6,2.4)):
        cylinder('Tent_GoldBand',-17,94,GROUND+z,r,.38,'Accent_Gold',r_top=r-.3,segments=8)
    box('Tent_Entry',-17,86.7,GROUND,3,1,3.5,'Accent_Pink',.08,front='DoorBlue')


def lighting():
    scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=64;scene.cycles.use_denoising=True
    prefs=bpy.context.preferences.addons['cycles'].preferences;prefs.compute_device_type='OPTIX';prefs.get_devices()
    for dev in prefs.devices:dev.use=dev.type=='OPTIX'
    scene.cycles.device='GPU';scene.cycles.max_bounces=6
    world=bpy.data.worlds.new('WORLD_R02_GoldenrodDay');world.use_nodes=True;scene.world=world
    nodes=world.node_tree.nodes;nodes.clear()
    sky=nodes.new('ShaderNodeTexSky');sky.sky_type='MULTIPLE_SCATTERING';sky.sun_elevation=math.radians(27);sky.sun_rotation=math.radians(210)
    sky.sun_disc=True;sky.sun_intensity=.7;sky.sun_size=math.radians(1.2)
    bg=nodes.new('ShaderNodeBackground');bg.inputs['Strength'].default_value=.26
    out=nodes.new('ShaderNodeOutputWorld');world.node_tree.links.new(sky.outputs[0],bg.inputs[0]);world.node_tree.links.new(bg.outputs[0],out.inputs[0])
    scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.view_settings.exposure=-1.6
    scene.render.resolution_x=1800;scene.render.resolution_y=1200;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG'


def cameras():
    specs=[('01_SouthGate',(0,-161,1.75),(-1,-76,7.5),24),
           ('02_CenterDepartment',(0,-126,1.75),(2,-94,7.3),20),
           ('03_Station',(-22,-29,1.75),(-32,24,9),23),
           ('04_Gym',(16,61,1.75),(48,86,6.5),24),
           ('05_Terminal',(-240,-14,1.75),(-215,31,30),20),
           ('06_Radio',(-88,-10,1.75),(-113,26,24),19),
           ('07_Aerial',(330,-385,360),(-35,-18,0),43),
           ('08_Promenade',(2,-73,1.75),(2,-40,1.3),30),
           ('09_Railway',(16,23,1.75),(43,88,8),25),
           ('10_CenterClose',(-2,-124,1.75),(-22,-96,4.8),28)]
    for name,pos,target,lens in specs:
        data=bpy.data.cameras.new('CAM_R02_'+name);data.lens=lens;data.clip_start=.15;data.clip_end=1500
        ob=bpy.data.objects.new(data.name,data);bpy.context.scene.collection.objects.link(ob);ob.location=pos
        ob.rotation_euler=(Vector(target)-ob.location).to_track_quat('-Z','Y').to_euler()
    bpy.context.scene.camera=bpy.data.objects['CAM_R02_01_SouthGate']


def finalize_geometry():
    # Export-ready mesh datablocks; no font, curve, camera proxy or skeleton is exported.
    for ob in list(bpy.context.scene.objects):
        if ob.type not in ('MESH','CURVE'):continue
        bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
        if ob.type=='CURVE':
            bpy.ops.object.convert(target='MESH');ob=bpy.context.object
            uv_mesh(ob.data,list(ob.data.materials))
        for mod in list(ob.modifiers):bpy.ops.object.modifier_apply(modifier=mod.name)
        bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
        bm=bmesh.new();bm.from_mesh(ob.data)
        boundary=[v for v in bm.verts if v.is_boundary]
        if boundary:bmesh.ops.remove_doubles(bm,verts=boundary,dist=.00001)
        bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(ob.data);bm.free()
    groups={}
    for ob in list(bpy.context.scene.objects):
        if ob.type=='MESH':groups.setdefault(ob['city_group'],[]).append(ob)
    for name,objects in groups.items():
        bpy.ops.object.select_all(action='DESELECT')
        for ob in objects:ob.select_set(True)
        bpy.context.view_layer.objects.active=objects[0]
        if len(objects)>1:bpy.ops.object.join()
        ob=bpy.context.object;ob.name='SM_R02_'+name
        bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY',center='BOUNDS')
    return groups


def build():
    OUT.mkdir(parents=True,exist_ok=True);EXPORT.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene=bpy.context.scene;scene.name='Goldenrod_Reference_R02'
    scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
    for key,tile in (('Promenade',4),('GoldenSidewalk',4),('PaleKerb',1)):material(key,tile)
    material('Accent_Glow',2,1.4);material('Accent_CyanGlow',2,1.2)
    ground_mesh();center();department();radio();station();gym();terminal();south_gate()
    buildings=supporting_buildings();street_props()
    finalize_geometry();lighting();cameras()
    for screen in bpy.data.screens:
        for area in screen.areas:
            for space in area.spaces:
                if space.type=='VIEW_3D':
                    space.clip_start=.5;space.clip_end=2000
                    space.overlay.show_extras=False
                    space.shading.type='MATERIAL'
    scene.eevee.taa_samples=64
    scene['reference']='Local Avery Plummer Goldenrod comparison images 3-8 + user floor reference'
    scene['art_direction']='Pokemon Goldenrod identity and silhouette first; human metric construction second'
    scene['characters']='No NPC, mannequin, human scale proxy or armature is present'
    scene['windows']='All window, frame and door appearance comes from image texture files on structural polygons'
    for im in bpy.data.images:
        if im.source=='FILE':im.pack()
    meshes=[ob for ob in scene.objects if ob.type=='MESH']
    report={'scene':scene.name,'mesh_objects':len(meshes),'triangles':sum(len(ob.data.loop_triangles) for ob in meshes),
            'npc_proxy_armature_objects':[o.name for o in scene.objects if o.type=='ARMATURE' or o.get('camera_proxy')],
            'materials':len(MATS),'packed_images':len([im for im in bpy.data.images if im.source=='FILE']),
            'ground':'One disjoint cell partition for city/peninsula/bridge; roads and curbs are not overlapping overlays',
            'window_geometry':'No independent window, backing, pane or mullion geometry is generated',
            'reference_correction':'Office tower ring removed; compact reference street relationships and restored hero profiles',
            'supporting_buildings':len(buildings)}
    assert not report['npc_proxy_armature_objects']
    for ob in meshes:
        ob.data.calc_loop_triangles()
        assert ob.data.uv_layers and ob.material_slots,ob.name
    report['triangles']=sum(len(o.data.loop_triangles) for o in meshes)
    (OUT/'Goldenrod_R02_Build_QA.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    (OUT/'Goldenrod_R02_Materials.json').write_text(json.dumps([
        {'material':mat.name,'prefix':key,'tile_m':mat['tile_m'],'emission':mat['emission_strength'],
         'normal_strength':next(n for n in mat.node_tree.nodes if n.type=='NORMAL_MAP').inputs['Strength'].default_value,
         'coat_weight':next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED').inputs['Coat Weight'].default_value,
         'textures':{channel:str(TEX/(key+'_'+channel+'.png')) for channel in ('BaseColor','Roughness','Normal','Metallic')}}
        for key,mat in MATS.items()],indent=2),encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Goldenrod_City_R02.blend'),compress=True)
    print('R02_BUILD',json.dumps(report),flush=True)


def render():
    scene=bpy.context.scene
    at=sys.argv.index('--render');selected=sys.argv[at+1].split(',') if at+1<len(sys.argv) else []
    preview=OUT/'Previews';preview.mkdir(exist_ok=True)
    for ob in sorted((o for o in scene.objects if o.type=='CAMERA'),key=lambda o:o.name):
        short=ob.name.removeprefix('CAM_R02_')
        if selected and short[:2] not in selected:continue
        scene.camera=ob;scene.render.filepath=str(preview/('GR_R02_'+short+'.png'))
        bpy.ops.render.render(write_still=True);print('R02_RENDERED',short,flush=True)


def export():
    scene=bpy.context.scene
    bpy.ops.object.select_all(action='DESELECT')
    for ob in scene.objects:
        if ob.type=='MESH':ob.select_set(True)
    options=dict(use_selection=True,object_types={'MESH'},global_scale=1,apply_unit_scale=True,
                 apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',bake_space_transform=False,
                 use_mesh_modifiers=True,use_triangles=True,use_tspace=True,mesh_smooth_type='FACE',
                 bake_anim=False,add_leaf_bones=False,path_mode='RELATIVE',embed_textures=False)
    bpy.ops.export_scene.fbx(filepath=str(EXPORT/'Goldenrod_City_Modular.fbx'),**options)
    bpy.context.view_layer.objects.active=next(o for o in scene.objects if o.type=='MESH')
    bpy.ops.object.join();ob=bpy.context.object;ob.name='SM_Goldenrod_City'
    scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    bpy.ops.export_scene.fbx(filepath=str(EXPORT/'Goldenrod_City.fbx'),**options)
    # A native snapshot of exactly the combined export makes round-trip checks meaningful.
    bpy.ops.wm.save_as_mainfile(filepath=str(EXPORT/'Goldenrod_Export_Source.blend'),compress=True)
    print('R02_FBX_EXPORTED',flush=True)


if __name__=='__main__':
    if '--render' in sys.argv:render()
    elif '--export' in sys.argv:export()
    else:build()
