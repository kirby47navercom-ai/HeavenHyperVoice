# 인스턴스의 비·눈과 표면 표현

## 실행 흐름과 권위

main: `InstanceWeather` 서버 계산 → `S_InstanceWeather` 패킷 →
`UEFieldServerBridgeComponent` → `UEInstanceWeatherPresentationComponent` →
`BP_InstanceWeatherDirector` → Niagara/공용 지면 함수/표면 데칼.

Yang2: 기존 브릿지가 **서버와 같은 InstanceWeather 계산을 로컬에서 실행**하고 같은
이벤트로 보낸다. 이후 연출 흐름은 main과 동일하다. 이 폴더에는 로그인 우회, 로컬 권위
판정이나 서버 날씨 생성 코드를 넣지 않는다. Yang2의 브릿지/빌드 매크로를 main에 보내면 안 된다.

인스턴스에 플레이어가 붙으면 `UEFieldClientSubsystem::AttachPlayerCharacter()`에서
`DA_ProjectAssets.InstanceWeatherDirectorClass`를 읽어 BP를 한 번 생성한다.
로그인 화면으로 돌아갈 때는 제거한다. 레벨 이동 시 기존 월드와 함께 사라진다.
서버 날씨가 아직 없으면 비·눈을 만들지 않는다. 기상 연출은 Dedicated Server에서 실행하지 않는다.

## 무엇을 어디서 수정하는가

| 위치 | 담당 |
|---|---|
| `UEInstanceWeatherPresentationComponent.h/.cpp` | mm/h, m/s, 적설 m를 0~1 연출 강도로 바꾸고 부드럽게 보간 |
| `UEInstanceWeatherDirector.h/.cpp` | 카메라 주변 낙하 시작점, 충돌 위치, 물 튀김 예약, 표면 타일 관리 |
| `UEWeatherExclusionVolume.h/.cpp` | 회전·크기를 포함한 실내 제외 상자와 선분 교차 |
| `/Game/VFX/Weather/BP_InstanceWeatherDirector` | 일곱 Niagara, 표면/피격 머티리얼, MPC 참조와 조절값 |
| `/Game/VFX/Weather/BP_WeatherExclusionVolume` | 맵에서 크기·위치를 직접 바꿀 수 있는 실내 차단 영역 |
| `/Game/VFX/Weather/NS_Weather_*` | Rain, Snow, Splash, WallSplash, Ripple, SnowPuff, SnowChunks의 편집 가능한 이미터 스택 |
| `/Game/VFX/Weather/Materials/M_Weather_*` | 입자 모양·색·투명도와 젖음/눈 덮임 머티리얼 그래프 |
| `/Game/VFX/Weather/MPC_InstanceWeather` | Wetness/Snow/Rain/Cloud/Fog/Wind를 다른 아트 머티리얼에서 읽는 접점 |
| `/Game/VFX/Weather/Materials/MF_WeatherSurface` | 기존 지면 색·거칠기에 젖음/적설/웅덩이/실내 노출을 합성하는 공용 함수 |
| `/Game/InstanceMap/Plain/Landscape/M_Landscape_GrassSoil` | 현재 인스턴스 Landscape에 공용 함수를 실제 연결한 지면 |
| `/Game/Blueprints/DA_ProjectAssets` | 자동 생성할 날씨 BP 참조 |

블루프린트 `OnWeatherVisualsUpdated` 이벤트에는 보간된 상태와 카메라의 실내 여부가 온다.
추가 음향은 이 이벤트에 연결한다. 낮밤·구름·안개·조석·먼지는 별도 `BP_EnvironmentScene`에서 처리한다.
최신 전체 범위는 `Server/InstanceServer/EARTH_ENVIRONMENT_GUIDE.md`에 설명했다.
`Fog`는 습도와 구름으로 추정한 **연출용 값**이며 실제 안개 물리량이 아니다.
전투/출현 판정은 이 보간된 값이 아닌 서버 상태로 처리한다.

## 낙하와 피격 위치

1. 카메라 주변 반경 안에서 면적에 균일하게 시작점을 뽑는다.
2. 시작점에서 위로 충돌 검사를 해 지붕 아래에서 생성되는 것을 막는다.
3. 바람 벡터와 낙하 속도로 경로를 정하고 첫 충돌점까지 검사한다.
4. C++이 구한 실제 수명과 속도를 `User.FallLifetime`, `User.FallVelocity`로 전달한다.
   가벼운 Niagara 이미터 한 입자가 그 경로를 따라간다.
5. 도착 시 경로를 다시 검사해서 표면 법선에 맞춰 Splash/SnowPuff를 만든다.
   액터 또는 충돌 컴포넌트에 `WeatherWater` 태그가 있으면 Splash 대신 평평한 Ripple을 쓴다.
   벽(법선 Z < 0.55)에는 작은 `WallSplash`를 쓴다. 비가 맞은 일반 표면에는 붙는 젖은 자국을
   기본 4초 동안 남기고 서서히 지운다. 동시에 최대 32개, BP에서 최대 64개까지 조절한다.
   눈이 바닥에 닿으면 눈가루와 작은 메시 눈 덩어리 3개가 짧게 튄다. 벽과 수면에는 덩어리를 만들지 않는다.

따라서 빗방울마다 서버에 충돌 요청을 보내지 않는다. 각 클라이언트의 입자 위치는 다를 수 있지만
같은 방의 강수량·기온·적설량은 서버 상태를 공유한다. 입자는 피해나 이동 판정에 사용하지 않는다.

기본 충돌 채널은 Visibility다. **지붕, 바닥, 수면에 이 채널을 막는 쿼리 충돌이 있어야 한다.**
충돌이 없는 장식 지붕, 동굴과 특수 실내에는 `BP_WeatherExclusionVolume`을 배치한다.
상자와 교차하는 낙하 경로를 제외한다. 마법 방벽처럼 날씨만 가리고 이동은 허용할 때도 쓴다.
수면 태그는 실제 맞는 충돌 컴포넌트 또는 소유 액터에 넣어야 한다.

## 쌓임과 젖음 위치

현재 `Plain` 인스턴스의 Landscape는 `MF_WeatherSurface`를 사용한다. 기존 Base Color와
Roughness 계산을 함수 입력으로 연결하고 출력만 원래 핀으로 돌려준다. 기존 노멀/레이어는 유지한다.
날씨 값이 0이면 원래 색과 거칠기가 그대로 나온다. 젖으면 어두워지고 매끈해지며,
눈은 위를 향한 표면에 쌓인 색으로 바뀐다. `VertexNormalWS`를 사용해 작은 노멀맵 요철이 아니라
실제 표면 방향으로 적설을 제한한다. 벽과 천장에는 눈이 거의 붙지 않는다.

웅덩이는 `WeatherPuddleMaskTexture`의 낮은 높이 부분에만 표시한다. 현재 기본값은 프로젝트에
있던 `rocky_trail_02_disp_4k`다. 흙의 낮은 홈처럼 보일 부분을 지정하는 아트 마스크이며 실제 유체 수위가 아니다.
MI에서 `WeatherPuddleLevel`을 낮추면 고이는 영역이 줄고, `WeatherPuddleWorldSize`로 반복 크기를
바꾼다. `WeatherUseVertexPuddleMask=1`이면 메시의 버텍스 색 B가 허용 영역이 된다.
현재 Landscape에는 텍스처 마스크를 사용한다. 별도 바닥 메시에서 버텍스 페인트를 사용할 수 있다.
`WeatherExposure=0`으로 그 머티리얼의 날씨를 끌 수 있다. `WeatherMaterialEnabled` 파라미터가 있는
표면에는 대체 데칼을 중복으로 붙이지 않는다.

다른 표면에 함수를 연결하려면 기존 색/거칠기, 월드 법선, Exposure, PuddleMask를 입력한다.
그 머티리얼에도 `WeatherMaterialEnabled` 스칼라 파라미터를 추가해 Exposure에 곱한다.
지붕 아래 바닥은 아래의 제외 BP나 별도 Exposure 마스크가 필요하다. 머티리얼 함수 자체가
위쪽으로 충돌 검사를 하지는 않는다.

공용 함수가 없는 기존 표면은 다음의 대체 데칼 방식으로 표현한다.
서버는 방 평균 `ground_wetness`, `snow_depth_m`를 보낸다. 클라이언트는 주변 격자의 각 위치에서
하늘 아래 가장 먼저 맞는 표면을 찾는다. 위를 향한 표면에만 얇은 데칼을 붙여 젖음/눈 덮임을
표시한다. 지붕이 있으면 지붕 위에 나타나고 아래 바닥에는 나타나지 않는다. 수면과 제외 상자는
눈 덮임 대상에서 빠진다. 적설 0.1m에서 최대 덮임이 기본값이며 BP에서 조절한다.

이 구현의 쌓임은 **표면 색·거칠기·덮임 표현**이다. 눈이 실제로 지형 높이를 올리거나 발자국을
남기지는 않는다. 낙하 입자 개수를 더해서 적설량을 만들지도 않는다. 서버 적설량이 줄면
표면의 눈 덮임도 줄어든다. 지점별 영구 적설/고임 수위/흐르는 물은 아직 별도 상태가 없다.

주변 11×11개의 타일을 재사용하고 0.1초마다 12개씩 위치를 갱신한다. 얇은 데칼이라 벽과
아래층으로 번지는 것을 줄이지만, 좁은 처마/복잡한 다층 구조는 타일 간격보다 작을 수 있다.
그런 맵은 타일 크기 조절과 제외 상자로 보완하거나 노출 마스크를 쓰는 전용 지면 머티리얼로
교체한다. 빠르게 움직이는 물체는 낙하 중 경로를 매 프레임 재계산하지 않아 시각 오차가 생길 수 있다.

## 나중에 실내·동굴·수면을 만들 때

현재 Plain에는 실내·동굴·수면을 새로 만들거나 차단 상자를 임의로 배치하지 않았다.
실내가 생기면 `/Game/VFX/Weather/BP_WeatherExclusionVolume`을 끌어 놓고 `WeatherBounds`를
실내 바닥까지 포함하도록 맞춘다. 지붕 윗면은 상자 밖에 두면 지붕에는 눈이 쌓이고 실내 바닥은 마른 채로 남는다.
회전/비균일 스케일도 지원한다. 플레이어 이동 충돌은 없고 날씨만 차단한다.

입자 경로는 모든 제외 상자를 검사한다. 지면 함수에는 플레이어와 가까운 **최대 8개 상자**를
1초 간격으로 전달한다. 상자 안에서는 젖음·적설·웅덩이가 꺼진다. 매우 넓고 많은 실내가
동시에 보이는 맵은 노출 마스크/RT가 필요하다. 실내가 생긴 뒤 실제 출입구에서의 모양 확인은 별도 작업이다.

수면을 추가하면 수면 충돌 컴포넌트/액터에 `WeatherWater` 태그를 넣고 Visibility 쿼리를 막게 한다.
일반 땅은 그대로 두고 수면에만 태그를 넣는다. 이후 비는 물보라 대신 파문을 만들고,
대체 적설 데칼은 그 표면을 제외한다. 수면 머티리얼에는 지면용 함수를 연결하지 않는다.

## 비용과 조절

- `MaxDropsPerSecond`: 기본 100, 최대 200개의 시각 샘플/초. 실제 mm/h의 물방울 개수가 아니다.
- 프레임당 생성 예산 16개, 예약 충돌 최대 256개. 프레임 지연 후 한꺼번에 폭발하지 않게 제한한다.
- 비의 낙하 속도 2200cm/s, 눈 180cm/s. 바람은 시각용으로 제한한다.
- `Radius`/`FallHeight`: 주변 범위와 생성 높이.
- `SurfaceTileSize`/`SurfaceGridRadius`: 표면 샘플 간격과 영역. 타일 수는 최대 169개.
- 비·눈 온도 분배는 서버와 같은 -1~1°C다. 0°C는 비와 눈이 반씩 섞인다.
- 기존 머티리얼을 대량 교체하지 않는다. 표면이 데칼을 받지 않으면 해당 메시의 Receives Decals를 켠다.

## 제작과 확인

`Client/Scripts/Unreal/create_instance_weather.py`를 5.8 에디터 Python에서 실행하면 에셋을 만든다.
기존 에셋은 유지하므로 아티스트 편집을 덮어쓰지 않는다. BP 참조는 다시 연결한다.
새로 재제작할 경우 대상 에셋만 에디터에서 삭제한 후 실행한다.

`extend_instance_weather.py`는 기본 에셋 제작 뒤 실행하는 지면/피격 확장 제작 스크립트다.
MF, 벽 물보라, 눈 덩어리, 젖은 피격 머티리얼을 생성하고 Plain 지면과 BP에 연결한다.
실행할 때 UE 5.8의 렌더링이 필요하다(`-AllowCommandletRendering`, `-NullRHI` 제외).

자동 검사: `Heaven.Weather.PresentationAndExposure`.
비/눈 분배, 북쪽을 가로지르는 바람 보간, 소스 해제, 지붕 충돌, 수면 분기, 회전한 실내 상자,
예약 피격 제거를 검사한다. 실제 서버 입장·다른 사용자 동기화는 별도 실행 환경에서 확인해야 한다.

2026-09-27 확인: UE 5.8 Development Editor 빌드(main/Yang2), 공통 자동 검사(main/Yang2),
Yang2 전용 `Heaven.Weather.Yang2LocalSource`(물 보존·로컬 적설→연출 전달)가 통과했다.
Niagara 5개 컴파일과 양쪽 BP/데이터 에셋 참조 로드도 확인했다.
`verify_instance_weather.py`는 기존 맵을 저장하지 않고 별도 미리보기를 만든다.
이미지와 에셋 검사 기록은 `Client/Saved/Codex/Weather/`, 자동 검사 결과는
`Client/Saved/WeatherTests/`에 남는다. 원격 서버와 실제 플레이 접속은 이 검사에 포함되지 않는다.

확장분 확인: 양쪽 Editor 빌드, Landscape 충돌→공용 지면 감지, 회전/스케일된 실내의 머티리얼 좌표,
젖은 자국의 표면 부착/개수 제한/해제 검사를 통과했다. Niagara 7개와 BP 참조를 로드하고,
별도 편집기 장면에서 공용 함수의 웅덩이 마스크/적설/실내 차단을 렌더로 확인했다.
`surface-wet.png`, `surface-snow.png`의 격자 마스크는 확인용 임시 머티리얼이다.
실제 Plain은 기존 높이 텍스처 마스크를 사용한다. 확장 검사 로그는 `Saved/weather-detail-*.log`,
결과는 `Saved/WeatherDetailTests/`에 있다. 요청에 따라 실게임 접속·멀티플레이·성능 검증(5번)은 제외했다.
