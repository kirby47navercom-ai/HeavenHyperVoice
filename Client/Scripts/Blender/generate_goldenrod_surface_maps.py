"""Generate deterministic, tileable raster PBR surfaces for Goldenrod lookdev.

Run with Python, NumPy and Pillow. No Blender scene or source asset is modified.
Normals are OpenGL tangent-space (+Y); coated surfaces use metallic zero.
"""

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'SourceArt/Environments/Goldenrod/Lookdev'
TEXTURES = OUT / 'Textures'
SIZE = 512
FAMILIES = {
    'concrete': dict(base='#e4e5e5', tile=4.0, rough=(.76,.90), strength=1.0,
                     usage='Tintable light mineral walls, plinths, structural concrete.'),
    'painted_metal': dict(base='#ededed', tile=4.0, rough=(.30,.48), strength=.75,
                          usage='Tintable opaque coated panels, station trim, signs and tower cladding.'),
    'ceramic': dict(base='#eeeeec', tile=4.0, rough=(.36,.52), strength=1.0,
                    cell=(.5,.5), joint=.008,
                    usage='Tintable 0.5m square ceramic panels or exterior tiles with thin grout.'),
    'asphalt': dict(base='#34404c', tile=4.0, rough=(.84,.96), strength=.8,
                    usage='Blue-gray roads; fine aggregate albedo and shallow surface relief.'),
    'golden_pavers': dict(base='#c6a466', tile=4.0, rough=(.66,.86), strength=1.0,
                          cell=(.4,.2), joint=.009,
                          usage='Honey-colored pedestrian paving, 0.4 x 0.2m running-bond pavers.'),
    'roof_metal': dict(base='#e7e9ea', tile=4.0, rough=(.38,.58), strength=.7,
                       seam_spacing=.5,
                       usage='Tintable coated standing-seam roofing; seams spaced 0.5m along U.'),
    'brick': dict(base='#996047', tile=3.84, rough=(.74,.91), strength=1.0,
                  cell=(.24,.08), joint=.010,
                  usage='Terracotta brick walls; 0.24 x 0.08m running-bond courses including mortar.'),
}


def rgb(hex_color):
    return np.array([int(hex_color[i:i+2],16)/255 for i in (1,3,5)])


def periodic_noise(rng, cutoff):
    """Fourier-filtered periodic noise, without non-wrapping edge padding."""
    frequency = np.fft.fftfreq(SIZE) * SIZE
    radius_squared = frequency[:,None]**2 + frequency[None,:]**2
    spectrum = np.fft.fft2(rng.normal(size=(SIZE,SIZE)))
    values = np.fft.ifft2(spectrum*np.exp(-radius_squared/(2*cutoff**2))).real
    return np.clip((values-values.mean())/(3*values.std()),-1,1)


def smoothstep(low, high, value):
    t = np.clip((value-low)/(high-low),0,1)
    return t*t*(3-2*t)


def paving(spec, rng, running_bond):
    meters = spec['tile']
    w,h = spec['cell']
    x,y = np.meshgrid((np.arange(SIZE)+.5)*meters/SIZE,
                      (np.arange(SIZE)+.5)*meters/SIZE)
    rows,cols = round(meters/h),round(meters/w)
    assert np.isclose(rows*h,meters) and np.isclose(cols*w,meters)
    assert not running_bond or rows%2==0, 'Running bond must repeat after an even number of rows.'
    row = np.floor(y/h).astype(int)%rows
    shifted_x = x + (row%2)*w*.5 if running_bond else x
    col = np.floor(shifted_x/w).astype(int)%cols
    edge_x = np.abs((shifted_x+w/2)%w-w/2)
    edge_y = np.abs((y+h/2)%h-h/2)
    edge = np.minimum(edge_x,edge_y)
    half_joint = spec['joint']/2
    grout = 1-smoothstep(half_joint*.55,half_joint*1.45,edge)
    tile_variation = rng.uniform(-1,1,(rows,cols))[row,col]
    return grout,tile_variation


def normal_from_height(height, tile_meters, strength):
    """PNG row direction is down, so OpenGL +V uses the opposite derivative."""
    pixel_meters = tile_meters/SIZE
    dx = (np.roll(height,-1,axis=1)-np.roll(height,1,axis=1))/(2*pixel_meters)
    image_dy = (np.roll(height,-1,axis=0)-np.roll(height,1,axis=0))/(2*pixel_meters)
    normals = np.stack((-dx*strength,image_dy*strength,np.ones_like(dx)),axis=-1)
    normals /= np.linalg.norm(normals,axis=-1,keepdims=True)
    return normals


def build_family(name, spec):
    seed = int.from_bytes(hashlib.sha256(('goldenrod-b02-'+name).encode()).digest()[:8],'little')
    rng = np.random.default_rng(seed)
    broad,medium,fine = (periodic_noise(rng,c) for c in (4,32,150))
    base = rgb(spec['base'])[None,None,:] * (1+(.015*broad+.018*fine)[...,None])
    low,high = spec['rough']
    roughness = (low+high)/2 + (high-low)/2*(.4*medium+.6*fine)
    height = .00010*medium + .00007*fine

    if name=='concrete':
        pores = smoothstep(.44,.82,-fine)
        base *= (1-.045*pores)[...,None]
        height -= .00045*pores
        roughness += .018*pores
    elif name=='painted_metal':
        height = .000025*medium + .000018*fine
    elif name=='asphalt':
        aggregate = smoothstep(.18,.62,fine)
        base *= (1+.12*aggregate+.065*medium)[...,None]
        height = .0008*fine + .0002*medium
        roughness -= .02*aggregate
    elif name in ('ceramic','golden_pavers','brick'):
        grout,variation = paving(spec,rng,name!='ceramic')
        if name=='ceramic':
            mortar,depth,tint = '#b8b9b6',.0018,.020
            height = .000035*fine
        elif name=='golden_pavers':
            mortar,depth,tint = '#958770',.0030,.055
            height = .00022*fine+.00010*medium
        else:
            mortar,depth,tint = '#b9afa0',.0035,.10
            height = .00028*fine+.00012*medium
        base *= (1+tint*variation)[...,None]
        base = base*(1-grout[...,None])+rgb(mortar)[None,None,:]*grout[...,None]
        height = height*(1-grout)-depth*grout
        roughness = (roughness+.025*variation)*(1-grout)+.89*grout
    elif name=='roof_metal':
        x = (np.arange(SIZE)+.5)*spec['tile']/SIZE
        spacing = spec['seam_spacing']
        distance = np.abs((x+spacing/2)%spacing-spacing/2)
        seam = np.exp(-.5*(distance/.008)**2)[None,:]
        height = .012*seam+.000035*medium+.000015*fine
        # Pigment/albedo does not include baked directional shadows.
        roughness += .018*seam

    roughness = np.clip(roughness,0,1)
    normals = normal_from_height(height,spec['tile'],spec['strength'])
    maps = {'BaseColor':np.clip(base,0,1),'Roughness':roughness,
            'Normal':normals*.5+.5,'Metallic':np.zeros((SIZE,SIZE))}
    details = {}
    for map_name,values in maps.items():
        assert np.isfinite(values).all()
        pixels = np.clip(np.rint(values*255),0,255).astype(np.uint8)
        filename = f'{name}_{map_name}.png'
        Image.fromarray(pixels).save(TEXTURES/filename)
        details[map_name] = {'path':f'Textures/{filename}',
                             'color_space':'sRGB' if map_name=='BaseColor' else 'Non-Color / linear',
                             'mode':'RGB' if pixels.ndim==3 else 'L',
                             'min_byte':int(pixels.min()),'max_byte':int(pixels.max())}
    return dict(tile_meters=[spec['tile'],spec['tile']],usage=spec['usage'],
                base_color_reference_srgb=spec['base'],dielectric=True,
                coated_surface=name in ('painted_metal','roof_metal'),metallic=0,
                cell_meters=list(spec['cell']) if 'cell' in spec else None,
                grout_width_meters=spec.get('joint'),seam_spacing_meters=spec.get('seam_spacing'),
                roughness_actual_range=[round(float(roughness.min()),4),round(float(roughness.max()),4)],
                normal_min_z=round(float(normals[:,:,2].min()),4),maps=details)


def preview_sheets():
    font = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf',19)
    small = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',16)
    sheet = Image.new('RGB',(1120,7*215+60),'#f0f1ee')
    draw = ImageDraw.Draw(sheet)
    for index,title in enumerate(('BASE COLOR','ROUGHNESS','NORMAL +Y','METALLIC')):
        draw.text((240+index*215,20),title,fill='#263438',font=font)
    for row,(name,spec) in enumerate(FAMILIES.items()):
        y=60+row*215
        draw.text((16,y+45),name,fill='#263438',font=font)
        draw.text((16,y+75),f'{spec["tile"]:g} x {spec["tile"]:g} m',fill='#607070',font=small)
        for column,map_name in enumerate(('BaseColor','Roughness','Normal','Metallic')):
            tile = Image.open(TEXTURES/f'{name}_{map_name}.png').convert('RGB').resize((200,200))
            sheet.paste(tile,(240+column*215,y))
    sheet.save(TEXTURES/'_QA_surface_contact_sheet.png')
    # A 2x2 repeat makes any seam or alternating-course mismatch directly visible.
    repeated = Image.new('RGB',(1536,560),'#f0f1ee')
    draw = ImageDraw.Draw(repeated)
    for column,name in enumerate(('golden_pavers','brick','roof_metal')):
        draw.text((column*512+12,14),f'{name} / 2 x 2 repeats',fill='#263438',font=font)
        tile = Image.open(TEXTURES/f'{name}_BaseColor.png').resize((256,256))
        for x in range(2):
            for y in range(2):
                repeated.paste(tile,(column*512+x*256,48+y*256))
    repeated.save(TEXTURES/'_QA_tiled_base_colors.png')


def main():
    TEXTURES.mkdir(parents=True,exist_ok=True)
    slope = np.broadcast_to(np.sin(2*np.pi*(np.arange(SIZE)+.5)/SIZE)[:,None],(SIZE,SIZE))*.001
    assert normal_from_height(slope,4,1)[0,0,1]>0, 'OpenGL green is positive when height increases down PNG rows.'
    surfaces = {name:build_family(name,spec) for name,spec in FAMILIES.items()}
    # Validate the real files, including the all-zero coated-surface metallic maps.
    for family in surfaces.values():
        for key,info in family['maps'].items():
            image = Image.open(OUT/info['path'])
            assert image.size==(SIZE,SIZE) and image.mode==info['mode']
            if key=='Metallic':assert np.asarray(image).max()==0
    preview_sheets()
    manifest = dict(version=1,resolution=[SIZE,SIZE],generator=Path(__file__).name,
                    source='Deterministic procedural raster generation with NumPy and Pillow; no photos, AI service, or external texture sources.',
                    normal_convention='Tangent-space OpenGL +Y. PNG rows run downward; green encodes the +V tangent axis. Blender Non-Color normal texture -> Normal Map node.',
                    unreal_import='Use these image files as regular textures. For the default Unreal DirectX tangent convention, enable Flip Green Channel on the normal texture. No automatic Blender node translation is assumed.',
                    sampling='Repeat/wrap on both U and V. Physical repeat size is per-family tile_meters. For 4m surfaces: a 4m UV span repeats once.',
                    continuity='Fourier noise is periodic. Joint grids use integer tile counts and even running-bond rows; normals use np.roll central differences across both boundaries.',
                    tinting='Multiply BaseColor with the chosen material tint in linear shader space. Concrete, painted_metal, ceramic and roof_metal deliberately use light neutral albedo.',
                    limitations='512px shared lookdev maps, authored procedurally rather than measured scans. Normal maps do not create real silhouette displacement. Solid pigment grout differences are included; no directional lighting or AO is baked into BaseColor.',
                    qa_images=['Textures/_QA_surface_contact_sheet.png','Textures/_QA_tiled_base_colors.png'],
                    families=surfaces)
    (OUT/'surface_manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps({'families':len(surfaces),'map_count':len(surfaces)*4,'resolution':SIZE,
                      'output':str(TEXTURES),'manifest':str(OUT/'surface_manifest.json'),
                      'roughness_ranges':{name:spec['roughness_actual_range'] for name,spec in surfaces.items()},
                      'normal_min_z':{name:spec['normal_min_z'] for name,spec in surfaces.items()}},indent=2))


if __name__=='__main__':
    main()
