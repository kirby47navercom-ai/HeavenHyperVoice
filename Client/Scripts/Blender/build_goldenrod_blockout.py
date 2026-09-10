"""Goldenrod metric planning / native Blender graybox. No generated assets or materials.

Plan: python build_goldenrod_blockout.py --plan-only
Build: blender --background --factory-startup --python build_goldenrod_blockout.py
Render saved scene: blender --background Goldenrod_Graybox.blend --python this.py -- --render
"""
import json
import math
import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'SourceArt' / 'Environments' / 'Goldenrod'
COLLECTIONS = ['00_REFERENCE', '01_BLOCKOUT', '02_ROADS', '03_HERO_BUILDINGS',
               '04_MODULAR_BUILDINGS', '05_PROPS', '06_VEGETATION', '07_DECALS',
               '08_COLLISION', '09_EXPORT']


def overlaps(a, b, margin=0):
    return a[0] < b[2]+margin and a[2] > b[0]-margin and a[1] < b[3]+margin and a[3] > b[1]-margin


def footprint(x, y, w, d):
    return [x-w/2, y-d/2, x+w/2, y+d/2]


def make_plan():
    rng = random.Random(20260910)
    p = {'title': 'GOLDENROD / METROPOLITAN REINTERPRETATION', 'revision': 'B02',
         'units': 'meters', 'north': '+Y', 'core_bounds': [-320, -320, 320, 320],
         'player_height': 1.75, 'camera_eye_height': 1.65, 'camera_follow_distance': 4.2,
         'stage': 'GRAYBOX ONLY — no final materials, windows, props, NPC systems or UE integration',
         'roads': [], 'buildings': [], 'spaces': [], 'routes': [], 'cameras': [],
         'districts': [
             {'id':'D1','label':'STATION / TRANSIT','at':[-80,135]},
             {'id':'D2','label':'DEPARTMENT / RETAIL','at':[80,-35]},
             {'id':'D3','label':'RADIO / CIVIC','at':[-230,118]},
             {'id':'D4','label':'MAIN SHOPPING STREET','at':[80,95]},
             {'id':'D5','label':'ENTERTAINMENT','at':[-85,-35]},
             {'id':'D6','label':'MIXED USE','at':[235,120]},
             {'id':'D7','label':'RESIDENTIAL EDGE','at':[235,-225]},
             {'id':'D8','label':'GYM / FLOWER STREET','at':[85,280]},
             {'id':'D9','label':'ALLEYS / SERVICE','at':[-230,-50]}]}
    xs = [-305, -160, 0, 160, 305]
    ys = [-305, -130, 40, 210, 305]
    for x in xs:
        p['roads'].append({'name': 'Central Boulevard' if x==0 else f'North South {x}',
                           'rect':[x-(11 if x==0 else 7),-320,x+(11 if x==0 else 7),320],
                           'width':22 if x==0 else 14, 'sidewalk':3 if x==0 else 2.5,
                           'axis':'Y', 'kind':'main' if x==0 else 'secondary'})
    for y in ys:
        p['roads'].append({'name':'Media Avenue' if y==40 else f'East West {y}',
                           'rect':[-320,y-(11 if y==40 else 7),320,y+(11 if y==40 else 7)],
                           'width':22 if y==40 else 14, 'sidewalk':3 if y==40 else 2.5,
                           'axis':'X', 'kind':'main' if y==40 else 'secondary'})
    heroes = [
        ('RadioTower', 'RADIO TOWER', -218,166,54,46,146,36,'D3',[-260,123,-178,200]),
        ('Station', 'GOLDENROD STATION', -83,170,108,48,27,5,'D1',[-145,104,-21,200]),
        ('DepartmentStore','DEPARTMENT STORE',60,-85,64,56,42,10,'D2',[17,-122,111,-32]),
        ('Gym','GOLDENROD GYM',83,252,45,55,17,4,'D8',[47,219,126,293]),
        ('PokemonCenter','POKEMON CENTER',-29,-85,25,35,12,3,'D5',[-47,-122,-12,-54])]
    for key,label,x,y,w,d,h,f,dist,res in heroes:
        p['buildings'].append({'name':key,'label':label,'x':x,'y':y,'w':w,'d':d,'h':h,
                               'floors':f,'district':dist,'hero':True,'rect':footprint(x,y,w,d),
                               'reserve':res})
    # Secondary reference silhouettes remain coarse masses, not additional final hero assets.
    for key,label,x,y,w,d,h,f,dist in [
            ('GlobalTerminal','GLOBAL TERMINAL',-366,144,44,40,62,12,'D3'),
            ('FlowerShop','FLOWER SHOP',129,252,24,24,9,2,'D8'),
            ('ShoppingArcade','COVERED SHOPPING',74,-207,24,68,12,2,'D4')]:
        p['buildings'].append({'name':key,'label':label,'x':x,'y':y,'w':w,'d':d,'h':h,
            'floors':f,'district':dist,'hero':False,'secondary':True,
            'rect':footprint(x,y,w,d),'reserve':footprint(x,y,w+6,d+6)})
    reserved = [b['reserve'] for b in p['buildings']]
    # Local video 00:27: rail connects directly to the station's east flank.
    p['rail']={'rect':[-29,180,340,198],'deck_top':9.5,'clearance':8.0}
    reserved.append([-29,176,320,202])
    p['coast']={'water_rect':[-450,-410,-280,410],
                'peninsula_rect':[-408,75,-328,174], 'bridge_rect':[-336,88,-280,104]}
    p['gateway']={'at':[0,-266],'clear_width':24,'clear_height':9,'height':12,'depth':3}
    reserved.append([-16,-269,16,-263])
    spaces = [
        ('Media Plaza',[-260,103,-172,142],'plaza'),
        ('Media Sightline',[-224,51,-212,104],'plaza'),
        ('Station Forecourt',[-143,108,-23,145],'plaza'),
        ('Station Street Connection',[-23,124,-11,136],'plaza'),
        ('Center Forecourt',[-16.5,-109,-11,-62],'plaza'),
        ('Department Forecourt',[12,-119,27,-48],'plaza'),
        ('Services Approach',[-42,-169,42,-138],'plaza'),
        ('Gym Forecourt',[50,219,124,224],'plaza'),
        ('Gym West Approach',[12,236,60.5,268],'plaza'),
        ('Terminal Waterfront',[-408,75,-328,121],'plaza'),
        ('Waterfront Connection',[-336,88,-260,104],'plaza'),
        ('East Neighborhood Park',[201,-84,280,-10],'park'),
        ('North Residential Garden',[-278,243,-190,286],'park')]
    for name,rect,kind in spaces:
        p['spaces'].append({'name':name,'rect':rect,'kind':kind})
        reserved.append(rect)
    # A planned urban street hierarchy first, then deterministic parcel variation.
    # Service lanes traverse each block; the large station and hero precincts take precedence.
    for ix in range(4):
        for iy in range(4):
            x0=xs[ix]+(11 if xs[ix]==0 else 7)+2
            x1=xs[ix+1]-(11 if xs[ix+1]==0 else 7)-2
            y0=ys[iy]+(11 if ys[iy]==40 else 7)+2
            y1=ys[iy+1]-(11 if ys[iy+1]==40 else 7)-2
            mid=(x0+x1)/2
            alley=[mid-3,y0-2,mid+3,y1+2]
            if not any(overlaps(alley,r) for r in reserved):
                p['roads'].append({'name':f'Service {ix+1}{iy+1}','rect':alley,
                                   'width':6,'sidewalk':0,'axis':'Y','kind':'alley'})
                reserved.append(alley)
            cross=[x0-2,(y0+y1)/2-3,x1+2,(y0+y1)/2+3]
            if not any(overlaps(cross,r) for r in reserved if r != alley):
                p['roads'].append({'name':f'Cross Lane {ix+1}{iy+1}','rect':cross,
                                   'width':6,'sidewalk':0,'axis':'X','kind':'alley'})
                reserved.append(cross)
            dist = ('D3' if ix==0 and iy==2 else 'D1' if ix==1 and iy==2 else
                    'D8' if ix==2 and iy==3 else 'D2' if ix==2 and iy==1 else
                    'D5' if ix==1 and iy==1 else 'D4' if ix==2 and iy==2 else
                    'D7' if iy==0 or iy==3 else 'D9' if ix==0 else 'D6')
            # Perimeter frontage parcels, service/courtyard space in the center.
            candidates=[]
            for edge in ('south','north'):
                cursor=x0
                while cursor+16<x1:
                    w=min(rng.choice([16,20,24,28,32]),x1-cursor)
                    d=min(rng.choice([20,24,28]),(y1-y0-10)/2)
                    cy=y0+d/2 if edge=='south' else y1-d/2
                    candidates.append((cursor+w/2,cy,w,d))
                    cursor+=w+rng.choice([2,4])
            if y1-y0>90:
                for edge in ('west','east'):
                    cy=y0+40
                    while cy+12<y1-30:
                        w=rng.choice([20,24]); d=22
                        cx=x0+w/2 if edge=='west' else x1-w/2
                        candidates.append((cx,cy,w,d)); cy+=28
            # Fine-grain infill behind the street frontage; reserved lanes keep it accessible.
            for cy in range(math.ceil(y0+53), math.floor(y1-39),32):
                for cx in range(math.ceil(x0+43),math.floor(x1-25),30):
                    candidates.append((cx,cy,24,24))
            for cx,cy,w,d in candidates:
                rect=footprint(cx,cy,w,d)
                if any(overlaps(rect,r,1) for r in reserved): continue
                if any(overlaps(rect,b['rect'],1) for b in p['buildings']): continue
                floors=rng.choice([2,3,4,5]) if dist in ('D7','D9') else rng.choice([4,5,6,7,9,10])
                if dist in ('D1','D6') and rng.random()<.30: floors=rng.choice([11,14,18,20])
                h=4.5+(floors-1)*3.6
                p['buildings'].append({'name':f'Block_{ix+1}{iy+1}_{len(p["buildings"]):03d}',
                    'x':round(cx,2),'y':round(cy,2),'w':w,'d':d,'h':round(h,2),
                    'floors':floors,'district':dist,'hero':False,'rect':rect,
                    'setback':floors>=7,'roof_family':'hip' if floors<=3 else 'chamfer',
                    'context':False})
    # Distant continuation east/north/south only. Western edge stays coastal.
    for side in ('N','E','S'):
        for i in range(12):
            a=-280+i*52
            cx,cy=(a,367) if side=='N' else (367,a) if side=='E' else (a,-367)
            w,d=32,30
            p['buildings'].append({'name':f'Context_{side}_{i:02d}','x':cx,'y':cy,
                'w':w,'d':d,'h':rng.choice([28,36,44,52,64,76]),'floors':0,
                'district':'CONTEXT','hero':False,'context':True,'rect':footprint(cx,cy,w,d)})
    p['routes']=[
        {'name':'ARRIVAL / SERVICES','points':[[9.5,-305],[9.5,-85],[-12,-85]],'width':2},
        {'name':'RETAIL / STATION','points':[[12,-85],[9.5,130],[-83,130],[-83,145]],'width':2},
        {'name':'MEDIA / WATERFRONT','points':[[-83,130],[-218,130],[-218,101],[-295,101],[-295,96],[-366,96],[-366,121]],'width':2},
        {'name':'GYM CONNECTION','points':[[-83,126],[9.5,126],[9.5,220],[83,220],[83,224]],'width':2}]
    p['cameras']=[
        {'name':'01_SouthArrival','pos':[0,-284,1.65],'target':[0,189,13], 'lens':28,
         'caption':'SOUTH GATE > CENTER / DEPARTMENT > ELEVATED RAIL | 1.65m eye'},
        {'name':'02_CenterDepartment','pos':[0,-187,1.65],'target':[12,-80,16], 'lens':17.5,
         'caption':'CENTER / DEPARTMENT | paired services across boulevard'},
        {'name':'03_MediaTower','pos':[-218,40,1.65],'target':[-218,166,40], 'lens':14,
         'caption':'MEDIA AVENUE | 146m radio tower above ordinary city blocks'},
        {'name':'04_StationForecourt','pos':[-63,85,1.65],'target':[-83,170,12], 'lens':18,
         'caption':'STATION FORECOURT | 108m station | human scale'},
        {'name':'05_GymApproach','pos':[36,220,1.8],'target':[88,252,7], 'lens':22,
         'caption':'GYM APPROACH | 45 x 55m arena'},
        {'name':'06_ServiceAlley','pos':[230,-282,1.65],'target':[230,-190,4], 'lens':24,
         'caption':'RESIDENTIAL EDGE | human scale street canyon'},
        {'name':'07_Aerial','pos':[800,-1050,800],'target':[0,25,15],'lens':42,
         'caption':'CITY STRUCTURE | 640 x 640m core | distant context'},
        {'name':'08_Masterplan','pos':[0,0,900],'target':[0,0,0], 'lens':40,'ortho':1320,
         'caption':'NORTH +Y | orthographic plan'},
        {'name':'09_Waterfront','pos':[-398,79,1.8],'target':[-302,159,25], 'lens':16,'proxy':True,
         'caption':'TERMINAL / RADIO / STATION | waterfront identity'},
        {'name':'10_RailUnderpass','pos':[9.5,160,1.8],'target':[83,252,7], 'lens':24,'proxy':True,
         'caption':'8m UNDERPASS | northern gym connection'}]
    p['reference_status']='Six unique local JPGs and 55.533s showcase video visually inspected on 2026-09-10.'
    for route in p['routes']:
        length=sum(math.dist(a,b) for a,b in zip(route['points'],route['points'][1:]))
        route.update(length_m=round(length,1),walk_seconds=round(length/2.6,1),run_seconds=round(length/3.9,1))
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'Goldenrod_Masterplan.json').write_text(json.dumps(p,indent=2),encoding='utf-8')
    print('PLAN',len(p['buildings']),'buildings;',len(p['roads']),'road segments')
    return p


def build(p):
    import bpy
    from mathutils import Vector
    # Run in a dedicated --factory-startup process. Never clear an interactive user scene.
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
    for mat in list(bpy.data.materials): bpy.data.materials.remove(mat)
    for coll in list(bpy.data.collections): bpy.data.collections.remove(coll)
    scene=bpy.context.scene
    scene.name='GR_Graybox_'+p['revision']
    scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=1.0
    scene.unit_settings.length_unit='METERS'
    coll={n:bpy.data.collections.new(n) for n in COLLECTIONS}
    for c in coll.values(): scene.collection.children.link(c)
    mesh_cache={}
    def mesh_object(name,verts,faces,collection,color,location=(0,0,0),cache_key=None):
        mesh=mesh_cache.get(cache_key) if cache_key else None
        if mesh is None:
            mesh=bpy.data.meshes.new(name+'_Geo'); mesh.from_pydata(verts,[],faces); mesh.update()
            if cache_key: mesh_cache[cache_key]=mesh
        ob=bpy.data.objects.new(name,mesh); coll[collection].objects.link(ob)
        ob.location=location
        gray=min(.9,color*1.15+.10);ob.color=(gray,gray,gray,1)
        ob['stage']='BLOCKOUT'; return ob
    def box(name,x,y,z,w,d,h,collection='01_BLOCKOUT',color=.56):
        verts=[(-w/2,-d/2,0),(w/2,-d/2,0),(w/2,d/2,0),(-w/2,d/2,0),
               (-w/2,-d/2,h),(w/2,-d/2,h),(w/2,d/2,h),(-w/2,d/2,h)]
        faces=[(3,2,1,0),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7),(4,5,6,7)]
        return mesh_object(name,verts,faces,collection,color,(x,y,z),('box',w,d,h))
    def cylinder(name,x,y,z,r,h,color=.62,segments=20,collection='01_BLOCKOUT'):
        verts=[(r*math.cos(i*2*math.pi/segments),r*math.sin(i*2*math.pi/segments),k*h)
               for k in (0,1) for i in range(segments)]
        faces=[tuple(reversed(range(segments))),tuple(range(segments,2*segments))]
        faces.extend((i,(i+1)%segments,(i+1)%segments+segments,i+segments) for i in range(segments))
        return mesh_object(name,verts,faces,collection,color,(x,y,z),('cyl',r,h,segments))
    def loft(name,x,y,z,w,d,h,top_w=None,top_d=None,cut=0,color=.56):
        # Closed chamfered/tapered mass. Pivots remain at the bottom center.
        def ring(w,d,c):
            c=min(c,w/4,d/4)
            return [(-w/2+c,-d/2),(w/2-c,-d/2),(w/2,-d/2+c),(w/2,d/2-c),
                    (w/2-c,d/2),(-w/2+c,d/2),(-w/2,d/2-c),(-w/2,-d/2+c)]
        tw=top_w if top_w is not None else w;td=top_d if top_d is not None else d
        if cut==0:cut=.001
        verts=[(a,b,0) for a,b in ring(w,d,cut)]+[(a,b,h) for a,b in ring(tw,td,cut)]
        faces=[tuple(reversed(range(8))),tuple(range(8,16))]
        faces.extend((i,(i+1)%8,(i+1)%8+8,i+8) for i in range(8))
        return mesh_object(name,verts,faces,'01_BLOCKOUT',color,(x,y,z),('loft',w,d,h,tw,td,cut))
    def cone(name,x,y,z,r1,r2,h,color=.62,segments=24):
        verts=[(r*math.cos(i*2*math.pi/segments),r*math.sin(i*2*math.pi/segments),k*h)
               for k,r in enumerate((r1,r2)) for i in range(segments)]
        faces=[tuple(reversed(range(segments))),tuple(range(segments,2*segments))]
        faces.extend((i,(i+1)%segments,(i+1)%segments+segments,i+segments) for i in range(segments))
        return mesh_object(name,verts,faces,'01_BLOCKOUT',color,(x,y,z),('cone',r1,r2,h,segments))
    def profile(name,x,y,z,cross,d,color=.67):
        verts=[(a,b,c) for b in (-d/2,d/2) for a,c in cross]; m=len(cross)
        faces=[tuple(range(m)),tuple(reversed(range(m,2*m)))]
        faces.extend((i,i+m,(i+1)%m+m,(i+1)%m) for i in range(m))
        return mesh_object(name,verts,faces,'01_BLOCKOUT',color,(x,y,z))
    def roof(name,x,y,z,w,d,rise):
        n=12
        # Solid shallow barrel volume, no facade/window/structural detail.
        cross=[(-w/2,0),(w/2,0)]+[(w/2*math.cos(i*math.pi/n),rise*math.sin(i*math.pi/n)) for i in range(1,n)]
        return profile(name,x,y,z,cross,d)
    # All gray values are solid-viewport object display colors, zero material slots.
    box('SM_GR_Ground_Blockout',45,0,-.5,730,820,.5,'02_ROADS',.43)
    for key,z,h,color in [('water_rect',-.8,.2,.24),('peninsula_rect',-.5,.5,.43),
                          ('bridge_rect',-.4,.4,.43)]:
        a,b,c,d=p['coast'][key]
        box('SM_GR_Coast_'+key,(a+c)/2,(b+d)/2,z,c-a,d-b,h,'02_ROADS',color)
    # Union of axis-aligned road rectangles as one closed, welded mesh. This avoids
    # coplanar faces / flicker at intersections without hidden stacked road slabs.
    gx=sorted({r['rect'][k] for r in p['roads'] for k in (0,2)})
    gy=sorted({r['rect'][k] for r in p['roads'] for k in (1,3)})
    cells=set()
    for ix in range(len(gx)-1):
        for iy in range(len(gy)-1):
            cx=(gx[ix]+gx[ix+1])/2;cy=(gy[iy]+gy[iy+1])/2
            if any(r['rect'][0]<=cx<=r['rect'][2] and r['rect'][1]<=cy<=r['rect'][3] for r in p['roads']):
                cells.add((ix,iy))
    verts=[];faces=[];index={}
    def face(coords):
        f=[]
        for v in coords:
            if v not in index:index[v]=len(verts);verts.append(v)
            f.append(index[v])
        faces.append(tuple(f))
    for ix,iy in sorted(cells):
        a,c=gx[ix],gx[ix+1];b,d=gy[iy],gy[iy+1];lo=.001;hi=.01
        face([(a,b,lo),(a,d,lo),(c,d,lo),(c,b,lo)])
        face([(a,b,hi),(c,b,hi),(c,d,hi),(a,d,hi)])
        if (ix,iy-1) not in cells:face([(a,b,lo),(c,b,lo),(c,b,hi),(a,b,hi)])
        if (ix+1,iy) not in cells:face([(c,b,lo),(c,d,lo),(c,d,hi),(c,b,hi)])
        if (ix,iy+1) not in cells:face([(c,d,lo),(a,d,lo),(a,d,hi),(c,d,hi)])
        if (ix-1,iy) not in cells:face([(a,d,lo),(a,b,lo),(a,b,hi),(a,d,hi)])
    mesh_object('SM_GR_RoadNetwork_Blockout',verts,faces,'02_ROADS',.30)
    for i,r in enumerate(p['roads']):
        a,b,c,d=r['rect']; sw=r['sidewalk']; w=c-a; dep=d-b
        if sw:
            # Sidewalks are segmented at cross streets so intersections stay traversable.
            cuts=[]
            for q in p['roads']:
                if q['axis']==r['axis'] or q['kind']=='alley': continue
                qr=q['rect']; cuts.append((qr[1],qr[3]) if r['axis']=='Y' else (qr[0],qr[2]))
            start,end=(b,d) if r['axis']=='Y' else (a,c)
            runs=[]; cursor=start
            for ca,cb in sorted(cuts):
                if ca>cursor: runs.append((cursor,min(ca,end)))
                cursor=max(cursor,cb)
            if cursor<end:runs.append((cursor,end))
            for j,(lo,hi) in enumerate(runs):
                if hi<=lo:continue
                for side in (0,1):
                    if r['axis']=='Y':
                        sx=a+sw/2 if side==0 else c-sw/2
                        box(f'SM_GR_Sidewalk_{i}_{j}_{side}',sx,(lo+hi)/2,0,sw,hi-lo,.15,'02_ROADS',.52)
                    else:
                        sy=b+sw/2 if side==0 else d-sw/2
                        box(f'SM_GR_Sidewalk_{i}_{j}_{side}',(lo+hi)/2,sy,0,hi-lo,sw,.15,'02_ROADS',.52)
    # Clip plaza slabs against roads and other slabs: no coplanar overlaps at connections.
    surface_obstacles=[r['rect'] for r in p['roads']]
    for s in p['spaces']:
        pieces=[s['rect']]
        for cut in surface_obstacles:
            remaining=[]
            for a,b,c,d in pieces:
                if not overlaps([a,b,c,d],cut):remaining.append([a,b,c,d]);continue
                x0,y0,x1,y1=max(a,cut[0]),max(b,cut[1]),min(c,cut[2]),min(d,cut[3])
                remaining.extend(q for q in [[a,b,x0,d],[x1,b,c,d],[x0,b,x1,y0],[x0,y1,x1,d]]
                                 if q[2]-q[0]>.001 and q[3]-q[1]>.001)
            pieces=remaining
        for i,(a,b,c,d) in enumerate(pieces):
            box('SM_GR_Reserve_'+s['name'].replace(' ','_')+f'_{i}',(a+c)/2,(b+d)/2,0,c-a,d-b,.15,
                '01_BLOCKOUT',.49 if s['kind']=='park' else .59)
        surface_obstacles.append(s['rect'])
    for b in p['buildings']:
        x,y,w,d,h=b['x'],b['y'],b['w'],b['d'],b['h']; name='SM_GR_'+b['name']
        parts=[]
        if b['hero'] or b.get('secondary'):
            if b['name']=='RadioTower':
                parts=[loft(name+'_Podium',x,y,0,w,d,15,cut=2,color=.48),
                       loft(name+'_Shaft',x,y,15,28,28,108,24,24,1.5,.54),
                       cone(name+'_CrownShoulder',x,y,123,12,20,4),
                       cylinder(name+'_BroadcastCrown',x,y,127,20,6,segments=24),
                       cone(name+'_CrownRoof',x,y,133,21.5,19,2,color=.52),
                       cylinder(name+'_Mast',x,y,135,1,11,color=.48,segments=12)]
            elif b['name']=='Station':
                parts=[box(name+'_WestConcourse',x-30,y,0,48,d,4.5,color=.61),
                       box(name+'_EastConcourse',x+30,y,0,48,d,4.5,color=.61),
                       box(name+'_EntryRecess',x,y+2,0,12,d-4,4.5,color=.51),
                       loft(name+'_Hall',x,y,4.5,w,d,13.5,cut=2,color=.61),
                       roof(name+'_Vault_A',x-27,y,18,54,d,9),
                       roof(name+'_Vault_B',x+27,y,18,54,d,9)]
            elif b['name']=='DepartmentStore':
                parts=[loft(name+'_RetailPodium',x,y,0,w,d,12,cut=4,color=.58),
                       loft(name+'_Upper',x+3,y+2,12,w-6,d-4,27,cut=3,color=.55),
                       loft(name+'_Roofline',x,y,39,w,d,3,cut=4,color=.64)]
            elif b['name']=='Gym':
                parts=[loft(name+'_Arena',x,y+2,0,w,d-4,9,cut=5,color=.58),
                       roof(name+'_Vault',x,y+2,9,w,d-4,8),
                       box(name+'_EntryMass',x,y-d/2+3,0,12,6,5,color=.52)]
            elif b['name']=='PokemonCenter':
                parts=[loft(name+'_Clinic',x-1,y,0,w-2,d,10.5,cut=3,color=.64),
                       loft(name+'_Roof',x,y,10.5,w,d,1.5,w-2,d-2,3,.58),
                       box(name+'_EntryMass',x+w/2-1,y,0,2,8,4.5,color=.51)]
            elif b['name']=='GlobalTerminal':
                parts=[loft(name+'_Lower',x,y,0,44,40,18,40,36,3,.56),
                       loft(name+'_TerraceA',x,y,18,42,38,2,cut=3,color=.64),
                       loft(name+'_Middle',x,y,20,36,32,16,32,28,2,.56),
                       loft(name+'_TerraceB',x,y,36,34,30,2,cut=2,color=.64),
                       loft(name+'_Upper',x,y,38,24,22,10,16,14,1.5,.56),
                       loft(name+'_Spire',x,y,48,7,7,10,5,5,.5,.50),
                       cylinder(name+'_Mast',x,y,58,.7,4,color=.48,segments=12)]
            elif b['name']=='ShoppingArcade':
                # Two coarse retail bars and a shallow canopy preserve an 8m covered passage.
                parts=[box(name+'_WestShops',x-8,y,0,8,d,8,color=.55),
                       box(name+'_EastShops',x+8,y,0,8,d,8,color=.55),
                       roof(name+'_Canopy',x,y,8,w,d,4)]
            else:
                parts=[loft(name+'_Shop',x,y,0,w,d,7.5,cut=1,color=.55),
                       loft(name+'_Roof',x,y,7.5,w,d,1.5,w-2,d-2,1,.61)]
        elif b.get('setback'):
            parts=[loft(name+'_Podium',x,y,0,w,d,8.1,cut=1,color=.53),
                   loft(name+'_Upper',x,y+1,8.1,w-2,d-2,h-8.1,cut=1,color=.56)]
        elif b.get('roof_family')=='hip':
            parts=[box(name+'_House',x,y,0,w,d,h-2.2,color=.55),
                   loft(name+'_HipRoof',x,y,h-2.2,w,d,2.2,w-5,d-5,.5,.62)]
        else: parts=[loft(name,x,y,0,w,d,h,cut=1,color=.46 if b.get('context') else .55)]
        for ob in parts:
            ob['landmark']=b['hero']; ob['district']=b['district']; ob['building_id']=b['name']
            ob['planned_floors']=b['floors']; ob['height_m']=h
            if b.get('context'):ob['context_only']=True
    # Trackbed mass only: no rails, sleepers, trains or detailed columns.
    a,b,c,d=p['rail']['rect']
    box('SM_GR_RailViaduct_Reserved',(a+c)/2,(b+d)/2,8,c-a,d-b,1.5,color=.43)
    for x in range(-20,321,40):
        # Clear cross streets; 8m underside for service vehicles and player cameras.
        if any(abs(x-roadx)<15 for roadx in (-160,0,160,305)):continue
        box(f'SM_GR_ViaductSupport_{x}',x,189,0,3,6,8,color=.48)
    # Opening silhouette only; no signs, decorative trim or material assignment.
    x,y=p['gateway']['at']
    for side in (-1,1):box(f'SM_GR_SouthGate_Pier_{side}',x+side*13,y,0,2,3,10.5,color=.62)
    bottom=[(-12+i*2,9+1.5*math.sin(i*math.pi/12)) for i in range(13)]
    cross=bottom+[(a,z+1.5) for a,z in reversed(bottom)]
    profile('SM_GR_SouthGate_Arch',x,y,0,cross,3,.64)
    def human(name,x,y,z):
        # Neutral proportion proxy: feet to head exactly 1.75m, no NPC implementation.
        pieces=[box(name+'_LegL',x-.13,y,z,.16,.20,.83,'00_REFERENCE',.24),
                box(name+'_LegR',x+.13,y,z,.16,.20,.83,'00_REFERENCE',.24),
                box(name+'_Torso',x,y,z+.83,.43,.24,.55,'00_REFERENCE',.24),
                box(name+'_Neck',x,y,z+1.38,.13,.13,.12,'00_REFERENCE',.24),
                cylinder(name+'_Head',x,y,z+1.50,.12,.25,.24,12,'00_REFERENCE'),
                box(name+'_ArmL',x-.29,y,z+.85,.14,.18,.52,'00_REFERENCE',.24),
                box(name+'_ArmR',x+.29,y,z+.85,.14,.18,.52,'00_REFERENCE',.24)]
        return pieces
    bpy.context.view_layer.update()
    def ground_height(x,y):
        hit,point,_,_,_,_=scene.ray_cast(bpy.context.evaluated_depsgraph_get(),
                                       Vector((x,y,3)),Vector((0,0,-1)),distance=5)
        if not hit:raise ValueError(f'No ground below review position {(x,y)}')
        return point.z
    for cam in p['cameras']:
        is_ground=int(cam['name'][:2])<=6 or cam.get('proxy')
        if is_ground:cam['pos'][2]=round(ground_height(*cam['pos'][:2])+1.65,4)
        data=bpy.data.cameras.new('CAM_GR_'+cam['name']); ob=bpy.data.objects.new(data.name,data)
        coll['00_REFERENCE'].objects.link(ob); ob.location=cam['pos']
        direction=Vector(cam['target'])-ob.location; ob.rotation_euler=direction.to_track_quat('-Z','Y').to_euler()
        data.lens=cam['lens']; data.sensor_width=36; data.clip_start=.1; data.clip_end=3000
        if cam.get('ortho'):data.type='ORTHO';data.ortho_scale=cam['ortho']
        ob['caption']=cam['caption']; ob['eye_above_ground_m']=1.65 if int(cam['name'][:2])<=6 or cam.get('proxy') else 0
        if is_ground:
            forward=Vector((direction.x,direction.y,0)).normalized()
            px=ob.location.x+forward.x*4.2; py=ob.location.y+forward.y*4.2
            proxy=human('REF_175cm_'+cam['name'],px,py,ground_height(px,py))
            heading=math.atan2(-forward.x,forward.y)
            for part in proxy:
                dx=part.location.x-px;dy=part.location.y-py
                part.location.x=px+dx*math.cos(heading)-dy*math.sin(heading)
                part.location.y=py+dx*math.sin(heading)+dy*math.cos(heading)
                part.rotation_euler.z=heading
                part['camera_proxy']=cam['name']
    scene.camera=bpy.data.objects['CAM_GR_07_Aerial']
    scene.render.engine='BLENDER_WORKBENCH'
    scene.render.resolution_x=1800;scene.render.resolution_y=1200;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG'
    scene.display.shading.light='STUDIO';scene.display.shading.studio_light='paint.sl'
    scene.display.shading.studiolight_rotate_z=math.radians(35)
    scene.display.shading.color_type='OBJECT'; scene.display.shading.show_shadows=True
    scene.display.shading.show_cavity=True;scene.display.shading.cavity_type='BOTH'
    scene.display.shading.curvature_ridge_factor=1.3;scene.display.shading.curvature_valley_factor=1.1
    scene.display.shading.show_object_outline=True;scene.display.shading.object_outline_color=(.14,.14,.14)
    scene.display.shading.background_type='WORLD';scene.world.color=(.55,.55,.55)
    scene.display.render_aa='16';scene.view_settings.view_transform='Standard'
    scene['stage']='STOP AFTER BLOCKOUT';scene['meters_per_blender_unit']=1.0
    scene['UE_note']='1m = 100uu; city core640m exceeds current server512m; not integrated.'
    scene['reference_status']=p['reference_status']
    for area in bpy.context.screen.areas if bpy.context.screen else []:
        if area.type=='VIEW_3D':
            area.spaces.active.region_3d.view_perspective='CAMERA'
            area.spaces.active.clip_end=4000
            area.spaces.active.shading.color_type='OBJECT'
    text=bpy.data.texts.new('README_GOLDENROD_BLOCKOUT')
    text.write('Goldenrod B02 — reference-informed graybox only.\nMetric: 1BU = 1m. North +Y.\n'
               'Scene geometry is intentionally untextured. No final hero assets, facades or NPCs.\n'
               'Five hero massing groups live in 01_BLOCKOUT. Production collections stay empty.\n'
               'Ten named cameras in 00_REFERENCE. Player proxies are exactly 1.75m tall.\n'
               'See Goldenrod_Design_Review.md, Masterplan.json and QA.json next to this file.\n'
               'Do not start detailed modeling until user approval.\n')
    (OUT/'Goldenrod_Masterplan.json').write_text(json.dumps(p,indent=2),encoding='utf-8')
    validate(p)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Goldenrod_Graybox.blend'))
    print('BUILD_COMPLETE',len(bpy.data.objects),'objects',len(bpy.data.meshes),'meshes')


def validate(p):
    import bpy
    import bmesh
    from mathutils import Vector
    bpy.context.view_layer.update()
    obs=[o for o in bpy.context.scene.objects if o.type=='MESH']
    nonmanifold=[];nonpositive=[]
    for mesh in bpy.data.meshes:
        bm=bmesh.new();bm.from_mesh(mesh)
        if any(not e.is_manifold for e in bm.edges):nonmanifold.append(mesh.name)
        if bm.calc_volume(signed=True)<=0:nonpositive.append(mesh.name)
        bm.free()
    bad_scale=[o.name for o in obs if any(abs(v-1)>1e-5 for v in o.scale)]
    intersections=[]
    bs=p['buildings']
    for i,a in enumerate(bs):
        for b in bs[i+1:]:
            if overlaps(a['rect'],b['rect']):intersections.append([a['name'],b['name']])
    road_conflicts=[]
    for b in bs:
        if b.get('context'):continue
        for r in p['roads']:
            if overlaps(b['rect'],r['rect']):road_conflicts.append([b['name'],r['name']])
    route_conflicts=[]
    for route in p['routes']:
        for a,b in zip(route['points'],route['points'][1:]):
            dist=math.dist(a,b)
            for i in range(max(2,math.ceil(dist*2))+1):
                f=i/max(2,math.ceil(dist*2)); x=a[0]+(b[0]-a[0])*f;y=a[1]+(b[1]-a[1])*f
                for building in bs:
                    if overlaps([x-.34,y-.34,x+.34,y+.34],building['rect']):
                        route_conflicts.append([route['name'],building['name']]);break
    proxies={}
    for cam in [c for c in p['cameras'] if int(c['name'][:2])<=6 or c.get('proxy')]:
        parts=[o for o in obs if o.get('camera_proxy')==cam['name']]
        points=[o.matrix_world @ Vector(v) for o in parts for v in o.bound_box]
        proxies[cam['name']]=round(max(v.z for v in points)-min(v.z for v in points),4)
    report={'mesh_objects':len(obs),'unique_meshes':len({o.data.name for o in obs}),
            'material_datablocks':len(bpy.data.materials),'material_slots':sum(len(o.material_slots) for o in obs),
            'nonmanifold_meshes':nonmanifold,'nonpositive_volume_meshes':nonpositive,
            'nonunit_scales':bad_scale,'building_overlaps':intersections,'road_conflicts':road_conflicts,
            'route_conflicts':list({tuple(x) for x in route_conflicts}),
            'proxy_heights_m':proxies,'core_buildings':sum(not b.get('context',False) for b in bs),
            'context_buildings':sum(b.get('context',False) for b in bs),
            'hero_dimensions':[{k:b[k] for k in ('name','w','d','h','floors')} for b in bs if b['hero']],
            'intentional_deferred':['UVs and PBR materials','final collision proxies','NPC behaviors',
                                    'Unreal import / server expansion','detailed facades and interiors'],
            'verification_scope':'Native Blender geometry and planned 2D route clearance only; no UE runtime test.'}
    (OUT/'Goldenrod_QA.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('QA',json.dumps(report))
    assert not nonmanifold and not nonpositive and not bad_scale
    assert not intersections and not road_conflicts and not route_conflicts
    assert all(abs(h-1.75)<.001 for h in proxies.values())
    assert report['material_datablocks']==0 and report['material_slots']==0


def render(p):
    import bpy
    scene=bpy.context.scene
    (OUT/'Previews').mkdir(exist_ok=True)
    for cam in p['cameras']:
        scene.camera=bpy.data.objects['CAM_GR_'+cam['name']]
        for ob in scene.objects:
            proxy=ob.get('camera_proxy')
            if proxy:ob.hide_render=proxy!=cam['name']
        scene.render.resolution_x=1800;scene.render.resolution_y=1200
        scene.render.filepath=str(OUT/'Previews'/('GR_'+cam['name']+'.png'))
        bpy.ops.render.render(write_still=True)
        print('RENDER_DONE',cam['name'],flush=True)


if __name__=='__main__':
    if '--plan-only' in sys.argv:make_plan()
    else:
        plan=json.loads((OUT/'Goldenrod_Masterplan.json').read_text(encoding='utf-8'))
        if '--render' in sys.argv:render(plan)
        else:build(plan)
