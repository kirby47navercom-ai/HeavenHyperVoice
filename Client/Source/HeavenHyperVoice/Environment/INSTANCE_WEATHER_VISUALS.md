# 인스턴스의 비·눈과 표면 표현

## 실행 흐름과 권위

main: `InstanceWeather` 서버 계산 → `S_InstanceWeather` 패킷 →
`UEFieldServerBridgeComponent` → `UEInstanceWeatherPresentationComponent` →
`BP_InstanceWeatherDirector` → Niagara/표면 데칼.

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
| `/Game/VFX/Weather/BP_InstanceWeatherDirector` | 다섯 Niagara, 표면 머티리얼, MPC 참조와 조절값 |
| `/Game/VFX/Weather/BP_WeatherExclusionVolume` | 맵에서 크기·위치를 직접 바꿀 수 있는 실내 차단 영역 |
| `/Game/VFX/Weather/NS_Weather_*` | Rain, Snow, Splash, Ripple, SnowPuff의 편집 가능한 이미터 스택 |
| `/Game/VFX/Weather/Materials/M_Weather_*` | 입자 모양·색·투명도와 젖음/눈 덮임 머티리얼 그래프 |
| `/Game/VFX/Weather/MPC_InstanceWeather` | Wetness/Snow/Rain/Cloud/Fog/Wind를 다른 아트 머티리얼에서 읽는 접점 |
| `/Game/Blueprints/DA_ProjectAssets` | 자동 생성할 날씨 BP 참조 |

블루프린트 `OnWeatherVisualsUpdated` 이벤트에는 보간된 상태와 카메라의 실내 여부가 온다.
추가 음향이나 하늘 표현을 원할 때 여기 연결한다. 현재 안개 액터나 낮밤 순환을 새로 만들지 않는다.
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

따라서 빗방울마다 서버에 충돌 요청을 보내지 않는다. 각 클라이언트의 입자 위치는 다를 수 있지만
같은 방의 강수량·기온·적설량은 서버 상태를 공유한다. 입자는 피해나 이동 판정에 사용하지 않는다.

기본 충돌 채널은 Visibility다. **지붕, 바닥, 수면에 이 채널을 막는 쿼리 충돌이 있어야 한다.**
충돌이 없는 장식 지붕, 동굴과 특수 실내에는 `BP_WeatherExclusionVolume`을 배치한다.
상자와 교차하는 낙하 경로를 제외한다. 마법 방벽처럼 날씨만 가리고 이동은 허용할 때도 쓴다.
수면 태그는 실제 맞는 충돌 컴포넌트 또는 소유 액터에 넣어야 한다.

## 쌓임과 젖음 위치

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

자동 검사: `Heaven.Weather.PresentationAndExposure`.
비/눈 분배, 북쪽을 가로지르는 바람 보간, 소스 해제, 지붕 충돌, 수면 분기, 회전한 실내 상자,
예약 피격 제거를 검사한다. 실제 서버 입장·다른 사용자 동기화는 별도 실행 환경에서 확인해야 한다.

2026-09-27 확인: UE 5.8 Development Editor 빌드(main/Yang2), 공통 자동 검사(main/Yang2),
Yang2 전용 `Heaven.Weather.Yang2LocalSource`(물 보존·로컬 적설→연출 전달)가 통과했다.
Niagara 5개 컴파일과 양쪽 BP/데이터 에셋 참조 로드도 확인했다.
`verify_instance_weather.py`는 기존 맵을 저장하지 않고 별도 미리보기를 만든다.
이미지와 에셋 검사 기록은 `Client/Saved/Codex/Weather/`, 자동 검사 결과는
`Client/Saved/WeatherTests/`에 남는다. 원격 서버와 실제 플레이 접속은 이 검사에 포함되지 않는다.
