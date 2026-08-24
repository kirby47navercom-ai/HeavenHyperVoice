# 런타임이 실제로 타는 경로를 밟는다: 카탈로그의 Find(UFUNCTION) 로 종족
# 데이터를 꺼내고, 그 데이터에 스켈레탈 메시가 물려 있는지 본다.
# (액터 스폰은 commandlet 의 빈 월드에서 불안정해 빼고, 데이터 경로만 본다.
#  실제 화면 렌더링은 PIE 에서 확인할 것.)
import unreal

catalog = unreal.load_asset("/Game/Pokemon/DA_PokemonSpeciesCatalog.DA_PokemonSpeciesCatalog")

ok, fail = 0, 0
for species_id in range(1, 21):
    data = catalog.find(species_id) if catalog else None   # UFUNCTION Find
    mesh = data.get_editor_property("skeletal_mesh") if data else None
    name = mesh.get_name() if mesh else "NONE"
    if mesh is None:
        unreal.log_error("[SMOKE] id {:2d}: Find -> no mesh".format(species_id))
        fail += 1
    else:
        unreal.log("[SMOKE] id {:2d}: {}".format(species_id, name))
        ok += 1

# 범위 밖은 nullptr 이어야 한다 (서버가 잘못된 번호를 보내도 안전한지)
assert catalog.find(0) is None, "id 0 은 nullptr 이어야 한다"
assert catalog.find(21) is None, "범위 밖은 nullptr 이어야 한다"

unreal.log("[SMOKE] via Find(): {}  failed: {}  bounds: ok".format(ok, fail))
