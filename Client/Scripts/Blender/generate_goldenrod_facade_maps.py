"""Make window overlays for existing Goldenrod walls; no Blender geometry is added.

Run with the existing Python, NumPy and Pillow environment. BaseColor alpha is
window-plus-frame coverage; GlassMask isolates the glass inside that coverage.
"""

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont


OUT = Path(__file__).resolve().parents[2] / 'SourceArt/Environments/Goldenrod/Lookdev/Textures/Facades'
SPECS = {
    'window_regular': {'size': (512, 512), 'tile': (4.0, 3.6), 'opening': (2.6, 2.35)},
    'window_hero': {'size': (512, 512), 'tile': (4.0, 3.6), 'opening': (2.6, 2.35), 'mullion': .045},
    'storefront': {'size': (512, 512), 'tile': (5.0, 4.0), 'opening': (4.0, 3.2)},
    'station_round': {'size': (2048, 512), 'tile': (34.0, 6.0), 'opening': (3.1, 3.1), 'round': True},
}
FRAME_METERS = .07
GLASS_HEX = '#294B5A'
FRAME_HEX = '#BCCACD'


def rgb(value):
    return np.array([int(value[i:i+2], 16) / 255 for i in (1, 3, 5)])


def smoothstep(low, high, value):
    t = np.clip((value - low) / (high - low), 0, 1)
    return t * t * (3 - 2 * t)


def normal_from_height(height, tile):
    """PNG rows point down, while the OpenGL +V tangent points up."""
    h, w = height.shape
    dx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) / (2 * tile[0] / w)
    row_dy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) / (2 * tile[1] / h)
    normal = np.stack((-dx, row_dy, np.ones_like(height)), axis=-1)
    return normal / np.linalg.norm(normal, axis=-1, keepdims=True)


def create_maps(name, spec):
    w, h = spec['size']
    tw, th = spec['tile']
    ow, oh = spec['opening']
    x, y = np.meshgrid((np.arange(w) + .5) * tw / w - tw / 2,
                       (np.arange(h) + .5) * th / h - th / 2)
    # Analytic coverage antialiasing preserves the physical dimensions.
    aa = max(tw / w, th / h) * .65
    if spec.get('round'):
        edge = ow / 2 - np.hypot(x, y)
    else:
        edge = np.minimum(ow / 2 - np.abs(x), oh / 2 - np.abs(y))
    alpha = smoothstep(-aa, aa, edge)
    glass = smoothstep(FRAME_METERS - aa, FRAME_METERS + aa, edge)
    if spec.get('mullion'):
        glass *= smoothstep(spec['mullion'] / 2 - aa, spec['mullion'] / 2 + aa, np.abs(x))

    # Low contrast interior color variation only; reflected light belongs to the shader.
    local_x, local_y = x / ow, y / oh
    variation = (.045 * np.cos(local_x * 3.1 + .4)
                 + .027 * np.sin(local_x * 7.0 + local_y * 2.0)
                 - .055 * np.clip(local_y, -.5, .5))
    inner_dark = .08 * np.exp(-((local_x + .28) / .22) ** 2 - ((local_y - .15) / .35) ** 2)
    if spec.get('mullion'):
        inner_dark += .065 * smoothstep(-.02, .02, -x)
    glass_rgb = rgb(GLASS_HEX) * (1 + variation - inner_dark)[..., None]
    # Keep frame color in transparent texels to avoid black edge filtering halos.
    glass_fraction = np.divide(glass, alpha, out=np.zeros_like(glass), where=alpha > 1e-7)
    color = rgb(FRAME_HEX) * (1 - glass_fraction[..., None]) + glass_rgb * glass_fraction[..., None]
    rgba = np.dstack((color, alpha))
    glass_rough = .19 + .018 * np.cos(local_x * 4.1 + local_y * 2.4)
    rough = .80 * (1 - alpha) + .32 * (alpha - glass) + glass_rough * glass
    # Shallow normal-only relief: frame +8mm, glass -6mm. No displacement or mesh.
    height = .008 * alpha - .014 * glass
    normal = normal_from_height(height, spec['tile'])
    arrays = {
        'BaseColor': np.rint(np.clip(rgba, 0, 1) * 255).astype('uint8'),
        'Roughness': np.rint(np.clip(rough, 0, 1) * 255).astype('uint8'),
        'Normal': np.rint((normal * .5 + .5) * 255).astype('uint8'),
        'GlassMask': np.rint(glass * 255).astype('uint8'),
    }
    # Check useful invariants, including physical centering and clean repeat boundaries.
    assert np.all(glass <= alpha + 1e-7)
    assert np.all(alpha[[0, -1], :] == 0) and np.all(alpha[:, [0, -1]] == 0)
    assert np.allclose(alpha, alpha[::-1, :]) and np.allclose(alpha, alpha[:, ::-1])
    assert np.all(normal[..., 2] > 0)
    inside = np.argwhere(alpha >= .5)
    actual = [(inside[:, 1].max() - inside[:, 1].min() + 1) * tw / w,
              (inside[:, 0].max() - inside[:, 0].min() + 1) * th / h]
    assert all(abs(a - b) <= 2 * p for a, b, p in zip(actual, spec['opening'], (tw / w, th / h)))
    if spec.get('mullion'):
        assert glass[h // 2, w // 2] == 0
    else:
        assert glass[h // 2, w // 2] == 1
    maps = {}
    for channel, array in arrays.items():
        path = OUT / f'{name}_{channel}.png'
        Image.fromarray(array).save(path, optimize=True)
        with Image.open(path) as check:
            assert check.size == (w, h)
            assert check.mode == ('RGBA' if channel == 'BaseColor' else 'RGB' if channel == 'Normal' else 'L')
        maps[channel] = {
            'path': path.name, 'color_space': 'sRGB' if channel == 'BaseColor' else 'Non-Color / linear',
            'mode': 'RGBA' if channel == 'BaseColor' else 'RGB' if channel == 'Normal' else 'L',
            'bytes': path.stat().st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
        }
    return {
        'resolution': spec['size'], 'tile_meters': spec['tile'],
        'window_outer_meters_including_frame': spec['opening'],
        'window_center_meters_from_tile_bottom_left': [tw / 2, th / 2],
        'shape': 'circle' if spec.get('round') else 'rectangle',
        'frame_width_meters': FRAME_METERS, 'vertical_mullion_width_meters': spec.get('mullion', 0),
        'coverage_threshold_0_5_bounds_meters': actual,
        'coverage_fraction': float(alpha.mean()), 'glass_fraction': float(glass.mean()),
        'glass_roughness_range': [float(glass_rough[glass > .999].min()), float(glass_rough[glass > .999].max())],
        'frame_roughness': .32, 'normal_min_z': float(normal[..., 2].min()), 'maps': maps,
    }


def contact_sheet():
    font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 19)
    small = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 15)
    sheet = Image.new('RGB', (1490, 1390), '#F0F1ED')
    draw = ImageDraw.Draw(sheet)
    titles = ['BaseColor / wall', 'Alpha coverage', 'Roughness', 'Normal +Y', 'GlassMask']
    draw.text((24, 17), 'GOLDENROD / WALL TEXTURES — NO WINDOW GEOMETRY', fill='#26363C', font=font)
    for i, title in enumerate(titles):
        draw.text((235 + i * 247, 61), title, fill='#26363C', font=small)
    for row, (name, spec) in enumerate(SPECS.items()):
        y = 100 + row * 320
        draw.text((24, y + 12), name, fill='#26363C', font=font)
        tw, th = spec['tile']
        draw.text((24, y + 44), f'Tile {tw:g} x {th:g} m', fill='#526268', font=small)
        draw.text((24, y + 67), f"{spec['size'][0]} x {spec['size'][1]} px", fill='#526268', font=small)
        draw.text((24, y + 98), 'Frame 0.07 m', fill='#526268', font=small)
        if spec.get('mullion'):
            draw.text((24, y + 121), '1 mullion / 0.045 m', fill='#526268', font=small)
        if spec.get('round'):
            draw.text((24, y + 121), 'Circle diameter 3.1 m', fill='#526268', font=small)
            draw.text((24, y + 154), 'Full tile + 4m crop', fill='#526268', font=small)
        images = [Image.open(OUT / f'{name}_{channel}.png') for channel in ('BaseColor', 'Roughness', 'Normal', 'GlassMask')]
        base = Image.new('RGBA', images[0].size, '#CFC6B6')
        base.alpha_composite(images[0])
        channels = [base.convert('RGB'), images[0].getchannel('A'), *images[1:]]
        for col, im in enumerate(channels):
            x = 230 + col * 247
            height = round(235 * th / tw)
            sheet.paste(im.resize((235, height), Image.Resampling.LANCZOS).convert('RGB'), (x, y))
            if spec.get('round'):
                w, h = im.size
                crop = im.crop((round((tw/2-2)/tw*w), round((th/2-2)/th*h),
                                round((tw/2+2)/tw*w), round((th/2+2)/th*h)))
                sheet.paste(crop.resize((220, 220), Image.Resampling.LANCZOS).convert('RGB'), (x + 7, y + 65))
        for im in images:
            im.close()
    draw.text((24, 1360), 'Display aspect follows physical meters. Glass color is restrained; reflections remain shader-driven.', fill='#526268', font=small)
    sheet.save(OUT / '_QA_facade_contact_sheet.png', optimize=True)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sets = {name: create_maps(name, spec) for name, spec in SPECS.items()}
    contact_sheet()
    manifest = {
        'version': 1, 'generator': Path(__file__).name,
        'source': 'Deterministic NumPy and Pillow raster authoring; no photos, AI image service or external assets.',
        'usage': 'Sample on existing exterior wall UVs. Alpha mixes wall and window material; do not connect it to overall wall opacity.',
        'color': {'glass_srgb': GLASS_HEX, 'frame_srgb': FRAME_HEX,
                  'note': 'Subtle glass value variation and weak vertical color gradient only; no strong baked reflection, lighting stripe or AO.'},
        'alpha': 'Straight/unassociated BaseColor RGBA alpha covers glass plus frame. Outside the opening alpha is zero; hidden RGB extends frame color for filtering.',
        'glass_mask': 'One on glass, zero on frame and wall, with antialiased boundaries. Frame coverage = max(BaseColor.alpha - GlassMask, 0).',
        'normal': 'Tangent-space OpenGL +Y, PNG row-down derivative accounted for; flat is approximately (128,128,255). Normal-only shallow relief, no displacement.',
        'unreal': 'Use Non-Color masks/roughness/normal. Flip normal green for the default DirectX tangent convention. No automatic shader conversion.',
        'repeat': 'Repeat U and V; all window coverage stays inside tile boundaries. Set UV spans from tile_meters, not from image pixel aspect.',
        'limitations': 'One deterministic window per repeat cell; these four maps cannot create independent random variation for every repeated window. No interiors or geometry.',
        'maps_count': 16,
        'total_png_bytes_excluding_contact_sheet': sum(m['bytes'] for s in sets.values() for m in s['maps'].values()),
        'uncompressed_channel_bytes_without_mips': sum(s['size'][0] * s['size'][1] * 9 for s in SPECS.values()),
        'qa_contact_sheet': '_QA_facade_contact_sheet.png', 'sets': sets,
    }
    (OUT / 'facade_manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps({'output': str(OUT), 'maps': 16,
                      'png_bytes': manifest['total_png_bytes_excluding_contact_sheet'],
                      'uncompressed_channel_bytes': manifest['uncompressed_channel_bytes_without_mips'],
                      'qa': 'Dimensions, centering, tile borders, glass coverage, modes and +Z normals verified.'}))


if __name__ == '__main__':
    main()
