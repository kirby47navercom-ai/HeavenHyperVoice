"""Read R03 mesh islands and emit native, server-exportable prop boxes. Never saves the blend."""
import bpy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
rows = []
summary = {}


def islands(obj):
    mesh = obj.data
    parents = list(range(len(mesh.vertices)))
    def root(i):
        while parents[i] != i:
            parents[i] = parents[parents[i]]
            i = parents[i]
        return i
    for edge in mesh.edges:
        a, b = (root(v) for v in edge.vertices)
        parents[a] = b
    groups = {}
    for vertex in mesh.vertices:
        groups.setdefault(root(vertex.index), []).append(obj.matrix_world @ vertex.co)
    for points in groups.values():
        lo = [min(p[i] for p in points) for i in range(3)]
        hi = [max(p[i] for p in points) for i in range(3)]
        yield lo, hi


def emit(kind, lo, hi):
    count = summary.get(kind, 0)
    summary[kind] = count + 1
    rows.append(dict(name=f'Prop_{kind}_{count:03d}',
        min=[round(lo[0]*100, 3), round(-hi[1]*100, 3), round(lo[2]*100, 3)],
        max=[round(hi[0]*100, 3), round(-lo[1]*100, 3), round(hi[2]*100, 3)]))


for group in ('Trees', 'StreetLamps', 'Planters', 'Benches', 'CoastBollards', 'Fountains'):
    obj = bpy.data.objects['SM_R02_'+group]
    for lo, hi in sorted(islands(obj), key=lambda b: (b[0][0], b[0][1], b[0][2])):
        w, d, h = [hi[i]-lo[i] for i in range(3)]
        if group == 'Trees' and abs(lo[2]-.12) < .01 and w < .6 and h > 1.5:
            emit('TreeTrunk', lo, hi)
        elif group == 'StreetLamps':
            if abs(lo[2]-.12) < .01:
                emit('LampBase', lo, hi)
            elif h > 3:
                emit('LampPost', lo, hi)
            elif 1 < h < 1.3:
                emit('LampGlobe', lo, hi)
        elif group == 'Planters' and abs(lo[2]-.12) < .01 and w > 4:
            emit('Planter', lo, hi)
        elif group == 'Benches' and w > 2:
            emit('BenchSeat' if h < .2 else 'BenchBack', lo, hi)
        elif group == 'CoastBollards':
            emit('Bollard', lo, hi)
        elif group == 'Fountains' and .7 < h < .9:
            emit('FountainBowl', lo, hi)
        elif group == 'Fountains' and 1 < h < 1.2:
            emit('FountainPedestal', lo, hi)

expected = {'TreeTrunk': 108, 'LampBase': 30, 'LampPost': 30, 'LampGlobe': 30,
            'Planter': 10, 'BenchSeat': 6, 'BenchBack': 6, 'Bollard': 50,
            'FountainBowl': 2, 'FountainPedestal': 2}
# Print counts before deciding whether the native model matches this extraction revision.
print('PROP_COUNTS', json.dumps(summary), flush=True)
if summary != expected:
    raise RuntimeError('Prop counts differ from R03 geometry; inspect before replacing native collision')
output = ROOT/'Source/HeavenHyperVoice/World/GoldenrodPropCollision.inl'
lines = ['// Generated from connected mesh islands in Goldenrod_City_R03.blend.',
         '// Local UE cm; tree crowns intentionally remain passable. Regenerate with extract_goldenrod_prop_collision.py.']
for row in rows:
    def vector(values): return 'FVector('+', '.join(f'{v:.3f}' for v in values)+')'
    lines.append(f'AddCollisionBox(TEXT("{row["name"]}"), {vector(row["min"])}, {vector(row["max"])}, false);')
output.write_text('\n'.join(lines)+'\n', encoding='utf-8')
report = ROOT/'Saved/Codex/Goldenrod/prop_collision.json'
report.write_text(json.dumps({'counts': summary, 'total': len(rows), 'boxes': rows},indent=2),encoding='utf-8')
