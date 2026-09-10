"""Paint complete, opaque Goldenrod facade PBR atlases; creates no geometry.

UV (0,0) is the wall's bottom left and (1,1) its top right. Pixel aspect is
deliberately square; the manifest supplies each wall's physical aspect ratio.
"""

import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parents[2] / 'SourceArt/Environments/Goldenrod/Rebuild/Textures'
SIZE, WORK = 1024, 2048
SPECS = {
    'HouseOchre': dict(color='#D9903D', width=14., height=9., kind='house'),
    'HouseRed': dict(color='#BC542D', width=12., height=9., kind='house'),
    'ShopCream': dict(color='#E8D1A3', width=20., height=16., kind='shop'),
    'ShopGold': dict(color='#D6A548', width=18., height=16., kind='shop'),
    'DeptBlue': dict(color='#204F7F', width=30., height=28., kind='department'),
    'CenterWhite': dict(color='#EEE7C9', width=24., height=8., kind='center'),
    'StationOchre': dict(color='#D9A557', width=32., height=14., kind='station'),
    'RadioDark': dict(color='#303C4A', width=32., height=12., kind='radio'),
    'TerminalGlass': dict(color='#204E51', width=34., height=18., kind='terminal'),
    'GymFront': dict(color='#684931', width=26., height=10., kind='gym'),
    'StationArch': dict(color='#E8CC81', width=32., height=8., kind='arch'),
    'DoorBlue': dict(color='#257EA0', width=4., height=4., kind='door', resolution=512, prefix='DoorBlue'),
}
DARK, CREAM, GLASS, BLUE = '#29363E', '#EDDDB8', '#659CAD', '#2E8BCA'


class Facade:
    def __init__(self, name, spec):
        self.name, self.spec = name, spec
        self.w, self.h = spec['width'], spec['height']
        self.base = Image.new('RGB', (WORK, WORK), spec['color'])
        self.rough = Image.new('L', (WORK, WORK), 193)
        self.metal = Image.new('L', (WORK, WORK), 0)
        self.relief = Image.new('F', (WORK, WORK), 0.)
        self.draw = [ImageDraw.Draw(im) for im in (self.base, self.rough, self.metal, self.relief)]
        self.windows, self.doors = [], []

    def pixel(self, x, z):
        return (round(x/self.w*WORK), round((1-z/self.h)*WORK))

    def rect(self, x, z, w, h, color, rough=.6, height=0., metallic=0):
        assert w > 0 and h > 0
        bounds = (*self.pixel(x, z+h), *self.pixel(x+w, z))
        for draw, value in zip(self.draw, (color, round(rough*255), round(metallic*255), height)):
            draw.rectangle(bounds, fill=value)

    def line(self, points, color, width=.06, rough=.4, height=.014, metallic=0):
        pixels = [self.pixel(*p) for p in points]
        weight = max(1, round(width*WORK/self.w))
        for draw, value in zip(self.draw, (color, round(rough*255), round(metallic*255), height)):
            draw.line(pixels, fill=value, width=weight)

    def ellipse(self, cx, cz, rx, rz, color, rough=.4, height=0.):
        bounds = (*self.pixel(cx-rx, cz+rz), *self.pixel(cx+rx, cz-rz))
        for draw, value in zip(self.draw, (color, round(rough*255), 0, height)):
            draw.ellipse(bounds, fill=value)

    def window(self, cx, z, w, h, diagonal=False, glass=GLASS, thin=False, record=True):
        x = cx-w/2
        outer, inner = (.09, .05) if thin else (.16, .10)
        inset = outer+inner
        if record:
            assert w-2*inset >= 2 and h-2*inset >= 2, (self.name, w, h)
            self.windows.append(dict(bounds_meters=[x, z, w, h], glass_meters=[w-2*inset, h-2*inset],
                                     diagonal_mullion=diagonal))
        self.rect(x, z, w, h, DARK, .41, .018)
        self.rect(x+outer, z+outer, w-2*outer, h-2*outer, CREAM if not thin else '#46777B', .35, .020)
        self.rect(x+inset, z+inset, w-2*inset, h-2*inset, glass, .24 if glass == '#B9A66E' else .22, -.020)
        # Deliberately quiet glass tint; light/reflection is calculated by PBR.
        if diagonal:
            self.line([(x+inset, z+inset), (x+w-inset, z+h-inset)], CREAM, .085, .36, .014)

    def door(self, cx, width=2.8, height=3.1):
        x, z = cx-width/2, .12
        self.rect(x-.22, z, width+.44, height+.23, DARK, .42, .018)
        self.rect(x-.10, z, width+.20, height+.12, CREAM, .42, .020)
        self.rect(x, z, width, height, '#4C9AB5', .21, -.020)
        self.rect(cx-.045, z, .09, height, DARK, .38, .014)
        self.rect(x, z, width, .20, '#2B677F', .32, .014)
        for offset in (-.23, .18):
            self.rect(cx+offset, 1.05, .055, .42, '#CDD6D3', .25, .018, 1)
        self.doors.append(dict(bounds_meters=[x, z, width, height], clear_width_meters=width,
                               clear_height_meters=height))

    def store(self, margin=1.4):
        # A continuous large glazing band, with structural mullions painted into it.
        left, width = margin, self.w-2*margin
        self.window(self.w/2, .22, width, 3.25, thin=True, record=False)
        count = max(2, round(width/4.5))
        for col in range(1, count):
            self.rect(left+col*width/count-.055, .30, .11, 3.04, DARK, .39, .014)
        self.door(self.w/2)

    def finish(self):
        channels = {}
        size = self.spec.get('resolution', SIZE)
        prefix = self.spec.get('prefix', f'Facade_{self.name}')
        base = np.asarray(self.base.resize((size, size), Image.Resampling.LANCZOS)).astype(np.float32)/255
        seed = int.from_bytes(hashlib.sha256(self.name.encode()).digest()[:8], 'little')
        rng = np.random.default_rng(seed)
        # Pigment grain only. No directional shadow, view-dependent highlight or AO.
        grain = rng.normal(0, .0016, (size, size)).astype(np.float32)
        base = np.clip(base+grain[..., None], 0, 1)
        rough = np.asarray(self.rough.resize((size, size), Image.Resampling.LANCZOS)).astype(np.float32)/255
        rough = np.clip(rough+grain*1.5, 0, 1)
        relief = np.asarray(self.relief.resize((size, size), Image.Resampling.LANCZOS)).copy()
        # Normal-only relief totals 4cm. Clamp resampling overshoot at hard edges.
        relief = np.clip(relief, -.02, .02)
        for _ in range(8):
            padded = np.pad(relief, 1, mode='edge')
            relief = (padded[1:-1, 1:-1]*4+padded[:-2, 1:-1]+padded[2:, 1:-1]+
                      padded[1:-1, :-2]+padded[1:-1, 2:])/8
        row_dy, dx = np.gradient(relief, self.h/size, self.w/size)
        normal = np.stack((-dx, row_dy, np.ones_like(dx)), -1)
        normal /= np.linalg.norm(normal, axis=-1, keepdims=True)
        metal = np.asarray(self.metal.resize((size, size), Image.Resampling.LANCZOS)).astype(np.float32)/255
        arrays = dict(BaseColor=base, Roughness=rough, Normal=normal*.5+.5, Metallic=metal)
        for name, data in arrays.items():
            assert np.isfinite(data).all() and data.shape[:2] == (size, size)
            filename = f'{prefix}_{name}.png'
            pixels = np.clip(np.rint(data*255), 0, 255).astype(np.uint8)
            Image.fromarray(pixels).save(OUT/filename)
            with Image.open(OUT/filename) as image:
                assert image.size == (size, size)
                assert image.mode == ('RGB' if name in ('BaseColor', 'Normal') else 'L')
            channels[name] = dict(file=filename, mode='RGB' if pixels.ndim == 3 else 'L',
                                  color_space='sRGB' if name == 'BaseColor' else 'Non-Color')
        assert float(normal[..., 2].min()) > .5
        assert all(.0 <= d['bounds_meters'][1] <= .15 for d in self.doors)
        print(f'{prefix}: 4 opaque {size}px atlas maps ready', flush=True)
        return dict(wall_meters=[self.w, self.h], resolution=[size, size], wall_color_srgb=self.spec['color'],
                    windows=self.windows, doors=self.doors, maps=channels,
                    height_range_meters=[float(relief.min()), float(relief.max())],
                    normal_min_z=float(normal[..., 2].min()))


def make(name, spec):
    f = Facade(name, spec)
    w, h, kind = f.w, f.h, spec['kind']
    if kind == 'door':
        f.rect(.40, .12, 3.2, 3.7, '#19566C', .40, .020)
        f.rect(.60, .12, 2.8, 3.5, '#63A4B7', .22, -.020)
        f.rect(1.975, .12, .05, 3.5, DARK, .39, .016)
        f.rect(.40, .12, 3.2, .08, '#315C69', .50, .018)
        f.rect(.20, 3.70, 3.6, .16, '#174B65', .41, .012)
        for x in (1.77, 2.18):
            f.rect(x, 1.08, .05, .38, '#CDD6D3', .25, .018, 1)
        f.doors.append(dict(bounds_meters=[.60, .12, 2.8, 3.5], clear_width_meters=2.8,
                            clear_height_meters=3.5))
        return f.finish()
    if kind == 'arch':
        f.ellipse(16., 0., 14.5, 7.0, '#216D59', .39, 0.)
        for cx in (9., 16., 23.):
            f.ellipse(cx, 3.4, 1.25, 1.25, '#DCA74E', .35, .020)
            f.ellipse(cx, 3.4, 1.07, 1.07, '#70B7CA', .22, -.020)
            f.windows.append(dict(shape='circle', center_meters=[cx, 3.4],
                                  outer_diameter_meters=2.5, glass_diameter_meters=2.14))
        return f.finish()
    f.rect(0, 0, w, .35, '#766B5A' if kind != 'terminal' else '#183D42', .78)
    f.rect(0, h-.22, w, .22, CREAM if kind not in ('radio', 'terminal', 'gym') else DARK, .58, .012)
    if kind == 'house':
        for z in np.arange(.40, h-.4, .52):
            for x in (.12, w-.52):
                f.rect(x, float(z), .40, .40, '#E8BA6E', .72, .006)
        for row, z in enumerate((3.15, 6.08)):
            for col, cx in enumerate((w*.26, w*.74)):
                f.window(cx, z, 3.0 if w < 13 else 3.35, 2.62, diagonal=(row+col)%2 == 0,
                         glass='#B9A66E' if col == row else GLASS)
        f.rect(.6, 2.96, w-1.2, .15, CREAM, .61, .013)
        f.door(w/2, 2.1, 2.65)
    elif kind == 'shop':
        for z in (4.50, 8.10, 11.70):
            f.rect(.65, z-.30, w-1.3, .18, '#A5855B', .67, .010)
            for col in range(3):
                f.window(w*(col+.5)/3, z, w/3-1.4, 2.75, diagonal=col == 1)
        for x in (.12, w-.59):
            for z in np.arange(.5, h-.5, .7):
                f.rect(x, float(z), .47, .46, '#B49F7E', .69, .008)
        f.store()
    elif kind == 'department':
        f.rect(.25, .4, .32, h-.9, '#2C7DC0', .45, .006)
        f.rect(w-.57, .4, .32, h-.9, '#2C7DC0', .45, .006)
        for row in range(6):
            for col in range(5):
                f.window(w*(col+.5)/5, 4.40+3.7*row, 4.2, 2.9,
                         glass=GLASS if (row+col)%3 else '#B9A66E')
        f.store(2.)
        f.rect(0, h-.75, w, .52, '#C14136', .42, .010)
    elif kind == 'center':
        f.rect(0, 3.90, w, .48, BLUE, .43, .012)
        f.rect(0, h-.4, w, .35, BLUE, .43, .012)
        f.window(w/2, 4.75, w-2.5, 2.6, glass='#689FB4')
        for col in range(1, 6):
            f.rect(1.25+col*(w-2.5)/6-.05, 5.01, .10, 2.08, BLUE, .38, .014)
        f.door(w/2, 2.8, 3.2)
        for x in (1., w-3.3):
            f.rect(x, .75, 2.3, 2.4, '#6CA9B9', .24, -.02)
    elif kind == 'station':
        for col in range(5):
            f.window(w*(col+.5)/5, 8.65, 5.0, 2.85, glass=GLASS)
        f.rect(1., 8.19, w-2., .20, CREAM, .58, .012)
        f.door(w/2, 3.2, 3.4)
        for cx in (w*.20, w*.80):
            f.window(cx, .95, 5.1, 3.4, glass=GLASS)
    elif kind == 'radio':
        for z in np.arange(.55, h-.3, .43):
            f.rect(0, float(z), w, .028, '#1F2934', .52, -.007)
        f.window(w/2, 7.75, w-3., 2.95, thin=True, glass=GLASS)
        for col in range(1, 8):
            f.rect(1.5+col*(w-3)/8-.05, 7.89, .1, 2.67, DARK, .40, .018)
        f.door(w/2, 2.8, 3.25)
    elif kind == 'terminal':
        for row in range(4):
            for col in range(6):
                f.window(.30+(w-.6)*(col+.5)/6, .40+row*4.35,
                         (w-.6)/6-.09, 4.15, thin=True, glass='#4C9BAC')
        # The lower tier's centered entry is part of the atlas, with no door mesh.
        f.door(w/2, 3.2, 3.4)
    else:
        for z in (1.6, 3.65):
            f.rect(0, z, w, .5, '#C3435F', .43, .012)
        for cx in (w*.18, w*.82):
            f.window(cx, .75, 3.8, 6.6, glass='#4D9EBB')
            f.rect(cx-.05, 1.01, .1, 6.08, '#2E8BCA', .40, .016)
        f.door(w/2, 3.2, 3.5)
        f.rect(w/2-2.6, 4.2, 5.2, 2.2, '#2A649C', .45, .014)
    return f.finish()


def sheets():
    font = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 22)
    small = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 17)
    overview = Image.new('RGB', (1800, 1480), '#EFEADF')
    draw = ImageDraw.Draw(overview)
    for i, (name, spec) in enumerate(SPECS.items()):
        col, row = i%3, i//3
        x, y = col*600+16, row*365+15
        draw.text((x, y), f'{name} / {spec["width"]:g} x {spec["height"]:g} m', font=font, fill='#293C43')
        prefix = spec.get('prefix', f'Facade_{name}')
        im = Image.open(OUT/f'{prefix}_BaseColor.png')
        target = (560, min(310, round(560*spec['height']/spec['width'])))
        # Cap height uniformly, preserving the world-space aspect, rather than squashing it.
        if round(560*spec['height']/spec['width']) > 310:
            target = (round(310*spec['width']/spec['height']), 310)
        overview.paste(im.resize(target, Image.Resampling.LANCZOS), (x, y+38))
    overview.save(OUT/'_QA_R02_facade_overview.png')
    channels = Image.new('RGB', (1270, len(SPECS)*240+70), '#EFEADF')
    draw = ImageDraw.Draw(channels)
    for c, name in enumerate(('BASE COLOR', 'ROUGHNESS', 'NORMAL +Y', 'METALLIC')):
        draw.text((270+c*245, 22), name, font=font, fill='#293C43')
    for row, (name, spec) in enumerate(SPECS.items()):
        y = row*240+70
        draw.text((15, y+62), name, font=font, fill='#293C43')
        draw.text((15, y+94), f'{spec["width"]:g} x {spec["height"]:g} m / UV 0..1', font=small, fill='#5D6563')
        for col, channel in enumerate(('BaseColor', 'Roughness', 'Normal', 'Metallic')):
            prefix = spec.get('prefix', f'Facade_{name}')
            im = Image.open(OUT/f'{prefix}_{channel}.png').convert('RGB')
            channels.paste(im.resize((225, 225), Image.Resampling.LANCZOS), (270+col*245, y))
    channels.save(OUT/'_QA_R02_facade_channels.png')


def radio_crown():
    """Four upper-floor panes, cut between mullions for a clean U repeat."""
    source_w, source_h = 34., 18.
    x0, x1, z0, z1 = .30, .30+(34.-.60)*4/6, 13.35, 17.70
    crop = (round(x0/source_w*SIZE), round((1-z1/source_h)*SIZE),
            round(x1/source_w*SIZE), round((1-z0/source_h)*SIZE))
    maps = {}
    for channel in ('BaseColor', 'Roughness', 'Normal', 'Metallic'):
        source = OUT/f'Facade_TerminalGlass_{channel}.png'
        filename = f'Facade_RadioCrown_{channel}.png'
        with Image.open(source) as image:
            result = image.crop(crop).resize((1024, 256), Image.Resampling.LANCZOS)
            result.save(OUT/filename)
        with Image.open(OUT/filename) as image:
            assert image.size == (1024, 256)
            assert image.mode == ('RGB' if channel in ('BaseColor', 'Normal') else 'L')
            if channel == 'Metallic':
                assert image.getextrema() == (0, 0), 'The crown strip must not contain door handles.'
        maps[channel] = dict(file=filename, mode='RGB' if channel in ('BaseColor', 'Normal') else 'L',
                             color_space='sRGB' if channel == 'BaseColor' else 'Non-Color')
    return dict(wall_meters=[x1-x0, z1-z0], resolution=[1024, 256],
                source_atlas='TerminalGlass', source_crop_pixels=list(crop),
                source_crop_world_meters=[x0, z0, x1, z1], maps=maps,
                windows_per_u_repeat=4, recommended_u_repeats=3, windows_full_circumference=12,
                doors=[], sampling='Repeat U three times around the crown; V spans 0..1 once. Four large upper-floor panes per repeat, no door.')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if '--radio-crown-only' in sys.argv:
        path = OUT/'R02_Facade_Manifest.json'
        manifest = json.loads(path.read_text(encoding='utf-8'))
        manifest['facades']['RadioCrown'] = radio_crown()
        manifest['png_maps'] = len(manifest['facades'])*4
        path.write_text(json.dumps(manifest, indent=2), encoding='utf-8')
        print('RadioCrown: 4 x 1024x256 maps ready; existing 48 PNGs unchanged.')
        return
    families = {name: make(name, spec) for name, spec in SPECS.items()}
    families['RadioCrown'] = radio_crown()
    sheets()
    manifest = dict(version='Goldenrod-R02', default_resolution=[SIZE, SIZE], facades=families,
                    glass_color_revision='Deeper blue/teal glass for direct sunlight: default #659CAD, warm #B9A66E, Terminal/RadioCrown #4C9BAC, StationArch #70B7CA, DoorBlue #63A4B7. Glass roughness 0.21-0.24; facade color, framing, layout and normal depth preserved.',
                    png_maps=len(families)*4, source='Original full-wall raster authoring with Pillow and NumPy; reference-guided colors, window proportions and bands. No window geometry.',
                    coordinates='Meters from wall bottom-left. UV 0..1 spans the entire wall. U is left to right; V is bottom to top. PNG row 0 is wall top. Clamp at texture edges.',
                    opacity='Every BaseColor PNG is RGB, fully opaque. Use a single opaque PBR shader on the existing wall; no alpha blend, overlay mesh or layered window shader.',
                    normal='OpenGL +Y tangent normal. Flip Green Channel for Unreal default DirectX tangent convention. Total normal-only depth range is at most 0.04m; this does not displace geometry.',
                    metallic='Walls, paint, glazing and coated frames are dielectric (0). Bare aluminum door handles are 1, with filtered edges.',
                    sampling='One complete facade per wall. Preserve each supplied wall_meters aspect where possible; do not tile these atlases per window.',
                    doors='Centered entry on every atlas. Use the entry atlas on the intended entrance face; author a separate side/back crop or facade assignment if an entry on those sides is unwanted.',
                    limits='Glazing is an opaque blue/turquoise PBR representation with low roughness. No transparent interior or directional reflection is baked in.',
                    qa='All saved PNGs reopened and checked for declared dimensions (1024 square facades; DoorBlue 512 square; RadioCrown 1024x256) and RGB/L mode. Recorded rectangular windows have >=2m clear pane width and height. Station arch circles have 2.14m clear glass diameter. Door sill positions and finite normal fields are checked. Visual sheets show world-space aspect and all four channels.')
    (OUT/'R02_Facade_Manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps(dict(facades=len(families), png_maps=len(families)*4, output=str(OUT)), indent=2))


if __name__ == '__main__':
    main()
