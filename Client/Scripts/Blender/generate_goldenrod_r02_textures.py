"""Goldenrod reference-led 2K raster PBR surfaces, authored with NumPy/Pillow.

Run with the workstation Python. No scene is opened or modified. Base colors
contain pigment and natural stone variation, never directional lighting or AO.
"""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

SIZE = 2048
OUT = Path(__file__).resolve().parents[2] / 'SourceArt/Environments/Goldenrod/Rebuild/Textures'
SURFACES = {
    'Promenade': dict(base='#805039', tile=4., rough=.79, kind='diamond', cell=1., joint=.018,
                      grout='#A1714D', relief=.0030,
                      usage='Warm red-brown diamond stone promenade; one diamond spans 1m along X/Y.'),
    'GoldenSidewalk': dict(base='#C69338', tile=4., rough=.74, kind='diamond', cell=.8, joint=.014,
                          grout='#997641', relief=.0023,
                          usage='Golden ochre square stone paving rotated 45 degrees; diamond width 0.8m.'),
    'PaleKerb': dict(base='#C7C0AA', tile=1., rough=.81, kind='kerb',
                     usage='Pale warm-gray cut limestone edging; 0.5m stone segments along U.'),
    'WallPlaster': dict(base='#F0EFEA', tile=2., rough=.77, kind='plaster',
                        usage='Neutral clean mineral render for reference-color tint multiplication.'),
    'PaintedMetal': dict(base='#F3F3F0', tile=2., rough=.34, kind='panel',
                         usage='Neutral coated metal cladding; restrained 2m panel joints.'),
    'RoofRed': dict(base='#CD422B', tile=2., rough=.34, kind='roof',
                    usage='Saturated red coated standing-seam roofing for Pokemon Center accents.'),
    'RoofCream': dict(base='#E8CC81', tile=2., rough=.38, kind='roof',
                      usage='Warm ivory-gold coated standing-seam roof for station vaults.'),
    'RoofTeal': dict(base='#216D59', tile=2., rough=.36, kind='roof',
                     usage='Deep green-teal coated roofing for Goldenrod roof accents.'),
}


def rgb(value):
    return np.array([int(value[i:i+2], 16)/255 for i in (1, 3, 5)], dtype=np.float32)


def smoothstep(a, b, x):
    t = np.clip((x-a)/(b-a), 0, 1)
    return t*t*(3-2*t)


def noise(rng, cutoff):
    frequency = np.fft.fftfreq(SIZE)*SIZE
    squared = frequency[:, None]**2 + frequency[None, :]**2
    values = np.fft.ifft2(np.fft.fft2(rng.normal(size=(SIZE, SIZE))) *
                         np.exp(-squared/(2*cutoff**2))).real
    return np.clip((values-values.mean())/(3*values.std()), -1, 1).astype(np.float32)


def normal_from_height(height, meters):
    spacing = meters/SIZE
    dx = (np.roll(height, -1, 1)-np.roll(height, 1, 1))/(2*spacing)
    dy = (np.roll(height, -1, 0)-np.roll(height, 1, 0))/(2*spacing)
    # PNG rows increase downward; OpenGL +V goes up, hence positive dy in green.
    normal = np.stack((-dx, dy, np.ones_like(dx)), -1)
    normal /= np.linalg.norm(normal, axis=-1, keepdims=True)
    return normal


def build(name, spec):
    seed = int.from_bytes(hashlib.sha256(('Goldenrod-R02-'+name).encode()).digest()[:8], 'little')
    rng = np.random.default_rng(seed)
    broad, grain, fine = (noise(rng, cutoff) for cutoff in (6, 95, 430))
    base = rgb(spec['base'])[None, None, :] * (1+.012*broad[..., None]+.012*fine[..., None])
    roughness = spec['rough'] + .020*grain + .010*fine
    height = .000025*grain + .000012*fine
    axis = (np.arange(SIZE, dtype=np.float32)+.5)*spec['tile']/SIZE
    x, y = np.meshgrid(axis, axis)
    kind = spec['kind']

    if kind == 'diamond':
        cell = spec['cell']
        # Both transformed grid axes advance an integer cell count at each edge.
        assert np.isclose(spec['tile']/cell, round(spec['tile']/cell))
        u, v = x+y, x-y
        if name == 'Promenade':
            # A subtle organic stone edge, without destroying the reference diamond motif.
            u += .008*broad
            v += .005*grain
        edge_u = np.abs((u+cell*.5) % cell-cell*.5)/np.sqrt(2)
        edge_v = np.abs((v+cell*.5) % cell-cell*.5)/np.sqrt(2)
        edge = np.minimum(edge_u, edge_v)
        grout = 1-smoothstep(spec['joint']*.35, spec['joint']*.70, edge)
        count = round(spec['tile']/cell)
        iu = np.floor(u/cell).astype(np.int32) % count
        iv = np.floor(v/cell).astype(np.int32) % count
        variation = rng.uniform(-1, 1, (count, count)).astype(np.float32)[iv, iu]
        tint = .085 if name == 'Promenade' else .065
        base *= (1+tint*variation+.018*grain)[..., None]
        base = base*(1-grout[..., None])+rgb(spec['grout'])*grout[..., None]
        height = .00020*grain+.00009*fine-spec['relief']*grout
        roughness += .023*variation
        roughness = roughness*(1-grout)+.87*grout
    elif kind == 'kerb':
        edge = np.abs((x+.25) % .5-.25)
        joint = 1-smoothstep(.0025, .0075, edge)
        pore = smoothstep(.40, .86, -fine)
        base *= (1-.035*pore+.02*grain)[..., None]
        base = base*(1-joint[..., None])+rgb('#8A867B')*joint[..., None]
        height = .00014*grain-.0002*pore-.002*joint
        roughness += .025*pore+.03*joint
    elif kind == 'plaster':
        pore = smoothstep(.42, .87, -fine)
        base *= (1-.025*pore)[..., None]
        height = .00007*grain+.000025*fine-.00018*pore
        roughness += .018*pore
    elif kind == 'panel':
        distance = np.minimum(np.minimum(x, spec['tile']-x), np.minimum(y, spec['tile']-y))
        seam = 1-smoothstep(.004, .012, distance)
        base *= (1-.065*seam)[..., None]
        height = -.0015*seam+.000007*fine
        roughness += .025*seam
    else:
        distance = np.abs((x+.25) % .5-.25)
        seam = 1-smoothstep(.008, .016, distance)
        height = .009*seam+.000013*grain+.000008*fine
        roughness += .018*seam

    roughness = np.clip(roughness, 0, 1)
    normals = normal_from_height(height, spec['tile'])
    channels = dict(BaseColor=np.clip(base, 0, 1), Roughness=roughness,
                    Normal=normals*.5+.5, Metallic=np.zeros_like(roughness))
    maps = {}
    for channel, values in channels.items():
        assert values.shape[:2] == (SIZE, SIZE) and np.isfinite(values).all()
        pixels = np.clip(np.rint(values*255), 0, 255).astype(np.uint8)
        filename = f'{name}_{channel}.png'
        Image.fromarray(pixels).save(OUT/filename)
        maps[channel] = dict(file=filename, color_space='sRGB' if channel == 'BaseColor' else 'Non-Color',
                             min_byte=int(pixels.min()), max_byte=int(pixels.max()))
    # Periodic edges are adjacent samples, not duplicated samples. Compare seam
    # deltas with all interior deltas to catch a discontinuity introduced by authoring.
    luminance = base.mean(-1)
    seam_delta = float(max(np.abs(luminance[:, 0]-luminance[:, -1]).mean(),
                           np.abs(luminance[0]-luminance[-1]).mean()))
    interior_max = float(max(np.abs(np.diff(luminance, axis=0)).mean(axis=1).max(),
                             np.abs(np.diff(luminance, axis=1)).mean(axis=0).max()))
    assert seam_delta <= interior_max+.002, (name, seam_delta, interior_max)
    result = dict(tile_meters=[spec['tile'], spec['tile']], base_color_reference_srgb=spec['base'],
                  usage=spec['usage'], metallic=0, maps=maps,
                  roughness_range=[round(float(roughness.min()), 4), round(float(roughness.max()), 4)],
                  normal_min_z=round(float(normals[:, :, 2].min()), 4),
                  seam_mean_absolute_delta=round(seam_delta, 6),
                  largest_interior_mean_delta=round(interior_max, 6))
    print(f'{name}: 4 x {SIZE}px maps ready', flush=True)
    return result


def sheets(names):
    font = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 21)
    small = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 16)
    sheet = Image.new('RGB', (1320, len(names)*250+65), '#f5f1e8')
    draw = ImageDraw.Draw(sheet)
    for i, text in enumerate(('BASE COLOR', 'ROUGHNESS', 'NORMAL +Y', 'METALLIC')):
        draw.text((300+i*250, 22), text, font=font, fill='#40372b')
    for row, name in enumerate(names):
        y = 65+row*250
        draw.text((20, y+80), name, font=font, fill='#40372b')
        draw.text((20, y+112), f'{SURFACES[name]["tile"]:g} m repeat / 2048 px', font=small, fill='#77684f')
        for col, channel in enumerate(('BaseColor', 'Roughness', 'Normal', 'Metallic')):
            tile = Image.open(OUT/f'{name}_{channel}.png').convert('RGB').resize((235, 235))
            sheet.paste(tile, (300+col*250, y))
    sheet.save(OUT/'_QA_R02_surface_contact_sheet.png')
    repeated = Image.new('RGB', (1536, 570), '#f5f1e8')
    draw = ImageDraw.Draw(repeated)
    for col, name in enumerate(('Promenade', 'GoldenSidewalk', 'PaleKerb')):
        if name not in names:
            continue
        draw.text((col*512+14, 17), f'{name} / 2 x 2 repeats', font=font, fill='#40372b')
        tile = Image.open(OUT/f'{name}_BaseColor.png').resize((256, 256))
        for x in range(2):
            for y in range(2):
                repeated.paste(tile, (col*512+x*256, 58+y*256))
    repeated.save(OUT/'_QA_R02_ground_repeats.png')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--families', default=','.join(SURFACES))
    args = parser.parse_args()
    names = args.families.split(',')
    assert names and set(names) <= SURFACES.keys()
    OUT.mkdir(parents=True, exist_ok=True)
    families = {name: build(name, SURFACES[name]) for name in names}
    for name in names:
        for channel in ('BaseColor', 'Roughness', 'Normal', 'Metallic'):
            with Image.open(OUT/f'{name}_{channel}.png') as img:
                assert img.size == (SIZE, SIZE)
                assert img.mode == ('RGB' if channel in ('BaseColor', 'Normal') else 'L')
                if channel == 'Metallic':
                    assert img.getextrema() == (0, 0)
    sheets(names)
    manifest = dict(version='Goldenrod-R02', resolution=[SIZE, SIZE], materials=families,
                    source='Original raster authoring with NumPy and Pillow, informed by the supplied Goldenrod screenshots. No copied image fragments, photos, scans or external generation service.',
                    normal_convention='OpenGL tangent +Y. Enable Flip Green Channel when importing to Unreal default DirectX tangent normal convention.',
                    metallic_rule='All eight surfaces are stone, plaster or opaque paint/coating and therefore use metallic 0.',
                    uv_rule='Use repeat/wrap; UV 0..1 equals each material tile_meters. Keep the same UV scale on all four channels.',
                    albedo_rule='BaseColor is pigment/stone only; no directional shadows, ambient occlusion or reflections are baked in.',
                    qa='Actual PNG dimensions/modes, finite values, metallic zero and periodic color seams are checked during generation. Inspect the contact and 2x2 repeat sheets for visual QA.')
    (OUT/'R02_Surface_Manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps(dict(materials=len(families), png_maps=len(families)*4, resolution=SIZE,
                          output=str(OUT)), indent=2), flush=True)


if __name__ == '__main__':
    main()
