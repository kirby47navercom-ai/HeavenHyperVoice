# 불꽃세례 VFX

에셋 폴더: `/Game/VFX/Pokemon/Ember`

| 에셋 | 사용 |
|---|---|
| `NS_Ember_Projectile` | 제자리에서 반복하는 작은 불덩이와 뒤로 흐르는 불씨. 자동 이동 없음. |
| `NS_Ember_Impact` | 적중 지점에서 한 번 퍼지고 사라지는 불꽃과 불씨. |
| `BP_Ember_Projectile` | 공격 Niagara를 담은 배치용 액터. |
| `BP_Ember_Impact` | 피격 Niagara를 담은 배치용 액터. 게임에서 1.5초 뒤 제거. |
| `L_Ember_Showcase` | 두 효과의 형태를 고정 시점에서 비교하는 미리보기 맵. |

## 사용

- 공격 방향은 로컬 **+X**, 꼬리는 **-X**다. 투사체 Niagara Component에 공격 시스템을 지정하거나 `Spawn System Attached`로 붙인다. 적중·취소 시 반복 효과를 Deactivate하거나 소유 액터와 함께 제거한다.
- 피격은 `Spawn System at Location`으로 적중 위치에 생성하고 Auto Destroy를 켠다.
- 이 에셋에는 이동·충돌·데미지 판정이 없다. 서버에서 판정한 결과에 따라 클라이언트에서 재생하는 용도다. 전투 연결은 하지 않았다.
- Showcase의 컴포넌트는 Desired Age로 멈춰 있다. 배치용 BP/NS는 정상 재생한다. 시간 흐름은 Niagara 에디터 타임라인에서 확인할 수 있다.

## 편집

각 Niagara 시스템은 편집 가능한 Lightweight emitter 4개로 구성된다. Initialize Particle에서 크기·수명, Spawn에서 개수, Add Velocity에서 퍼지는 속도, Gravity Force에서 불씨의 상승·하강을 조절한다.

공격: LeadingFlames / TrailingFlames / FlyingEmbers / WarmGlow.

피격: FlamePetals / HotFlash / ScatteredEmbers / FadingGlow.

`Materials/M_Ember_*`의 Custom 노드에서 불꽃 형태·색상, `Brightness`에서 발광 세기를 수정한다. 영상의 작은 노란 불꽃과 주황색 불씨를 참고한 절차적 머티리얼이다.

제작 스크립트 `create_ember_vfx.py`는 같은 폴더의 `create_water_gun_vfx.py`와 기존 에디터 전용 `UEWaterVFXEditorLibrary`를 재사용한다. 완성 에셋은 직접 편집하며, 재실행 시 기존 시스템을 덮어쓰지 않는다. 런타임에 제작 스크립트나 에디터 모듈은 필요하지 않다.
