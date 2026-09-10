"""Bake Goldenrod R02 accent colors and facade sign art into direct-use PBR PNGs."""

import hashlib
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont


OUT = Path(__file__).resolve().parents[2] / 'SourceArt/Environments/Goldenrod/Rebuild/Textures'
SIZE = 1024
PALETTE = {
    'Cream': '#F0DDB4', 'Ochre': '#D29A45', 'BrickRed': '#BB4F2C',
    'Navy': '#233443', 'Cobalt': '#24558C', 'Teal': '#21665D',
    'Pink': '#D74865', 'Orange': '#DC7F24', 'Bronze': '#BC7B32',
    'Gold': '#D8B153', 'Charcoal': '#26323A', 'Aluminum': '#AAB9BD',
    'Green': '#386B3A', 'Leaf': '#51833D', 'Water': '#247C98',
    'Glow': '#FFEACA', 'CyanGlow': '#72D9F2', 'RoofBrown': '#60422C',
}
SIGN_ASPECTS = {'Station': 4, 'Dept': .25, 'Gym': 1, 'Arcade': 1.5,
                'Radio': 3, 'Terminal': .4, 'Center': 1}


def to_linear(color):
    return np.where(color <= .04045, color / 12.92, ((color + .055) / 1.055) ** 2.4)


def to_srgb(color):
    return np.where(color <= .0031308, color * 12.92, 1.055 * np.maximum(color, 0) ** (1 / 2.4) - .055)


def resized_source(prefix, channel):
    with Image.open(OUT / f'{prefix}_{channel}.png') as source:
        image = source.convert('L' if channel in ('Roughness', 'Metallic') else 'RGB').resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    if channel == 'Normal':
        n = np.asarray(image, dtype=np.float32) / 255 * 2 - 1
        n /= np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-8)
        image = Image.fromarray(np.rint(np.clip(n * .5 + .5, 0, 1) * 255).astype('uint8'))
    return image


def write_maps(prefix, base, roughness, normal, metallic):
    result = {}
    for channel, image in {'BaseColor': base, 'Roughness': roughness, 'Normal': normal,
                           'Metallic': Image.new('L', (SIZE, SIZE), round(metallic * 255))}.items():
        path = OUT / f'{prefix}_{channel}.png'
        assert image.size == (SIZE, SIZE)
        assert image.mode == ('L' if channel in ('Roughness', 'Metallic') else 'RGB')
        image.save(path, optimize=True)
        with Image.open(path) as check:
            assert check.size == (SIZE, SIZE) and check.mode == image.mode
            if channel == 'Metallic':
                assert check.getextrema() == (round(metallic * 255),) * 2
        result[channel] = {'file': path.name, 'color_space': 'sRGB' if channel == 'BaseColor' else 'Non-Color',
                           'bytes': path.stat().st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    return result


def accent(name, color):
    source = 'WallPlaster' if name in ('Cream', 'Ochre', 'BrickRed') else 'PaintedMetal'
    tint = np.array([int(color[i:i+2], 16) / 255 for i in (1, 3, 5)])
    original = np.asarray(resized_source(source, 'BaseColor'), dtype=np.float32) / 255
    baked = to_srgb(to_linear(original) * to_linear(tint))
    base = Image.fromarray(np.rint(np.clip(baked, 0, 1) * 255).astype('uint8'))
    rough = resized_source(source, 'Roughness')
    if name in ('Water', 'Glow', 'CyanGlow'):
        rough = Image.new('L', (SIZE, SIZE), round((.18 if name == 'Water' else .30) * 255))
    metallic = 1 if name in ('Bronze', 'Aluminum') else 0
    maps = write_maps('Accent_' + name, base, rough, resized_source(source, 'Normal'), metallic)
    return {'source_surface': source, 'tint_srgb_baked_in_linear_space': color,
            'tile_meters': [2, 2], 'metallic': metallic, 'maps': maps}


def fit_text(draw, text, box, color, max_size):
    x0, y0, x1, y1 = box
    for size in range(max_size, 5, -1):
        font = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', size)
        bounds = draw.textbbox((0, 0), text, font=font)
        if bounds[2] - bounds[0] <= x1 - x0 and bounds[3] - bounds[1] <= y1 - y0:
            break
    draw.text(((x0+x1-bounds[2]+bounds[0])/2 - bounds[0],
               (y0+y1-bounds[3]+bounds[1])/2 - bounds[1]), text, fill=color, font=font)


def sign(name, aspect):
    # Author in its physical face aspect, then store square pixels for UV 0..1.
    w = 2048 if aspect >= 1 else round(2048 * aspect)
    h = round(2048 / aspect) if aspect >= 1 else 2048
    backgrounds = {'Station': 'Teal', 'Dept': 'Pink', 'Gym': 'Cobalt', 'Arcade': 'Navy',
                   'Radio': 'Charcoal', 'Terminal': 'Charcoal', 'Center': 'Cream'}
    im = Image.new('RGB', (w, h), PALETTE[backgrounds[name]])
    d = ImageDraw.Draw(im)
    s = min(w, h)
    white, gold, navy, pink = PALETTE['Glow'], PALETTE['Gold'], PALETTE['Navy'], PALETTE['Pink']
    if name in ('Station', 'Radio'):
        pad = round(s * .08)
        d.rounded_rectangle((pad, pad, w-pad, h-pad), radius=round(s*.05), outline=gold, width=round(s*.022))
        fit_text(d, 'GOLDENROD' if name == 'Station' else 'RADIO',
                 (s*.23, h*.20, w-s*.23, h*.80), white, round(h*.66))
    elif name == 'Dept':
        d.rectangle((w*.045, h*.025, w*.955, h*.975), outline=gold, width=round(w*.028))
        for i, letter in enumerate('DEPT'):
            fit_text(d, letter, (w*.15, h*(.045+i*.23), w*.85, h*(.245+i*.23)), white, round(w*.80))
    elif name == 'Gym':
        d.rounded_rectangle((s*.10, s*.10, s*.90, s*.90), radius=round(s*.10), outline=white, width=round(s*.032))
        shield = [(s*.50,s*.24),(s*.73,s*.34),(s*.73,s*.57),(s*.50,s*.79),(s*.27,s*.57),(s*.27,s*.34)]
        d.polygon(shield, fill=white)
        d.arc((s*.34,s*.36,s*.66,s*.68),180,360,fill=PALETTE['Cobalt'],width=round(s*.052))
        d.rectangle((s*.35,s*.55,s*.65,s*.60),fill=PALETTE['Cobalt'])
    elif name == 'Arcade':
        cx, cy = w / 2, h * .69
        rx, ry = w * .45, h * .60
        oval = (cx-rx,cy-ry,cx+rx,cy+ry)
        d.pieslice(oval,180,360,fill=PALETTE['Navy'],outline=pink,width=round(s*.055))
        for angle in (195,220,245,270,295,320,345):
            a=math.radians(angle)
            d.line((cx,cy,cx+rx*.85*math.cos(a),cy+ry*.85*math.sin(a)),fill=gold,width=round(s*.027))
        for i in range(12):
            d.rectangle((i*w/12,h*.70,(i+1)*w/12,h*.965),fill=PALETTE['Green'] if i%2 else gold)
        d.rectangle((0,h*.675,w,h*.72),fill=pink)
    elif name == 'Terminal':
        p = w*.09
        d.rectangle((p,h*.018,w-p,h*.982),outline=pink,width=round(w*.055))
        cy = h*.60
        for r in (w*.18,w*.29,w*.40):
            d.arc((w/2-r,cy-r,w/2+r,cy+r),205,335,fill=gold,width=round(w*.043))
        r=w*.056
        d.ellipse((w/2-r,cy-r,w/2+r,cy+r),fill=gold)
        d.rectangle((w*.28,h*.73,w*.72,h*.975),outline=gold,width=round(w*.035))
    elif name == 'Center':
        cx=cy=s/2
        outer=s*.40
        box=(cx-outer,cy-outer,cx+outer,cy+outer)
        d.ellipse(box,fill=navy)
        r=s*.37
        inner=(cx-r,cy-r,cx+r,cy+r)
        d.pieslice(inner,180,360,fill='#D8442F')
        d.pieslice(inner,0,180,fill='#F8F0D9')
        d.rectangle((cx-r,cy-s*.035,cx+r,cy+s*.035),fill=navy)
        r=s*.13
        d.ellipse((cx-r,cy-r,cx+r,cy+r),fill=navy)
        r=s*.085
        d.ellipse((cx-r,cy-r,cx+r,cy+r),fill='#F8F0D9')
    base=im.resize((SIZE,SIZE),Image.Resampling.LANCZOS)
    maps=write_maps('Sign_'+name,base,Image.new('L',(SIZE,SIZE),round(.34*255)),
                    Image.new('RGB',(SIZE,SIZE),(128,128,255)),0)
    return {'display_width_over_height':aspect,'uv_bounds':[0,0,1,1],
            'source_surface':'original opaque sign art', 'metallic':0,'roughness':.34,'maps':maps}


def contact_sheet(materials):
    sheet=Image.new('RGB',(1440,1320),'#ECEDE7')
    d=ImageDraw.Draw(sheet)
    font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',19)
    small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',15)
    d.text((20,14),'GOLDENROD R02 / BAKED ACCENTS + SURFACE SIGN ART',fill='#233443',font=font)
    for i,(name,color) in enumerate(PALETTE.items()):
        x=20+(i%6)*238;y=55+(i//6)*224
        with Image.open(OUT/f'Accent_{name}_BaseColor.png') as im:
            sheet.paste(im.resize((208,176),Image.Resampling.LANCZOS),(x,y))
        d.text((x,y+182),name+'  '+color,fill='#233443',font=small)
    start=756
    for i,(name,aspect) in enumerate(SIGN_ASPECTS.items()):
        x=20+(i%4)*356;y=start+(i//4)*272
        with Image.open(OUT/f'Sign_{name}_BaseColor.png') as im:
            w=min(320,round(235*aspect));h=round(w/aspect)
            sheet.paste(im.resize((w,h),Image.Resampling.LANCZOS),(x+(320-w)//2,y))
        d.text((x,y+242),f'Sign_{name} / face aspect {aspect:g}',fill='#233443',font=small)
    sheet.save(OUT/'_QA_R02_accents_and_signs.png',optimize=True)


def main():
    for source in ('WallPlaster','PaintedMetal'):
        for channel in ('BaseColor','Roughness','Normal'):
            if not (OUT/f'{source}_{channel}.png').exists():
                raise FileNotFoundError(OUT/f'{source}_{channel}.png')
    materials={'Accent_'+name:accent(name,color) for name,color in PALETTE.items()}
    materials.update({'Sign_'+name:sign(name,aspect) for name,aspect in SIGN_ASPECTS.items()})
    assert len(materials)==25
    contact_sheet(materials)
    report={
        'version':'Goldenrod-R02','generator':Path(__file__).name,'resolution':[SIZE,SIZE],
        'source':'Deterministic Pillow/NumPy authoring from the local R02 WallPlaster/PaintedMetal maps; no external assets or image-generation service.',
        'accent_count':18,'sign_count':7,'map_count':100,
        'usage':'BaseColor tint is already baked in linear color space. Connect PNGs directly to PBR inputs; use white BaseColor multiplier and do not reapply palette tint.',
        'normal_convention':'OpenGL tangent +Y. Resampled source normals are renormalized. Flip green for Unreal default DirectX convention.',
        'sign_uv_rule':'Signs are opaque RGB. Map full UV 0..1 onto a face with the per-sign display_width_over_height ratio. The square PNG intentionally stores this aspect anamorphically.',
        'metallic_rule':'Accent_Bronze and Accent_Aluminum are 1; all other accents and every sign are 0.',
        'optical_note':'Water/Glow/CyanGlow use roughness .18/.30/.30. Emission or water animation is a later shader setting; these four maps do not encode it.',
        'qa':'All 100 files checked for exact 1024x1024 size, RGB/L modes and metallic values; contact sheet uses intended sign face aspect.',
        'total_png_bytes':sum(channel['bytes'] for material in materials.values() for channel in material['maps'].values()),
        'uncompressed_channel_bytes_without_mips':25*SIZE*SIZE*8,
        'contact_sheet':'_QA_R02_accents_and_signs.png','materials':materials,
    }
    (OUT/'R02_Accent_Manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({k:report[k] for k in ('accent_count','sign_count','map_count','total_png_bytes','uncompressed_channel_bytes_without_mips')}))


if __name__=='__main__':
    main()
