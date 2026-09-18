# 열풍 VFX

에셋 폴더: `/Game/VFX/Pokemon/HeatWave`

| 에셋 | 용도 |
|---|---|
| `NS_HeatWave_Attack` | 원점에서 전방 +X로 뿜는 붉은 열풍. 바람 줄기, 반투명한 뜨거운 기류, 주황빛 선과 불씨가 반복된다. |
| `NS_HeatWave_Impact` | 맞은 위치에서 붉은 회오리, 바람에 찢기는 줄기, 짧은 섬광과 불씨가 퍼지는 1회 피격. |
| `BP_HeatWave_Attack` / `BP_HeatWave_Impact` | 각각 Niagara를 담은 배치용 블루프린트. 피격 액터는 1.5초 뒤 제거. |
| `L_HeatWave_Showcase` / `LS_HeatWave_Preview` | 두 효과를 함께 살펴보는 맵과 2초 시퀀스. |

## 연결

- `NS_HeatWave_Attack`를 `Spawn System Attached`로 입이나 발사 소켓에 붙인다. 원점은 고정되고 입자만 **로컬 +X** 방향으로 흐른다. 이동하는 투사체 액터가 아니므로 별도의 투사체 이동을 더하지 않아도 된다.
- 소켓의 전방이 +X와 다르면 부착 회전을 맞춘다. 크기는 Niagara 컴포넌트 Relative Scale로 조절한다. 기본 입자 속도는 레이어에 따라 340~760cm/s, 수명은 0.4~0.8초다. 보이는 길이는 대략 수 미터이며 충돌·공격 판정 거리와는 별개다.
- 공격 종료/취소 시 `Deactivate`하면 방출을 멈추고 남은 바람이 최대 0.8초 동안 사라진다. 즉시 없애려면 `Destroy Component`, BP 액터로 사용했다면 `Destroy Actor`한다.
- 서버의 적중 판정에 맞춰 `NS_HeatWave_Impact`를 적중 위치에 `Spawn System at Location`으로 별도 생성하고 Auto Destroy를 켠다. 피격 입자의 최대 수명은 0.62초다.
- 데미지, 충돌, 대상 추적, 기술 호출은 포함하지 않았다. 공격 시스템이 피격 시스템을 자동 생성하지 않는다.

## 편집

- 공격: `RedWindSheets`, `HotStreamlines`, `HeatedAir`, `CarriedEmbers`.
- 피격: `ScatteringGusts`, `RedPressureCloud`, `WindShear`, `HotContact`, `ScatteredEmbers`.
- Niagara의 생성량·수명·속도·Cone Angle·Scale Sprite Size에서 밀도, 길이, 퍼지는 폭을 조절한다. 길게 늘어진 바람 줄기는 Velocity Aligned로 이동 방향을 따라간다.
- `M_HeatWave_*` 머티리얼의 `Brightness`와 Custom 노드에서 밝기, 색, 바람 곡선과 마스크를 조절한다. 별도 텍스처 원본이 필요하지 않다. `HotAir`는 반투명 기류 표현이며 화면 굴절은 사용하지 않는다.
- 먼 거리까지 늘리면 Niagara Fixed Bounds도 함께 넓혀야 한다.

## 미리보기

`L_HeatWave_Showcase`를 열고 `LS_HeatWave_Preview`를 Sequencer에서 재생한다. 60fps 기준 공격은 0프레임, 피격은 36프레임에 시작한다. **45프레임**에서 함께 볼 수 있다. 맵만 열면 1회 피격이 이미 끝날 수 있으므로 시퀀스를 이용한다.

Niagara 컴파일, 에디터 외형, 저장된 시퀀스 참조를 확인했다. PIE·전투 테스트는 하지 않았다.

제작 스크립트: `create_heat_wave_vfx.py`. 기존 `create_water_gun_vfx.py`와 `UEWaterVFXEditorLibrary`를 재사용하며 기존 HeatWave 에셋이 있으면 덮어쓰지 않는다. 런타임 C++ 변경은 없다.
