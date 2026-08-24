# 카탈로그가 20칸을 채웠고 각 칸이 스켈레탈 메시를 물었는지 확인만 한다.
import unreal

cat = unreal.load_asset("/Game/Pokemon/DA_PokemonSpeciesCatalog.DA_PokemonSpeciesCatalog")
if cat is None:
    unreal.log_error("[VERIFY] catalog not found")
else:
    species = cat.get_editor_property("species")
    unreal.log("[VERIFY] catalog entries: {}".format(len(species)))
    ok, bad = 0, 0
    for i, data in enumerate(species, 1):
        if data is None:
            unreal.log_warning("[VERIFY] id {}: EMPTY".format(i))
            bad += 1
            continue
        sid = data.get_editor_property("species_id")
        mesh = data.get_editor_property("skeletal_mesh")
        mesh_name = mesh.get_name() if mesh else "NONE"
        if mesh is None:
            bad += 1
        else:
            ok += 1
        unreal.log("[VERIFY] id {:2d}  species_id={}  mesh={}".format(i, sid, mesh_name))
    unreal.log("[VERIFY] with mesh: {}  problems: {}".format(ok, bad))
