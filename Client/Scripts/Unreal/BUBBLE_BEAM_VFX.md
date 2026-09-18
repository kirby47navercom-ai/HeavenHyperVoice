# 거품광선 VFX

에셋 폴더: `/Game/VFX/Pokemon/BubbleBeam`

| 에셋 | 용도 |
|---|---|
| `NS_BubbleBeam_Projectile` | 제자리에서 뽀글거리는 거품과 작은 물입자. 컴포넌트를 이동하면 생성된 거품이 월드 공간에 남아 이동 궤적을 만든다. |
| `NS_BubbleBeam_Impact` | 거품이 사방으로 퍼지고 터지는 링, 물방울과 짧은 물보라. 1회 재생. |
| `BP_BubbleBeam_Projectile` / `BP_BubbleBeam_Impact` | 각 시스템을 담은 배치용 블루프린트. 피격 액터는 1.5초 뒤 제거. |
| `NE_BubbleBeam_Bubbles` / `NE_BubbleBeam_FineFoam` | 공격의 편집 가능한 일반 Niagara 에미터 원본. |
| `L_BubbleBeam_Showcase` / `LS_BubbleBeam_Preview` | 정지 공격, 이동 꼬리, 피격을 살펴보는 맵과 시퀀스. |

## 연결

- 투사체의 Niagara Component에 `NS_BubbleBeam_Projectile`를 넣거나 `BP_BubbleBeam_Projectile` 액터의 위치를 움직인다. VFX 자체에는 자동 전진이 없다. 멈춰 있으면 원점 부근에서 거품이 생겼다가 사라진다.
- 공격 에미터의 **Local Space는 꺼진 상태**로 유지한다. 생성된 입자는 이후의 액터 이동을 따라가지 않으므로 뒤에 꼬리가 남는다. 방향이 바뀌어도 지나온 경로를 따른다. 화면 굴절이나 고정 길이 빔은 사용하지 않는다.
- 기본 거품 수명은 0.38~0.72초, 작은 물입자는 0.30~0.55초다. 꼬리 길이는 이동 속도 × 입자 수명에 따라 달라진다. 아주 빠른 투사체에서 간격이 벌어지면 Spawn Rate를 늘린다.
- 적중/취소 시 `Deactivate`로 방출을 끊으면 남은 거품이 최대 0.72초 동안 사라진다. 꼬리까지 즉시 제거하려면 `Destroy Component` 또는 `Destroy Actor`한다. 액터를 즉시 파괴하면 그 액터의 Niagara 꼬리도 함께 사라진다.
- 서버 적중 판정에 맞춰 `NS_BubbleBeam_Impact`를 적중 위치에 `Spawn System at Location`으로 별도 생성하고 Auto Destroy를 켠다. 공격이 피격을 자동 생성하지 않는다. 이동·충돌·데미지는 호출하는 게임 로직에서 처리한다.
- 월드 공간 공격은 컴포넌트 Scale만으로 이미 생성된 입자까지 크기를 바꾸지 않는다. 거품 크기는 Initialize Particle의 Sprite Size, 생성 범위는 Shape Location의 Sphere Radius에서 조절한다.

## 편집 및 미리보기

공격의 Spawn Rate, Initialize Particle, Shape Location, Add Velocity, Gravity Force는 일반 Niagara 스택에서 수정할 수 있다. 낮은 초기 속도와 약한 위쪽 힘만 주어 발사체 자체가 이동하지 않을 때는 원점 근처에서 뽀글거린다.

피격은 `BurstingBubbles`, `WhiteFoam`, `BubblePops`, `WaterContact` 네 레이어다. `M_BubbleBeam_Bubble`의 Custom 노드에는 투명한 속, 푸른 테두리, 밝은 하이라이트와 수명 끝의 터짐 표현이 있다. 각 머티리얼의 Brightness로 밝기를 조절한다.

미리보기 맵에서 시퀀스를 **0프레임부터 재생**하면 위쪽 정지 효과와 아래쪽 이동 효과를 비교할 수 있다. 아래쪽 효과는 1초 동안 560cm 이동하고, 60프레임에서 피격이 시작한다. 월드 공간 궤적은 시간에 따라 축적되므로 타임라인을 한 번에 건너뛰면 이동 꼬리가 정확하게 재구성되지 않을 수 있다.

제작 스크립트는 `create_bubble_beam_vfx.py`다. `main()`으로 원본 에미터·공격 시스템·피격을 만든 뒤 `finish()`, `showcase()`를 실행한다. 기존 완성 에셋은 덮어쓰지 않는다. 기존 `UEWaterVFXEditorLibrary`에 언리얼 기본 에디터 함수를 사용하는 일반 에미터 추가 기능을 넣었다.
