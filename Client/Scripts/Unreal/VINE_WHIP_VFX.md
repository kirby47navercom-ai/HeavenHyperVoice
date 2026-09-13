# 덩굴채찍 VFX

에셋 폴더: `/Game/VFX/Pokemon/VineWhip`

| 에셋 | 용도 |
|---|---|
| `NS_VineWhip_Attack` | 가늘고 긴 3D 덩굴이 말려 올라갔다가 내려치고 회수되는 1회 공격. |
| `NS_VineWhip_Impact` | 연두색 섬광, 짧은 타격 자국, 흩어지는 잎 조각. |
| `BP_VineWhip_Attack` / `BP_VineWhip_Impact` | 각각 Niagara를 담은 배치용 액터. 1.5초 뒤 제거. |
| `Meshes/SM_VineWhip_Segmented` | 변형용 원통 메시. 길이 방향 96개 구간, 끝으로 갈수록 가늘어지는 형태는 머티리얼에서 처리. |
| `Materials/M_VineWhip_Vine` | 덩굴 색상과 굽힘 동작. `Reach`와 `Thickness`로 길이·두께 조절. |
| `L_VineWhip_Showcase` / `LS_VineWhip_Preview` | 공격과 피격을 함께 살펴보는 맵과 시퀀스. |

## 재생 및 연결

- 공격 루트는 원점에 고정되며 전방은 로컬 **+X**다. 포켓몬의 덩굴 시작 소켓에 `Spawn System Attached`로 붙이거나 Niagara Component에 지정한다.
- 기본 도달 거리는 **400cm**, 전체 동작은 **1.1초**다. 약 **0.638초**에 덩굴 끝이 `(400, 0, 0)`에 도달한다. 대상을 바라보는 회전과 목표까지의 거리는 호출하는 쪽에서 맞춘다. 자동 추적 기능은 없다.
- 적중 판정에 맞춰 별도 `NS_VineWhip_Impact`를 적중 위치에 `Spawn System at Location`으로 생성하고 Auto Destroy를 켠다. 공격 시스템이 피격을 자동 생성하지는 않는다.
- 이동·충돌·데미지는 포함하지 않았다. 서버의 전투 판정에 따라 클라이언트에서 재생하는 시각 효과다.

## 편집

공격 Niagara는 한 개의 Mesh Renderer로 연속된 3D 덩굴을 그린다. 화면을 향하는 평면 덩굴 이미지가 아니다. 머티리얼의 World Position Offset에서 감아 올리기 → 빠른 내려치기 → 회수를 계산한다.

`M_VineWhip_Vine`의 Material Instance를 만들면 `Reach`(기본 400)와 `Thickness`(기본 2.6)를 별도로 조절할 수 있다. 공격 Mesh Renderer의 Override Materials에 해당 인스턴스를 지정한다. 길이를 늘릴 때 Niagara Fixed Bounds와 메시 Bounds Extension도 여유 있게 조절한다.

공격의 **Scale Color.R은 색상이 아니라 0~1 재생 시간**이다. 이 곡선과 Particle Color.R → 굽힘 노드의 연결을 유지한다. 덩굴의 실제 녹색은 머티리얼에서 수정한다. Initialize Particle의 수명을 변경하면 동작 전체가 그 수명에 맞게 늘거나 줄고, 타격 시점은 수명의 58%다.

피격 레이어는 ContactFlash / CuttingStreaks / LeafFragments다. Niagara에서 입자 수·크기·수명·속도를, 각 머티리얼에서 색상·형태를 수정할 수 있다.

## 미리보기

`L_VineWhip_Showcase`를 연 다음 `LS_VineWhip_Preview`를 열어 Sequencer에서 재생하거나 시간을 이동한다. 60fps 기준 공격은 0프레임, 피격은 38프레임에 시작한다. 맵만 열면 1회 효과가 이미 끝나 보이지 않을 수 있으므로 시퀀스를 이용한다.

에디터의 재생 시점 이동과 외형·Niagara/머티리얼 컴파일을 확인했다. PIE·전투 테스트는 하지 않았다.

제작 스크립트: `create_vine_whip_vfx.py`. 기존 `create_water_gun_vfx.py`와 에디터용 `UEWaterVFXEditorLibrary`를 재사용하며, 기존 완성 시스템을 덮어쓰지 않는다. 런타임 C++ 변경은 없다.
