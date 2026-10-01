# 독립 날씨 에셋 라이브러리

게임 맵에 투입하지 않은 편집용 에셋이에요. 기존 맵, 지면 머티리얼, 서버 계산,
프로젝트 DA, 날씨 Director의 연결은 바꾸지 않아요. main과 Yang2에서 같은 에셋을 써요.

## 콘텐츠 위치

```text
/Game/VFX/Weather/
  Materials/Surfaces/
    MF_SurfaceWetness
    M_EnvironmentSurface
    MI_Surface_DrySoil / WetSoil / PuddleSoil / SnowSoil / ThawingSoil
    MI_Surface_DryStone / WetStone / FrostStone / FrozenPuddle
    MI_Surface_DrySand / WetSand
  Materials/Atmosphere/
    M_Atmosphere_GroundMist / SandDrift / SnowDrift
  Niagara/Atmosphere/
    NS_Atmosphere_GroundMist / SandDrift / SnowDrift
  Blueprints/Atmosphere/
    BP_Atmosphere_GroundMist / SandDrift / SnowDrift
```

기존 흙·돌·모래 텍스처와 얼음 함수를 참조해요. 원본 에셋을 복사하거나 이동하지 않아요.
원본 이미지, 로그, 사진, 임시 레벨을 Content에 넣지 않아요.

## 지면 머티리얼

하나의 부모 머티리얼에서 11개 MI의 값을 바꿔요. 기본값은 수동 입력이에요.
서버의 날씨를 자동 구독하거나 새 계산을 수행하지 않아요.

| 값 | 용도 |
|---|---|
| SurfaceColor / SurfaceNormal / SurfaceRoughness / SurfaceHeight | 원래 지면 무늬와 미세 요철, 거칠기, 웅덩이 허용 높이 마스크예요. MI에서 교체해요. |
| CoverageTexture / SnowBreakup | 눈이 고르게 덮이거나 군데군데 남는 무늬를 조절해요. |
| SnowCrystalTexture | 기존 서리 결정 텍스처로 눈의 불규칙한 미세 요철을 만들어요. |
| Tiling / SurfaceTint / NormalStrength / RoughnessScale | 원본 무늬의 반복 크기, 색, 요철, 거칠기예요. |
| SoilNormalEncoded / UseEncodedSoilNormal | 기존 선형 GL 흙 노멀의 디코딩을 선택해요. 돌·모래는 SurfaceNormal의 일반 노멀맵을 사용해요. 원본 압축 설정은 바꾸지 않아요. |
| Wetness | 0~1의 표면 젖음이에요. 색과 거칠기가 함께 변해요. |
| WetDarkening / WetRoughness | 표면 종류마다 다른 젖은 색과 반사 정도예요. |
| PuddleStrength / PuddleLevel / PuddleRoughness | 높이 마스크에서 낮은 곳에 얇은 물막 반사를 만들어요. 실제 수위나 물 흐름 계산은 아니에요. |
| IceCoverage / FrostAmount / IceTint / IceCellSizeCm | 얼음 덮임, 서리 비중, 얼음 색, 균열 무늬 크기예요. 기존 MF_EnvironmentIce를 재사용해요. |
| IceInPuddles | 1이면 높이 마스크의 낮은 홈만 얼어요. 물이 얼어 Wetness가 줄어도 결빙 허용 마스크는 사라지지 않아요. |
| SnowCoverage / SnowTint | 위쪽 표면에 남는 눈의 덮임과 색이에요. |
| SurfaceMask / Exposure | 효과 허용 정도예요. 함수의 Mask 입력에는 나중에 위치별 텍스처를 연결할 수 있어요. |
| WaterCoatRoughness | 물막/얼음 코팅의 반사 거칠기예요. |

젖음은 원본 무늬를 유지해요. 웅덩이에서는 요철을 완화하고, 눈은 결정의 작은 요철을 써요.
얼음/서리는 기존 균열 텍스처를 사용하며 눈은 마지막에 덮어요. 프리셋의 수치는
시각 샘플이에요. ThawingSoil이 실제 시간에 따라 녹거나 물을 생성하지는 않아요.
실제 지면 높이/충돌/눈 두께/발자국/서버의 위치별 상태는 만들지 않아요.

나중에 BP에서 동적 MI의 Wetness/SnowCoverage/IceCoverage 값을 넣을 수 있어요.
공용 함수는 기존 색/거칠기와 마스크를 받아 다른 부모 머티리얼에 연결할 수 있어요.
IceCoverage는 이 라이브러리에서 0~1의 수동 덮임으로 사용해요.

## 대기 효과

GroundMist는 낮게 흐르는 얇은 안개, SandDrift는 낮은 모래 먼지와 작은 알갱이,
SnowDrift는 바람에 날리는 눈가루와 작은 입자예요. BP에 시스템을 지정해 두었어요.
모두 Niagara의 방출량/영역/속도/수명/크기를 직접 편집할 수 있어요.
액터의 로컬 +X가 흐르는 방향이에요. 회전으로 방향을, 스케일로 발생 영역을 조절해요.
스케일이 입자 크기까지 늘려 큰 판이 되는 것은 막았어요.

Tint/Density/WispTexture/IntersectionFadeCm은 각 머티리얼에서 조절해요.
장면 조명을 받으며 흰 발광은 사용하지 않아요. 지면 교차는 Depth Fade로 부드럽게 지워요.
지형 추종/실내 차단/동적 충돌/날씨에 따른 활성화는 아직 연결하지 않았어요.
평평한 구역의 시각 샘플이므로 복잡한 지형은 발생 영역을 나눠 배치해야 해요.

## 제작과 사진

`Client/Scripts/Unreal/create_weather_asset_library.py`를 Unreal 에디터 Python으로 실행해요.
이미 있는 에셋은 보존하고 `--refresh`를 명시할 때만 이 라이브러리 그래프를 갱신해요.
셰이더 소스는 같은 Scripts/Unreal/Shaders 폴더에서 상대 위치로 읽어 에셋에 저장해요.
게임 실행 때 Python/ush/외부 이미지/PC의 파일 경로를 읽지 않아요.
프로젝트 내부 /Game 참조는 저장된 에셋 참조로 사용해요.

`preview_weather_asset_library.py`는 저장하지 않는 /Temp 레벨에서 실제 uasset을 렌더해요.
사진과 결과 목록은 현재 프로젝트의 Saved/Codex/WeatherAssetLibrary에 저장해요.
PIE, 로그인, 서버, 실제 게임 맵은 실행하거나 저장하지 않아요.
