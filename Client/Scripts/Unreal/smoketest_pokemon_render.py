# 런타임이 실제로 타는 경로를 밟는다: 카탈로그의 Find(UFUNCTION) 로 종족
# 데이터를 꺼내고, 메시 + 애니메이션(ABP)이 물려 있는지 본다.
# (액터 스폰은 commandlet 의 빈 월드에서 불안정해 데이터 경로만 본다.
#  실제 화면 렌더링은 PIE 에서 확인할 것.)
#
# 전설 4종(기라티나2·디아루가6·아르세우스11·펄기아19)은 아직 팀원 DA 가 없어
# 비어 있는 게 정상이다 — 그 칸은 런타임에서 색 큐브로 폴백된다.
import unreal

LEGENDARY = {2, 6, 11, 19}

catalog = unreal.load_asset("/Game/Pokemon/DA_PokemonSpeciesCatalog.DA_PokemonSpeciesCatalog")

ok, fail = 0, 0
for species_id in range(1, 21):
    data = catalog.find(species_id) if catalog else None   # UFUNCTION Find
    if data is None:
        if species_id in LEGENDARY:
            unreal.log("[SMOKE] id {:2d}: empty (전설, 큐브 폴백)".format(species_id))
        else:
            unreal.log_error("[SMOKE] id {:2d}: Find -> None (기대: 데이터 있음)".format(species_id))
            fail += 1
        continue
    mesh = data.get_editor_property("skeletal_mesh")
    anim = data.get_editor_property("anim_instance_class")
    if mesh is None or anim is None:
        unreal.log_error("[SMOKE] id {:2d}: mesh={} anim={}".format(
            species_id, mesh is not None, anim is not None))
        fail += 1
    else:
        unreal.log("[SMOKE] id {:2d}: {} + {}".format(
            species_id, mesh.get_name(), anim.get_name()))
        ok += 1

# 범위 밖은 nullptr 이어야 한다 (서버가 잘못된 번호를 보내도 안전한지)
assert catalog.find(0) is None, "id 0 은 nullptr 이어야 한다"
assert catalog.find(21) is None, "범위 밖은 nullptr 이어야 한다"

unreal.log("[SMOKE] rendered(mesh+anim): {}  failed: {}  bounds: ok".format(ok, fail))
