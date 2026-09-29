# 지구과학 엔진: 시간·날씨·지표·해안·사막

## 현재 구현 범위

한 인스턴스 방의 평균 환경을 계산하는 게임용 모델이다. 방마다 계산 상태가 독립되어
같은 종류의 던전도 입장한 방에 따라 초기 기온·습도·구름이 조금 달라진다.
공통 기후는 데이터 에셋으로 편집하고 서버 설정으로 내보낸다.

| 대상 | 서버 계산 | 클라이언트 표현 |
|---|---|---|
| 시간·낮밤 | 누적 게임 시간, 자정 순환, 위도에 따른 태양 고도·방위 | 해·달 방향, 황혼 색, 직사광·환경광 밝기 |
| 계절·고도 | 연중 주기, 남북반구 반대 계절, 기준 고도에 따른 기온 변화 | 같은 기온이 강수 형태·눈·젖음에 연결. 나뭇잎 색은 별도 아트 작업 |
| 대기 | 저층·상층 온도, 수증기, 포화·응결, 기압 변화, 바람 | 구름 머티리얼 밀도, 습도 기반 안개, 비·눈 강도·방향 |
| 물순환 | 증발, 층간 혼합, 강수, 눈·얼음, 융해, 침투, 배수 | 바닥 젖음·눈 덮임, 웅덩이 마스크, 표면 피격 |
| 지표 | 공기와 별도의 열관성, 물·토양수·얼음 저장량 | 상태를 BP에서 읽을 수 있음. 얼음 전용 외형·미끄러짐은 없음 |
| 해안 | 주기적 상대 수위, 바람에 반응하는 파고 | UE Water Ocean 수위 + 네 개의 Gerstner 파도 |
| 사막 | 모래 가용량 × 건조도 × 풍속, 눈·얼음 덮임의 억제 | 카메라 주변 먼지 Niagara, 황갈색 안개, 서서히 증가/감소 |
| 전환 | 열·파도·먼지가 각 반응 시간으로 목표값을 추종 | 패킷 사이 지수 보간. 자정·연말·북쪽은 짧은 방향으로 보간 |

비·눈의 개별 낙하 위치는 서버가 복제하지 않는다. 클라이언트에서 카메라 주변을 샘플링하고
지형 트레이스로 맞은 위치에 물보라·파문·눈가루·눈 덩어리를 배치한다. 지붕 트레이스와
`BP_WeatherExclusionVolume`은 기존 비·눈 차단 기능이다. 새 건물에 충돌/차단 영역을 배치하면
사용할 수 있다. 현재는 실내 맵을 만들지 않았다. 모래폭풍의 실내 차단은 별도 연결 대상이다.

## main과 Yang2가 다른 부분

```text
main: DA → 서버 ini → Room.weather → WeatherState 패킷 → 브릿지 이벤트
Yang2: 레벨의 DA → Yang2 전용 로컬 InstanceWeather → 같은 브릿지 이벤트
                                                     ↓
                Presentation 보간 → 강수 Director + EnvironmentScene
```

main 클라이언트에는 날씨 계산기를 넣지 않는다. `UEEnvironmentScene`은 받은 상태로
빛·물·입자를 표시한다. 물 표면 높이는 연출이며 서버의 이동·수영 판정을 변경하지 않는다.
Yang2만 동일한 서버 계산 소스를 로컬에서 컴파일한다. 전용 파일/매크로/로그인 우회는 main에 보내지 않는다.
서버가 없는 main에서는 임의 날씨를 만들지 않으며 원래 레벨의 빛과 바다가 유지된다.

## 파일별 역할과 계산 순서

서버 파일은 `src/EarthScience/`에 모았다. 긴 하나의 cpp 대신 다음 순서로 호출한다.

1. `InstanceWeather.cpp`: 방 초기값과 고정 계산 시간 누적만 관리한다. 현실 dt × 배속을
   모아서 게임 시간 10초마다 계산한다. 1회 최대 4096단계 뒤 남은 시간은 다음 호출에 유지한다.
2. `EnvironmentClock.cpp`: 누적 초를 하루/1년 주기로 바꾼다. 위도와 태양 적위로 태양 방향을 얻는다.
3. `AtmosphereCycle.cpp`: 계절·고도·태양·구름을 목표 기온에 반영하고 지표 → 저층 → 상층 순으로
   서서히 반응시킨다. 기압 전선과 돌풍은 게임용 주기 근사다.
4. `SurfaceWaterCycle.cpp`: 증발 → 층간 혼합 → 포화 조절 → 강수 → 눈/얼음 융해 또는 동결 →
   침투/배수 순서로 물을 이동한다. 배수량도 따로 합산해 질량 보존 오차를 확인한다.
5. `CoastalEnvironment.cpp`: `진폭 × sin(2π × 시간/주기 + 위상)`으로 상대 수위를 만든다.
   풍속이 커질수록 파고가 커지며 `WaveResponseSeconds` 동안 서서히 따라간다.
6. `DesertEnvironment.cpp`: 젖은 땅은 먼지가 잘 날리지 않는다. 토양 건조도와 바람의 임계값으로
   목표 농도를 얻고 `DustResponseSeconds`로 강도를 변화시킨다.
7. `WeatherSnapshot.cpp`: 내부 저장량을 기온·습도·강수량·적설·수위 등의 전송 상태로 바꾼다.

`WeatherMath.h`에는 포화 수증기압, 두 저장소 사이 물 이동, 지수 반응 같은 작은 공통 계산만 있다.
`EnvironmentProfile.h/.cpp`는 설정과 유효 범위, `EnvironmentState.h`는 결과를 담는다.
`EnvironmentConfig.cpp`는 알 수 없는 키·중복 키·NaN·범위를 벗어난 설정을 서버 기동 전에 거부한다.

클라이언트 파일은 `Client/Source/HeavenHyperVoice/Environment/`에 있다.

| 파일 | 담당 |
|---|---|
| `UEEnvironmentProfile.h` | 편집 가능한 UDataAsset. 각 수치의 단위/용도를 한글 주석으로 설명 |
| `UEEnvironmentState.h/.cpp` | 서버 결과 USTRUCT와 순환 값 보간 |
| `UEEnvironmentScene.h/.cpp` | BP의 액터·에셋 연결, 시작 시 원상태 저장, 종료/소스 해제 시 복구 |
| `UEEnvironmentLighting.cpp` | 해/달/환경광/구름/안개 변화 |
| `UEEnvironmentOcean.cpp` + `UEEnvironmentWaves.h` | 상대 수위 적용과 Water 기본 파도 생성 |
| `UEEnvironmentDust.cpp` | 카메라 주변 먼지, 바람 방향, MPC 값 |
| 기존 `UEInstanceWeatherPresentationComponent.*` | 서버 수치를 연출 강도로 바꾸고 매 프레임 보간 |
| 기존 `UEInstanceWeatherDirector.*` | 비·눈 낙하, 충돌 피격, 지면 젖음/적설 |

서버는 `std::map`, `std::set`, 표준 수학/난수/파일 입력을 사용한다.
UE 객체·에셋은 GC와 블루프린트 연동을 위해 `TObjectPtr`, `TArray`, `UPROPERTY`를 사용한다.
물리 계수·초기 기본값까지 전부 없앤 코드는 아니다. 맵 좌표나 런타임 콘텐츠 경로는 새 계산 코드에 넣지 않았다.
물리 근사 계수는 계산 파일에, 지역별 조절값은 DA에, 실제 액터/외형 참조는 BP와 레벨에 있다.

## 에셋과 조절 방법

기존 `/Game/VFX/Weather` 안에 아래 에셋을 저장했다.

- `DA_Environment_Temperate`: 기존 Plain용 온대 기본값.
- `DA_Environment_Coast`: 조석 진폭 1.5 m, 파고 상한 1.2 m, 완만한 온도 반응.
- `DA_Environment_Desert`: 평균 32°C, 낮은 초기 수분, 모래 가용량 1, 강한 바람과 큰 일교차.
- `BP_EnvironmentScene`: C++ 연출 액터의 자식 BP. Dust/MPC 참조가 저장되어 있다.
- `NS_Environment_Dust`, `M_Environment_Dust`: 편집 가능한 입자와 머티리얼 그래프.
- `M_Environment_Sand`: 공용 젖음/적설 함수를 연결한 예제 지면.
- `L_Environment_Coast`, `L_Environment_Desert`: 연결 상태를 확인할 수 있는 작은 예제 레벨.
  완성된 해안/사막 던전은 아니며 새 포탈·인스턴스 ID·서버 이동 메시까지 연결하지 않는다.

기존 `/Game/InstanceMap/Plain/Plain`에는 Scene 하나를 놓고 기존 Sun/Sky/Fog/Clouds를 연결했다.
Moon을 하나 추가했으며, 고정 낮 하늘 돔은 실행 중 환경이 활성화될 때만 숨긴다.
원래 지형·포탈·스폰 배치를 이동하지 않았다. Goldenrod 도시 바다는 이번 변경에서 교체하지 않는다.

레벨에 Scene을 하나만 배치하고 `Sun`, `Moon`, `Sky`, `Fog`, `Clouds`, 필요한 경우 `Ocean`을
스포이트로 연결한다. 구름의 `CloudDensityParameter`는 머티리얼에 실제 존재하는 이름이어야 한다.
현재 예제는 엔진 기본 구름의 `Cloud_GlobalDensity`를 사용한다. 물은 `WaterBodyOcean`을 연결한다.
`WaveLengthCm`, `WaveSteepness`, 낮밤 색·밝기·안개 범위는 BP/레벨에서 조절할 수 있다.

서버 설정을 바꾸는 순서는 다음과 같다.

1. 환경 DA를 수정하고 저장한다.
2. 에디터 Python에서 `Client/Scripts/Unreal/export_environment_profiles.py`를 실행한다.
3. 생성된 `Server/InstanceServer/config/environment/DA_Environment_*.ini`를 서버에 배포한다.
4. 기존 서버 실행 명령에 `--instance-environment 1=config/environment/DA_Environment_Coast.ini`처럼
   **등록되어 있는 인스턴스 종류 번호**와 설정 경로를 추가한다. 번호 1은 예시다.

기존 `--instance-weather`와 함께 지정하면 환경 파일의 전체 30개 값이 우선한다.
설정은 새 서버 프로세스/새 방에 적용되며 실행 중 DA 수정의 원격 전송 기능은 없다.
Yang2는 Scene의 Profile을 직접 읽으므로 서버용 내보내기가 필요 없다.
서버 숫자를 정하는 데 클라이언트 DA를 신뢰하거나 클라이언트에서 업로드하지 않는다.

## 그라데이션의 의미와 한계

`현재값 + (목표값-현재값) × (1-exp(-dt/반응시간))` 방식으로 변화시킨다.
갑자기 상태를 비/눈/낮/밤으로 토글하지 않는다. 다만 처음 방에 입장하면 현재 서버 상태로
즉시 맞춘다. 이전 방의 해안 수위를 새 사막으로 천천히 이어 붙이지 않기 위해서다.

조석은 바다의 외부 저장소에서 오는 물로 취급한다. 지면의 폐쇄된 물순환 질량 장부에
바다 전체를 억지로 합산하지 않는다. Water 파도는 **표면 표현**이며 유체 입자 전체를 푸는 해양 모델이 아니다.
간조 때 수위가 내려갈 수 있도록 Ocean 기준 위치와 양의 HeightOffset을 함께 보정한다.

아직 구현하지 않은 것은 지형별 날씨 격자·수증기 수평 이류·강물 유량/범람·침식/퇴적·
해일·부력/수영 서버 판정·계절 식생 아트·날씨에 따른 전투/포켓몬 출현 규칙이다.
위도/고도는 방의 설정이며 플레이어가 산을 오르면 자동으로 별도의 기후가 되는 구조는 아니다.
게임용 기압·바람/일사 근사이고 실제 지구 전체의 열역학·기후 예측 모델은 아니다.

## 확인 범위

독립 `InstanceEnvironmentTests`: 물 보존, 낮밤·연중 진행, 밀물/썰물, 건조/젖은 땅의 먼지,
프레임 간격 독립성, 설정 오류 거부를 검사한다.
UE `Heaven.Weather.EnvironmentCycles`: 자정/연말/방위 순환과 파고 합성을 검사한다.
`verify_environment_scenes.py`: 저장된 BP·DA·Niagara와 세 레벨의 연결을 검사한다.
실제 게임 접속·멀티플레이·장시간 부하 측정은 사용자가 제외한 범위다.
