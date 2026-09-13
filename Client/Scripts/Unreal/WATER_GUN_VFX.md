# 물대포 VFX

에셋 폴더: `/Game/VFX/Pokemon/WaterGun`

| 에셋 | 사용 |
|---|---|
| `NS_WaterGun_Projectile` | 제자리 반복 공격 효과. 중심은 이동하지 않고 뒤쪽으로 물이 흐른다. |
| `NS_WaterGun_Impact` | 피격 위치에서 한 번 터지고 사라지는 물보라. |
| `BP_WaterGun_Projectile` | 공격 Niagara를 담은 배치용 액터. 이동·충돌·데미지 로직 없음. |
| `BP_WaterGun_Impact` | 피격 Niagara를 담은 액터. 게임에서 1.5초 뒤 제거됨. |
| `L_WaterGun_Showcase` | 두 효과를 고정된 재생 시점에서 비교하는 미리보기 맵. |

## 연결

- 공격: 실제 이동 액터의 Niagara Component에 `NS_WaterGun_Projectile`을 지정하거나 `Spawn System Attached`로 붙인다. 전방은 로컬 **+X**, 물 꼬리는 **-X**다. 반복 효과이므로 적중·취소 때 Deactivate하거나 소유 투사체와 함께 제거한다.
- 피격: 적중 위치에서 `Spawn System at Location`으로 `NS_WaterGun_Impact`를 한 번 실행하고 Auto Destroy를 켠다. 부착하면 대상과 함께 움직이므로 보통 월드 위치에 생성한다.
- 서버 권위의 이동·충돌·데미지 판정은 기존 전투 코드에서 처리한다. 이 에셋은 시각 효과만 담당한다. 현재 공격 스킬이나 서버에 자동 연결하지 않았다.

## 편집

Niagara 시스템마다 4개 Lightweight emitter가 있다. 각 레이어의 Initialize Particle에서 수명과 크기, Spawn에서 입자 수, Add Velocity에서 퍼지는 속도를 편집한다. Transform Scale로 효과 전체 크기를 조절할 수 있다. 공격 시스템에는 액터를 이동시키는 기능이 없다. Lightweight 입자는 로컬 공간에서 동작한다.

공격: WaterCore / SpiralSheets / FineSpray / LeadingWater.
피격: SplashPetals / ImpactFlash / RadialDroplets / ExpandingRing.

`Materials/M_WaterGun_*`의 색상 계산과 `Brightness`로 파란색 물과 흰색 하이라이트를 수정한다. 텍스처 추출 없이 절차적 셰이더로 만들었다. 참조 영상은 타이밍·형태 확인에만 사용했다.

미리보기 맵의 Niagara Component는 Desired Age로 멈춰 있다. 실제 효과의 시간 흐름은 Niagara 에디터 타임라인에서 확인한다. 배치용 BP와 NS 자체는 정상 재생 설정이다.

제작: `create_water_gun_vfx.py` + 에디터 전용 `UEWaterVFXEditorLibrary`. 런타임에서 제작 스크립트나 에디터 모듈은 필요하지 않다. 기존 에셋이 있으면 스크립트는 덮어쓰지 않는다.

확인: C++ 빌드, Niagara 컴파일, 에디터에서 고정 시점 외형 확인. PIE·전투 테스트 및 커밋·푸시는 하지 않음.
