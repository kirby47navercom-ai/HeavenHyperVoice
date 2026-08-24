# 카탈로그 20칸이 메시 + 애님까지 물었는지, 전설 4종만 비었는지 확인한다.
import unreal

cat = unreal.load_asset("/Game/Pokemon/DA_PokemonSpeciesCatalog.DA_PokemonSpeciesCatalog")
if cat is None:
    unreal.log_error("[VERIFY] catalog not found")
else:
    species = cat.get_editor_property("species")
    unreal.log("[VERIFY] catalog entries: {}".format(len(species)))
    with_mesh, with_anim, empty = 0, 0, []
    for i, data in enumerate(species, 1):
        if data is None:
            empty.append(i)
            unreal.log("[VERIFY] id {:2d}: EMPTY (큐브 폴백)".format(i))
            continue
        mesh = data.get_editor_property("skeletal_mesh")
        anim = data.get_editor_property("anim_instance_class")
        mesh_name = mesh.get_name() if mesh else "NONE"
        anim_name = anim.get_name() if anim else "NONE"
        if mesh:
            with_mesh += 1
        if anim:
            with_anim += 1
        unreal.log("[VERIFY] id {:2d}: mesh={} anim={}".format(i, mesh_name, anim_name))
    unreal.log("[VERIFY] mesh:{} anim:{} empty:{}".format(with_mesh, with_anim, empty))

    # 런타임 Find 경로도 확인
    ok = sum(1 for sid in range(1, 21) if cat.find(sid) is not None)
    assert cat.find(0) is None and cat.find(21) is None, "범위 밖은 nullptr 이어야 한다"
    unreal.log("[VERIFY] via Find(): {}  bounds ok".format(ok))
