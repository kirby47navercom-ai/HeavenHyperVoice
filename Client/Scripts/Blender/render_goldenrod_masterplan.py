"""Render the approved masterplan data as a matching vector/raster review sheet.

Run with Python + Pillow; no Blender scene, assets or source JSON are changed.
"""

import json
import math
from html import escape
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "SourceArt/Environments/Goldenrod"
DATA = json.loads((OUT / "Goldenrod_Masterplan.json").read_text(encoding="utf-8-sig"))
W, H = 3200, 2500
PAPER, INK, MUTED = "#faf8f2", "#20353b", "#637477"
COLORS = dict(zip(
    [f"D{i}" for i in range(1, 10)],
    ["#9ebcce", "#e6c28d", "#a5b8b0", "#d4c59e", "#d2b6b8",
     "#b6c4cb", "#c7cbb0", "#c8b3cc", "#bbb8ae"],
))
ROUTE_COLORS = ["#27826d", "#2768a3", "#af7224", "#914d86"]
im = Image.new("RGB", (W, H), PAPER)
draw = ImageDraw.Draw(im)
svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
       f'<title>Goldenrod Metropolitan Reinterpretation - masterplan {escape(DATA["revision"])}</title>',
       '<desc>North is +Y. Metric plan of the 640 m core, five primary landmarks, the waterfront Global Terminal, nine districts, roads, elevated rail and four pedestrian routes. Review drawing only; no Blender materials.</desc>']
fonts = {}


def font(size, bold=False):
    key = (size, bold)
    if key not in fonts:
        fonts[key] = ImageFont.truetype(f"C:/Windows/Fonts/{'segoeuib' if bold else 'segoeui'}.ttf", size)
    return fonts[key]


def rect(box, fill=PAPER, stroke=None, width=1):
    x1, y1, x2, y2 = box
    draw.rectangle(box, fill=fill, outline=stroke, width=width)
    svg.append(f'<rect x="{x1:.2f}" y="{y1:.2f}" width="{x2-x1:.2f}" height="{y2-y1:.2f}" fill="{fill or "none"}" stroke="{stroke or "none"}" stroke-width="{width}"/>')


def line(points, color=INK, width=2, dash=None):
    if dash:
        for a, b in zip(points, points[1:]):
            dist = math.dist(a, b)
            for k in range(0, math.ceil(dist), sum(dash)):
                p = [(a[j] + (b[j] - a[j]) * d / dist) for d in (k, min(k + dash[0], dist)) for j in (0, 1)]
                draw.line(p, fill=color, width=width)
    else:
        draw.line(points, fill=color, width=width, joint="curve")
    attrs = f' stroke-dasharray="{dash[0]} {dash[1]}"' if dash else ""
    svg.append(f'<polyline points="{" ".join(f"{x:.2f},{y:.2f}" for x,y in points)}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linejoin="round"{attrs}/>')


def text(x, y, value, size=26, color=INK, bold=False, anchor="left"):
    value = str(value)
    length = draw.textlength(value, font=font(size, bold))
    x -= length * {"left": 0, "center": .5, "right": 1}[anchor]
    draw.text((x, y), value, fill=color, font=font(size, bold), anchor="lt")
    svg.append(f'<text x="{x:.2f}" y="{y:.2f}" font-family="Segoe UI, sans-serif" font-size="{size}" font-weight="{700 if bold else 400}" fill="{color}" dominant-baseline="text-before-edge">{escape(value)}</text>')


def circle(x, y, r, fill=INK, stroke=None, width=1):
    draw.ellipse((x-r, y-r, x+r, y+r), fill=fill, outline=stroke, width=width)
    svg.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r}" fill="{fill}" stroke="{stroke or "none"}" stroke-width="{width}"/>')


def polygon(points, fill=INK):
    draw.polygon(points, fill=fill)
    svg.append(f'<polygon points="{" ".join(f"{x:.2f},{y:.2f}" for x,y in points)}" fill="{fill}"/>')


SCALE = 2.15
LEFT, TOP = 140, 330
XMIN, YMAX = -450, 410


def point(x, y):
    return LEFT + (x-XMIN)*SCALE, TOP + (YMAX-y)*SCALE


def maprect(bounds, fill, stroke=None, width=1):
    a, b = point(bounds[0], bounds[3]), point(bounds[2], bounds[1])
    rect((*a, *b), fill, stroke, width)


def badge(x, y, label, color=INK, size=22):
    tw = draw.textlength(label, font=font(size, True))
    rect((x-tw/2-13, y-7, x+tw/2+13, y+size+8), PAPER, "#d6d8cf")
    text(x, y, label, size, color, True, "center")


def section(y, number, title):
    text(2190, y, number, 23, "#a17d4c", True)
    text(2250, y-2, title, 28, INK, True)
    line([(2190, y+42), (3080, y+42)], "#d8d9d0", 2)


# Header and a quiet editorial frame.
rect((0, 0, W, H))
text(110, 67, "GOLDENROD", 82, INK, True)
text(114, 170, "METROPOLITAN REINTERPRETATION  /  CITY MASTERPLAN", 30, MUTED)
text(3080, 78, f"{DATA['revision']}  /  GRAYBOX REVIEW", 30, INK, True, "right")
text(3080, 124, "640 x 640 m core  |  north +Y  |  meters", 25, MUTED, anchor="right")
text(3080, 166, "HeavenHyperVoice  /  Blender to Unreal Engine 5", 24, MUTED, anchor="right")
line([(110, 231), (3080, 231)], INK, 3)
rect((92, 275, 2102, 2160), "#f3f1e8", "#dddcd2", 2)
maprect((-450, -410, 410, 410), "#efece2")
coast = DATA["coast"]
water = coast["water_rect"]
maprect(water, "#d8e6e7")
for y in range(int(water[1])+24, int(water[3]), 24):
    line([point(water[0]+10, y), point(water[0]+40, y)], "#c3d7da", 1)
maprect((-320, -410, 410, 410), "#efece2")
maprect(coast["peninsula_rect"], "#e9dfca", "#859d9d", 2)
maprect(coast["bridge_rect"], "#bfc9c4", "#758d90", 2)
for x in range(-320, 321, 80):
    line([point(x, -320), point(x, 320)], "#e6e4d8", 1)
for y in range(-320, 321, 80):
    line([point(-320, y), point(320, y)], "#e6e4d8", 1)

# Roads use their JSON footprint, including sidewalks, not decorative widths.
for road in sorted(DATA["roads"], key=lambda r: r["kind"] == "main"):
    b = road["rect"]
    maprect(b, "#d8d0bd" if road["kind"] == "main" else "#e1dfd4")
    inset = road["sidewalk"]
    carriageway = ([b[0]+inset, b[1], b[2]-inset, b[3]] if road["axis"] == "Y"
                   else [b[0], b[1]+inset, b[2], b[3]-inset])
    maprect(carriageway, "#fffdf7" if road["kind"] != "alley" else "#fcfaf5")

for space in DATA["spaces"]:
    fill = {"park": "#d0ddc1", "plaza": "#e9dfca", "reserve": "#e1d9c3"}[space["kind"]]
    maprect(space["rect"], fill, "#a9b49d" if space["kind"] == "park" else "#c8bfa8")

heroes = [b for b in DATA["buildings"] if b.get("hero")]
secondaries = [b for b in DATA["buildings"] if b.get("secondary")]
for b in DATA["buildings"]:
    if b.get("hero") or b.get("secondary"):
        continue
    fill = "#e1e1d8" if b.get("context") else COLORS[b["district"]]
    maprect(b["rect"], fill, "#cecec3" if b.get("context") else "#939c93", 1)
    if b.get("h", 0) >= 40 and not b.get("context"):
        rr = b["rect"]
        maprect([rr[0]+3, rr[1]+3, rr[2]-3, rr[3]-3], None, "#87958e", 1)

# Rail bridge is a separate elevated layer; street routes pass underneath.
rail = DATA["rail"]
maprect(rail["rect"], "#71868b", "#4b626a", 2)
r = rail["rect"]
rail_mid = (r[1]+r[3])/2
rail_half = (r[3]-r[1])/2
for y in (rail_mid-rail_half/3, rail_mid+rail_half/3):
    line([point(r[0], y), point(r[2], y)], "#e8eeea", 2)
for x in range(int(r[0]), int(r[2]), 12):
    line([point(x, rail_mid-rail_half*.6), point(x, rail_mid+rail_half*.6)], "#b4c2c2", 1)

for route in DATA["routes"]:
    line([point(*p) for p in route["points"]], PAPER, 11)
for route, color in zip(DATA["routes"], ROUTE_COLORS):
    pts = [point(*p) for p in route["points"]]
    line(pts, color, 6, (11, 8) if route["name"] == "GYM CONNECTION" else None)
    circle(*pts[0], 7, PAPER, color, 3)
    a, b = pts[-2:]
    length = math.dist(a, b)
    if length:
        dx, dy = (b[0]-a[0])/length, (b[1]-a[1])/length
        polygon([b, (b[0]-13*dx+6*dy,b[1]-13*dy-6*dx),
                 (b[0]-13*dx-6*dy,b[1]-13*dy+6*dx)], color)

# Hero identifiers remain unobscured and match the dimension schedule.
hero_names = {"RadioTower": "RADIO TOWER", "Station": "STATION", "DepartmentStore": "DEPARTMENT", "Gym": "GYM", "PokemonCenter": "CENTER"}
for i, b in enumerate(heroes, 1):
    maprect(b["rect"], "#435d64", "#20353b", 3)
    x, y = point(b["x"], b["y"])
    circle(x, y-10, 22, PAPER)
    text(x, y-25, f"{i:02}", 26, INK, True, "center")
    if b["w"] >= 40:
        text(x, y+20, hero_names[b["name"]], 18, PAPER, True, "center")

secondary_codes = {"GlobalTerminal": "GT", "FlowerShop": "FS", "ShoppingArcade": "SA"}
for b in secondaries:
    maprect(b["rect"], "#617a80", "#304f58", 3)
    rr = b["rect"]
    maprect([rr[0]+5, rr[1]+5, rr[2]-5, rr[3]-5], None, "#c1d1d0", 2)
    text(*point(b["x"], b["y"]+4), secondary_codes[b["name"]], 25, PAPER, True, "center")
    if b["name"] == "FlowerShop":
        text(*point(b["x"], rr[1]-8), "FLOWER", 16, INK, True, "center")
        text(*point(b["x"], rr[1]-17), "SHOP", 16, INK, True, "center")
    else:
        text(*point(b["x"], rr[1]-8), b["label"], 18, INK, True, "center")
    if b["name"] == "GlobalTerminal":
        text(*point(b["x"], rr[1]-19), f"{b['h']:g} m HIGH", 16, MUTED, anchor="center")

# District badges are keyed to the actual district-assigned buildings.
# Small placement adjustments keep the badges clear of building footprints.
district_at = {"D1": (-81, 99), "D2": (76, -28), "D3": (-234, 111),
               "D4": (81, 112), "D5": (-97, -39), "D6": (235, 109),
               "D7": (233, -218), "D8": (84, 292), "D9": (-231, -46)}
for d in DATA["districts"]:
    badge(*point(*district_at[d["id"]]), d["id"], size=24)

# Road and route labels sit in known open spaces, not over hero footprints.
badge(*point(159, 38), "MEDIA AVENUE / 22 m", MUTED, 19)
badge(*point(92, -171), "R1", ROUTE_COLORS[0], 20)
line([point(78, -175), point(10, -175)], ROUTE_COLORS[0], 2)
badge(*point(50, 7), "R2", ROUTE_COLORS[1], 20)
line([point(37, 3), point(10, 3)], ROUTE_COLORS[1], 2)
badge(*point(-155, 149), "R3", ROUTE_COLORS[2], 20)
line([point(-155, 142), point(-155, 130)], ROUTE_COLORS[2], 2)
badge(*point(38, 170), "R4", ROUTE_COLORS[3], 20)
line([point(25, 166), point(10, 166)], ROUTE_COLORS[3], 2)
text(*point(-433, -118), "WEST", 22, "#60828a", True)
text(*point(-433, -135), "WATERFRONT", 18, "#60828a")
text(*point(168, 220), "ELEVATED RAIL", 19, "#476570", True)
text(*point(168, 209), f"+{rail['deck_top']:g} m deck / {rail['clearance']:g} m clearance", 17, "#476570")
text(*point(0, 345), "NORTH / ROUTE 35", 21, MUTED, True, "center")
text(*point(0, -333), "SOUTH / ROUTE 34", 21, MUTED, True, "center")
core = DATA["core_bounds"]
pts = [point(core[0], core[1]), point(core[2], core[1]), point(core[2], core[3]), point(core[0], core[3]), point(core[0], core[1])]
line(pts, "#8b958b", 2, (12, 8))
text(130, 292, "ORTHOGRAPHIC / +Y NORTH", 20, MUTED, True)
text(2056, 292, "CORE 0.4096 km2", 20, MUTED, anchor="right")
nx, ny = 1966, 408
polygon([(nx,ny-52),(nx-17,ny+14),(nx,ny),(nx+17,ny+14)], INK)
text(nx, ny-88, "N", 30, INK, True, "center")

# Right-hand reading order: landmarks, districts, infrastructure, routes.
section(285, "01", "LANDMARK SCHEDULE")
for i, b in enumerate(heroes, 1):
    y = 355 + (i-1)*110
    circle(2214, y+15, 23, INK)
    text(2214, y-1, f"{i:02}", 25, PAPER, True, "center")
    text(2257, y-3, b["label"], 28, INK, True)
    text(2257, y+38, f"{b['w']} x {b['d']} m footprint    /    {b['h']} m high    /    {b['floors']} floors", 23, MUTED)
    text(2257, y+72, f"X {b['x']:+g} m   Y {b['y']:+g} m", 21, MUTED)

section(932, "02", "DISTRICTS")
district_names = ["Station / transit", "Department / retail", "Radio / civic", "Main shopping street", "Entertainment", "Mixed use", "Residential edge", "Gym / flower street", "Alleys / service"]
for i, (d, label) in enumerate(zip(DATA["districts"], district_names)):
    col, row = (i % 2), (i // 2)
    x, y = 2190 + col*448, 1006 + row*69
    rect((x, y, x+46, y+38), COLORS[d["id"]], "#929b91")
    text(x+23, y+5, d["id"], 23, INK, True, "center")
    text(x+61, y+5, label, 24, INK)

section(1388, "03", "MOVEMENT & OPEN SPACE")
legend = [("Main boulevard", "22 m incl. 3 m sidewalks", "#c4b597"),
          ("Secondary road", "14 m incl. 2.5 m sidewalks", "#a9aaa1"),
          ("Service alley", "6 m shared service access", "#d1cfc2"),
          ("Elevated railway", f"+{rail['deck_top']:g} m deck / {rail['clearance']:g} m clearance", "#71868b"),
          ("Public realm", "plaza / park / waterfront", "#c9d7b8")]
for i, (name, spec, color) in enumerate(legend):
    y = 1462 + i*50
    rect((2190,y+4,2240,y+25),color)
    text(2257,y,name,23,INK,True)
    text(3080,y,spec,22,MUTED,anchor="right")

section(1753, "04", "PEDESTRIAN REVIEW ROUTES")
for i, (route, color) in enumerate(zip(DATA["routes"], ROUTE_COLORS)):
    y = 1829+i*83
    line([(2190,y+17),(2238,y+17)],color,7, (11,8) if i == 3 else None)
    text(2257,y,f"R{i+1}  {route['name']}",24,INK,True)
    text(2257,y+37,f"{route['length_m']:.1f} m   |   walk {route['walk_seconds']:.1f} s   |   run {route['run_seconds']:.1f} s",23,MUTED)
text(2190, 2184, "Route times use 2.6 m/s walk / 3.9 m/s run.", 23, MUTED)
text(2190, 2220, "Planning routes; not a live collision or navigation test.", 22, MUTED)

# Scale and scope notes are part of the printed sheet, not external captions.
text(115,2210,"METRIC SCALE",23,INK,True)
barx,bary=115,2257
for i in range(4):
    rect((barx+i*50*SCALE,bary,barx+(i+1)*50*SCALE,bary+18),INK if i%2==0 else PAPER,INK,1)
for meters in (0,50,100,200):
    text(barx+meters*SCALE,bary+31,f"{meters}",22,MUTED,anchor="center")
text(barx+200*SCALE+35,bary+29,"m",22,MUTED)
text(810,2210,"640 m CORE  /  1.75 m PLAYER",26,INK,True)
core_count = sum(not b.get("context", False) for b in DATA["buildings"])
context_count = len(DATA["buildings"]) - core_count
text(810,2255,f"5 primary + {len(secondaries)} secondary landmarks  /  {core_count} planned masses  /  {context_count} context masses",23,MUTED)
text(810,2292,"Context footprints outside the dashed core are skyline placeholders.",23,MUTED)
line([(110,2352),(3080,2352)],"#c7ccc2",2)
text(110,2380,"REFERENCE  Six local reference images + 55.53 s reference video reviewed; HGSS spatial relationships adapted to metropolitan scale.",24,MUTED)
text(110,2424,"REVIEW ONLY  District colors are diagram keys; no Blender materials. Existing 512 m server grid does not cover this 640 m city core.",24,MUTED)

assert len(heroes) == 5 and len(DATA["districts"]) == 9 and len(DATA["routes"]) == 4
assert point(0, 1)[1] < point(0, 0)[1], "North must point upward."
svg.append("</svg>")
OUT.mkdir(parents=True, exist_ok=True)
(OUT / "Goldenrod_Masterplan.svg").write_text("\n".join(svg), encoding="utf-8")
im.save(OUT / "Goldenrod_Masterplan.png", dpi=(300,300))
print(f"Saved {W}x{H} PNG and editable SVG to {OUT}")
