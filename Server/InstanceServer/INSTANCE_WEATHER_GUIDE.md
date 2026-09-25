# 인스턴스 던전 서버 권위 날씨 구조

이 문서는 현재 프로젝트에 들어간 날씨 코드가 **어느 파일에 있고**, **각 코드가 무엇을
뜻하며**, **기존 인스턴스 서버와 어떻게 연결되고**, **언리얼 블루프린트가 어떤 값을
받는지**를 코드와 1:1로 설명한다.

## 1. 전체 구조

```text
로그인 후 인스턴스 입장
        │
        ▼
RoomManager::join(instanceType)
        │ 자리가 없으면
        ▼
RoomManager::createRoomLocked(type)
        │
        ├─ World 생성              : 플레이어·야생·충돌·이동
        └─ InstanceWeather 생성    : 그 방만의 대기·구름·지표·토양
                    │
                    ▼
       InstanceServer 20Hz 틱
                    │
                    ├─ 매 틱: 이동과 AI
                    └─ 현실 1초마다: 날씨 계산 + WeatherState 전송
                                             │
                                             ▼
                              FHHVFieldConnection (통신 스레드)
                                             │ 큐
                                             ▼
                         UUEFieldServerBridgeComponent (게임 스레드)
                                             │
                          OnInstanceWeatherChanged (블루프린트 이벤트)
                                             │
                   Niagara / 머티리얼 / 하늘 / 젖은 바닥 연출
```

핵심 소유 관계는 다음 한 줄이다.

```cpp
struct Room {
    World world;
    InstanceWeather weather;
};
```

`Room`이 사라지면 그 방의 `World`와 `weather`가 같이 사라진다. 따라서 1번 방의 비가
2번 방으로 새지 않고, 다른 던전에 들어간 플레이어가 같은 날씨를 억지로 공유하지 않는다.

## 2. 파일별 역할

| 파일 | 실제 역할 |
|---|---|
| `src/InstanceWeather.h` | 날씨 입력 설정, 내부 상태, 클라이언트로 내보낼 결과의 모양 |
| `src/InstanceWeather.cpp` | 증발·대기 혼합·응결·강수·눈/얼음·토양 침투 계산 |
| `src/RoomManager.h` | `Room` 하나에 `InstanceWeather` 하나를 실제로 소유 |
| `src/RoomManager.cpp` | 방 생성 때 날씨 초기화, 현실 1초마다 계산, 방 플레이어에게 전송 |
| `src/World.h/.cpp` | 그 방에 접속한 플레이어 모두에게 같은 날씨 패킷을 전달 |
| `Protocol/field.fbs` | 서버와 클라이언트가 약속한 `WeatherState` 패킷 구조 |
| `Protocol/FieldCodec.h` | C++ 날씨 결과를 FlatBuffers 패킷으로 직렬화 |
| `Net/HHVFieldConnection.h/.cpp` | 언리얼 통신 스레드에서 패킷을 읽어 게임 스레드 큐에 적재 |
| `Server/UEFieldServerBridgeComponent.h/.cpp` | 날씨를 블루프린트 구조체와 이벤트로 공개 |

## 3. `InstanceWeatherProfile`: 던전 종류의 기후 기준

```cpp
struct InstanceWeatherProfile {
    double meanTemperatureC = 18.0;
    double initialRelativeHumidityPct = 72.0;
    double meanPressureHpa = 1013.25;
    double initialSurfaceWaterKgM2 = 2.0;
    double initialSoilWaterKgM2 = 12.0;
    double gameSecondsPerRealSecond = 60.0;
};
```

각 변수의 뜻은 다음과 같다.

- `meanTemperatureC`: 이 던전 기단이 장기적으로 돌아가려는 평균 기온이다. 18이면 봄·가을
  정도, 음수로 내리면 눈과 결빙이 생길 수 있다.
- `initialRelativeHumidityPct`: 방이 처음 열렸을 때의 평균 상대습도다. 실제 방 상태에는
  `room_id` 기반 편차가 들어가므로 72를 넣어도 모든 방이 정확히 72%로 시작하지 않는다.
- `meanPressureHpa`: 기준 해면기압이다. 현재는 던전 전체가 한 기단이므로, 이 값을 중심으로
  느린 기압 변화가 생긴다.
- `initialSurfaceWaterKgM2`: 바닥에 고여 있는 물이다. 물 1 kg/m²는 수심 1 mm와 같다.
- `initialSoilWaterKgM2`: 토양이 처음 들고 있는 물이다. 지표수가 없으면 일부가 증발원으로
  쓰인다.
- `gameSecondsPerRealSecond`: 현실 1초에 날씨 시간이 몇 초 흐르는지 정한다. 기본값 60은
  현실 1분에 시뮬레이션 1시간이 흐른다는 뜻이다. 이것은 기존 요청대로 언리얼의 낮밤
  조명 순환과 연결하지 않았다.

이 설정은 `InstanceType`에 붙었다.

```cpp
struct InstanceType {
    const Map* map = nullptr;
    InstanceWeatherProfile weather;
    std::vector<std::uint16_t> wildSpecies;
};
```

따라서 나중에 인스턴스 종류 2를 설산으로 만들 때는 서버가 `types[2]`를 구성하는 자리에서
다음처럼 기준값만 바꾸면 된다.

```cpp
types[2].weather.meanTemperatureC = -4.0;
types[2].weather.initialRelativeHumidityPct = 82.0;
types[2].weather.initialSurfaceWaterKgM2 = 0.5;
types[2].weather.initialSoilWaterKgM2 = 8.0;
```

현재 `main.cpp`는 이 값을 따로 덮지 않으므로 모든 인스턴스 종류가 기본 기후를 사용한다.
그러나 실제 날씨 상태는 `room_id`를 섞어 초기화하므로 복제 방마다 다르다.

## 4. 방 하나가 실제로 들고 있는 물리 상태

### 대기층 `AirLayer`

```cpp
struct AirLayer {
    double temperatureC;
    double depthM;
    double vaporKgM2;
    double liquidKgM2;
};
```

- `temperatureC`: 이 대기층의 온도다.
- `depthM`: 계산용 대기층 두께다. 같은 수증기 질량이라도 얇은 층에 몰리면 증기압이
  높아진다.
- `vaporKgM2`: 기체 상태 수증기다. 상대습도와 응결 가능성을 계산한다.
- `liquidKgM2`: 공기 속 액체 물방울이다. 상층에서는 구름물, 저층에서는 안개 물방울로
  해석한다.

`nearAir_`는 지표 근처 120 m, `upperAir_`는 그 위 500 m를 대표한다. 렌더링용으로
수백 개 입자를 저장하는 구조가 아니라, 던전 한 방의 평균 기단을 나타내는 두 개의
수직 상자다.

### 지표 `Surface`

```cpp
struct Surface {
    double temperatureC;
    double waterKgM2;
    double soilKgM2;
    double snowKgM2;
    double iceKgM2;
};
```

- `waterKgM2`: 바닥에 고인 액체 물이다. 비가 오면 증가하고 증발·침투하면 감소한다.
- `soilKgM2`: 흙 속 물이다. 지표수가 스며들면 증가하고 배수·토양 증발로 감소한다.
- `snowKgM2`: 쌓인 눈의 물 환산 질량이다. `snowKgM2 / 100`으로 눈 깊이 m를 구한다.
- `iceKgM2`: 영하에서 언 지표수다.

이 값들은 서버 내부 상태다. 클라이언트는 직접 바꾸지 못한다.

## 5. 새 방이 열릴 때 초기화되는 순서

`RoomManager::createRoomLocked`에서 실제 연결은 다음처럼 이루어진다.

```cpp
room->id = nextRoomId_++;
room->type = type;
room->world.setMap(map);
room->weather.initialize(type, room->id, config.weather);
```

1. 고유 `room->id`를 받는다.
2. 던전 종류 `type`을 저장한다.
3. 기존 충돌 맵을 `World`에 연결한다.
4. 같은 방 객체 안의 `weather`를 초기화한다.

`initialize`는 다음 값을 `type`과 `roomId`로 만든 고정 씨앗에서 뽑는다.

- 초기 습도 편차: 기준값에서 -12~+12%p
- 초기 기온 편차: 기준값에서 -2~+2°C
- 초기 기압 편차: 기준값에서 -7~+7 hPa
- 초기 구름물: 0~0.16 kg/m²
- 느린 기단 변화의 위상

그래서 `type=1, room=10`은 다시 계산해도 같은 초기값을 얻지만,
`type=1, room=11`은 다른 날씨를 얻는다. 매 틱 무작위 수를 뽑지 않으므로 서버 재현성과
디버깅도 유지한다.

## 6. 한 번의 지구과학 계산

`InstanceWeather::simulateStep(dt)`는 다음 순서로 실행된다. 순서를 바꾸면 물의 출발지와
도착지가 달라지므로 코드에서도 이 순서를 고정했다.

### 6.1 기단 기온·기압 변화

```cpp
const double targetTemperature = meanTemperature + 3.0 * sin(frontAngle);
const double targetPressure = meanPressure + 9.0 * sin(frontAngle * 0.55 + 0.8);
nearAir.temperatureC = relax(current, target, dt, 1800.0);
```

`targetTemperature`와 `targetPressure`는 던전을 지나가는 느린 기단의 목표값이다. 이것은
낮밤 조명 계산이 아니다. `relax`는 현재값을 목표값으로 즉시 순간이동시키지 않고,
응답 시간에 따라 천천히 가까워지게 한다.

```cpp
alpha = 1 - exp(-dt / responseSeconds)
newValue = oldValue + (target - oldValue) * alpha
```

단순히 `old + speed * dt`로 만들면 서버가 잠깐 멈춰 `dt`가 커졌을 때 목표를 지나칠 수
있다. 지수 완화식은 큰 `dt`에서도 목표값을 넘지 않는다.

### 6.2 증발

```cpp
vaporDeficit = saturationPressure(groundTemperature)
             - vaporPressure(nearAir);
evaporationRate = coefficient * max(0, vaporDeficit);
```

따뜻한 지표가 만들 수 있는 포화수증기압보다 실제 공기의 수증기압이 낮으면 공기가 아직
물을 받을 수 있다. 이 차이가 클수록 증발량이 커진다.

- 바닥에 물이 있으면 `ground.water → nearAir.vapor`
- 바닥 물이 없으면 젖은 정도만큼 `ground.soil → nearAir.vapor`

`transfer(from, to, requested)`는 저장량보다 더 많은 물을 빼지 않는다. 예를 들어 바닥에
0.1 kg/m²만 있는데 0.5를 증발시키라고 해도 실제 이동량은 0.1이다.

### 6.3 두 대기층의 수증기 혼합

```cpp
nearConcentration = nearVapor / nearDepth;
upperConcentration = upperVapor / upperDepth;
```

저층과 상층의 수증기 농도 차이를 계산해 높은 쪽에서 낮은 쪽으로 옮긴다. 이 단계 덕분에
지표에서 증발한 물이 상층으로 올라가 구름 재료가 된다. 현재는 던전 평균 한 칸이므로
동서남북 이웃 셀로 이동하는 수평 이류는 없다.

### 6.4 응결

```cpp
capacity = saturationMassKgM2(air);
if (vapor > capacity)
    transfer(vapor, liquid, vapor - capacity);
```

현재 온도에서 공기가 들 수 있는 수증기 질량을 넘으면 초과분이 액체 물방울로 바뀐다.
반대로 포화량보다 수증기가 적고 액체 물방울이 남아 있으면 일부가 다시 증발한다.

이 계산에서 온도가 내려가면 포화량이 줄고, 같은 수증기량에서도 상대습도가 올라가며,
100%를 넘는 부분이 구름으로 바뀐다. 즉 기온·습도·구름이 서로 떨어진 임의 변수가 아니다.

### 6.5 비와 눈

```cpp
cloudExcess = max(0, upperLiquid - cloudThreshold);
falling = cloudExcess * (1 - exp(-dt / 600));
snowFraction = clamp((1 - nearTemperature) / 2, 0, 1);
```

상층 구름물이 `0.05 kg/m²`를 넘으면 초과분 일부가 내려온다. 지표 근처 기온에 따라
비와 눈의 비율을 나눈다.

- 1°C 이상: 거의 비
- -1°C 이하: 거의 눈
- 그 사이: 비와 눈이 섞임

이번 단계에서 실제로 내려온 질량을 `mm/h`로 환산해 클라이언트에 보낸다. 따라서
나이아가라 비 방출량을 이 값에 연결할 수 있다.

### 6.6 눈·얼음

- 지표 온도가 0°C보다 높으면 눈과 얼음이 지표수로 녹는다.
- 0°C 이하면 지표수가 얼음으로 이동한다.

현재 모델은 졸업작품 서버 비용에 맞춘 온도 지수 방식이다. 잠열까지 풀어내는 완전한
에너지 방정식은 아직 포함하지 않았다.

### 6.7 침투와 배수

```cpp
soilSpace = soilCapacity - soilWater;
transfer(surfaceWater, soilWater, infiltrationRate * dt);

soilExcess = max(0, soilWater - fieldCapacity);
drained = soilExcess * (1 - exp(-dt / 86400));
```

비가 온 직후 지표수는 토양의 빈 공간으로 스며든다. 토양이 포장용수량을 넘으면 아주
천천히 외부로 배수된다. 배수된 물은 없어진 것이 아니라 `drainedWaterKgM2_`에 기록해
물 수지 검사에 사용한다.

## 7. 물 수지 오차가 필요한 이유

서버가 계산한 물은 다음 저장소 사이를 옮겨 다닌다.

```text
지표수 ↔ 수증기 ↔ 구름물 → 비/눈 → 지표수 → 토양수 → 외부 배수
```

검사용 값은 다음 식이다.

```cpp
error = currentWater - initialWater + drainedWater;
```

외부 배수 외에는 물을 새로 만들거나 삭제하지 않으므로 정상이라면 부동소수점 반올림
수준으로 0에 가까워야 한다. `WaterBalanceErrorKgM2`가 계속 커지면 VFX 문제가 아니라
서버 계산에서 물이 생성되거나 사라진 버그라는 뜻이다.

## 8. 기존 20Hz 서버에 부담 없이 연결되는 방식

기존 `RoomManager::tickShard`는 20Hz로 이동과 야생 AI를 돌린다. 날씨는 그 빈도를 그대로
따르지 않는다.

실제 `DeltaTime`의 출발점은 `main.cpp`의 `std::chrono::steady_clock`이다.

```cpp
const auto now = std::chrono::steady_clock::now();
const float dt = std::chrono::duration<float>(now - previous).count();
previous = now;
rooms.tickShard(shard, tickThreads, dt);
```

즉 임의로 `0.05`를 날씨 시간에 더하는 방식이 아니다. 서버가 실제로 지난 시간을
`chrono`로 재고, 그 `dt`가 `RoomManager`를 거쳐 `InstanceWeather::advance`까지 들어간다.
`steady_clock`은 윈도우 시각을 바꿔도 뒤로 뛰지 않으므로 게임 델타타임에 맞다.

```cpp
room->weatherBroadcastAccumulator += dt;
if (room->weatherBroadcastAccumulator >= 1.0) {
    const double elapsed = room->weatherBroadcastAccumulator;
    room->weatherBroadcastAccumulator = 0.0;
    room->weather.advance(elapsed);
    const InstanceWeatherSnapshot weather = room->weather.snapshot();
    room->world.broadcast(proto::encodeWeatherState(
        weather.roomId, weather.revision, weather.simulationTimeSeconds,
        weather.temperatureC, weather.relativeHumidityPct, weather.pressureHpa,
        weather.cloudCover, weather.precipitationMmPerHour, weather.windSpeedMps,
        weather.windDirectionDegrees, weather.groundWetness, weather.snowDepthM,
        weather.waterBalanceErrorKgM2));
}
```

기본 설정에서 방 하나는 다음 정도만 계산한다.

- 현실 1초마다 호출 1회
- 현실 1초 = 시뮬레이션 60초
- 내부 고정 단계 10초 × 6회
- 한 단계는 대기층 2개와 지표 1개만 계산

따라서 방 100개가 동시에 차 있어도 초당 약 600번의 작은 상태 갱신이다. 96칸 격자를
모든 방에 복제하는 구조보다 훨씬 가볍다. 비어 있는 방은 회수 유예 60초 동안 존재해도
날씨 계산과 전송을 멈춘다.

방에 별도 날씨 스레드를 만들지 않았다. 기존 규칙대로 같은 `room_id`는 늘 같은 틱 샤드가
처리하므로 `InstanceWeather` 내부에는 mutex가 필요 없다. 네트워크 전송 때만 `World`의
기존 엔티티 mutex를 사용한다.

## 9. 서버에서 클라이언트까지 가는 데이터

`WeatherState`는 다음 값을 초당 한 번 보낸다.

| 필드 | 단위/범위 | 클라이언트에서 쓸 곳 |
|---|---:|---|
| `room_id` | 방 번호 | 현재 입장 방과 패킷 방이 같은지 검사 |
| `revision` | 증가 번호 | 새 상태인지 판단 |
| `simulation_time_seconds` | s | 보간 기준, 디버그 표시 |
| `temperature_c` | °C | 눈/비 연출, 입김, 환경 UI |
| `relative_humidity_pct` | % | 안개·습한 공기 표현 |
| `pressure_hpa` | hPa | 날씨 UI, 향후 기압 경향 계산 |
| `cloud_cover` | 0~1 | 구름 양, 하늘 밝기 |
| `precipitation_mm_per_hour` | mm/h | 비·눈 Niagara 방출량 |
| `wind_speed_mps` | m/s | 입자 속도, 나뭇잎 흔들림 |
| `wind_direction_degrees` | 0~360° | 바람 벡터 방향 |
| `ground_wetness` | 0~1 | 젖은 바닥 머티리얼 파라미터 |
| `snow_depth_m` | m | 눈 쌓임 머티리얼/지형 효과 |
| `water_balance_error_kg_m2` | kg/m² | 서버 계산 검증용 |

이 패킷을 `Snapshot`에 넣지 않은 이유는 이동이 20Hz라서다. 날씨 값이 1초에 한 번만
바뀌는데 이동 패킷에 넣으면 같은 값을 20번 반복 전송한다.

## 10. 언리얼과 연결되는 코드

통신 스레드의 `FHHVFieldConnection`은 `WeatherState`를 받으면
`FHHVInstanceWeatherState`로 복사해 큐에 넣는다. `Poll()`은 게임 스레드에서 그 큐를
꺼내 다음 콜백을 호출한다.

```cpp
TFunction<void(const FHHVInstanceWeatherState &Weather)> OnWeatherState;
```

`UUEFieldServerBridgeComponent`는 이 콜백을 블루프린트 이벤트로 바꾼다.

```cpp
UPROPERTY(BlueprintAssignable, Category = "Field Server|Weather")
FUEOnInstanceWeatherChanged OnInstanceWeatherChanged;

UFUNCTION(BlueprintPure, Category = "Field Server|Weather")
const FUEInstanceWeatherState &GetInstanceWeatherState() const;
```

블루프린트에서 쓰는 순서는 다음과 같다.

1. 로컬 `PlayerController`에서 `UEFieldServerBridgeComponent`를 얻는다.
2. `On Instance Weather Changed` 이벤트를 바인딩한다.
3. 이벤트의 `Weather` 구조체를 `Break FUEInstanceWeatherState`로 나눈다.
4. `CloudCover`, `PrecipitationMmPerHour`, `WindSpeedMps`, `GroundWetness`를 각 연출
   블루프린트에 전달한다.
5. 화면이나 날씨 액터가 늦게 생겼다면 `HasInstanceWeatherState`를 확인하고
   `GetInstanceWeatherState`로 마지막 값을 즉시 읽는다.

클라이언트는 날씨를 다시 계산하지 않는다. 서버 값을 시각적으로 부드럽게 보간하기만
해야 한다. 그래야 같은 방의 모든 플레이어가 같은 비와 바닥 젖음 상태를 본다.

### 인스턴스 종류별 기후 지정

서버 실행 시 `--instance-weather`를 반복해서 주면 인스턴스 종류마다 다른 기후로
방을 시작한다. 값의 순서는 다음과 같다.

```text
type=평균기온°C,초기상대습도%,평균기압hPa,지표수kg/m²,토양수kg/m²,시간배율
```

예를 들어 1번 인스턴스를 온화하고 습한 숲으로 시작하려면 다음처럼 지정한다.

```text
--instance-weather 1=18,82,1013.25,4,16,60
```

같은 `type`으로 생성된 방은 이 기후를 공유하지만 `room_id`가 난수 씨앗에 들어가므로
구름, 초기 온도와 기압은 조금씩 다르다. 기후가 지정되지 않은 종류는
`InstanceWeatherProfile`의 기본값을 사용한다. 맵이 등록되지 않은 타입을 지정하면
서버가 시작 단계에서 오류를 내므로 설정 오타가 조용히 무시되지 않는다.

### 공용 날씨 표현 컴포넌트

`UUEInstanceWeatherPresentationComponent`는 서버 패킷과 Yang2 로컬 계산을 구분하지
않고 기존 `OnInstanceWeatherChanged`를 받는다. 날씨 연출 블루프린트에 이 컴포넌트를
추가하고 `On Weather Presentation Updated`를 연결한다.

컴포넌트가 보내는 값은 다음과 같다.

- `CloudAmount`: 구름과 하늘 머티리얼에 넣는 0~1 값
- `RainIntensity`, `SnowIntensity`: Niagara 방출량에 넣는 0~1 값
- `FogDensity`: Exponential Height Fog 또는 로컬 안개 머티리얼에 넣는 0~1 값
- `WindIntensity`, `WindDirectionDegrees`: 입자와 나뭇잎 방향에 넣는 값
- `GroundWetness`: 지면 머티리얼 파라미터에 넣는 0~1 값
- `SnowCoverage`: 적설 머티리얼 파라미터에 넣는 0~1 값
- `bRaining`, `bSnowing`, `bFoggy`: 출현 규칙과 향후 전투가 읽을 수 있는 상태

서버 값은 초당 한 번 오지만 컴포넌트가 매 프레임 보간한 뒤 이벤트를 보내므로 비와
구름이 갑자기 튀지 않는다. 실제 Niagara, 머티리얼, 사운드는 이 C++ 클래스에
하드코딩하지 않고 블루프린트에서 직접 배치한다.

## 11. 연출 블루프린트에 연결할 때의 예

초당 한 번 들어온 값을 바로 입자에 넣으면 비가 계단처럼 변할 수 있다. 연출 액터가
매 프레임 다음처럼 현재 표시값을 목표값으로 보간하면 된다.

```text
TargetRain = Weather.PrecipitationMmPerHour
DisplayedRain = FInterp To(DisplayedRain, TargetRain, DeltaSeconds, 2.0)

TargetCloud = Weather.CloudCover
DisplayedCloud = FInterp To(DisplayedCloud, TargetCloud, DeltaSeconds, 0.5)
```

예시 연결값은 다음처럼 잡을 수 있다. 이것은 물리 계산이 아니라 화면 표현 범위다.

```text
RainSpawnRate = MapRangeClamped(PrecipitationMmPerHour, 0..10, 0..5000)
WetMaterial    = GroundWetness
CloudOpacity   = CloudCover
WindVector.X   = cos(radians(WindDirectionDegrees)) * WindSpeedMps
WindVector.Y   = sin(radians(WindDirectionDegrees)) * WindSpeedMps
```

비·눈 Niagara와 머티리얼 파라미터 컬렉션은 에셋의 외형과 레벨 배치를 결정하는 작업이라
이번 서버 계산 코드에는 하드코딩하지 않았다. 서버는 권위 있는 숫자를 주고, 보이는 결과는
블루프린트와 에셋이 맡는 기존 프로젝트 원칙을 그대로 따른다.

## 12. 지금 모델의 범위와 다음 확장 지점

현재 구현은 **방 하나를 평균 기단 한 칸으로 계산**한다. 사용자가 말한 “첫 번째 인스턴스
날씨가 이렇고 두 번째 인스턴스는 다르다”는 요구에는 맞는다. 같은 방 안의 산 정상과 계곡이
서로 다른 비를 갖는 공간 날씨는 아직 계산하지 않는다.

공간 날씨가 실제 게임 플레이에 필요해질 때만 `InstanceWeather` 내부를 여러 `Cell`로
바꾸면 된다. `Room`, `WeatherState`, 블루프린트 이벤트의 바깥 연결은 유지할 수 있다.
그때 추가할 대상은 다음 세 가지다.

- 맵을 4×4 또는 8×8 기후 셀로 나누기
- 바람에 따른 수증기 수평 이류
- 고도와 경사에 따른 지형성 상승·강수

현재 졸업작품 단계에서는 한 방 평균 상태가 서버 비용과 구현 검증 면에서 가장 작은 완성
단위다. 플레이어 위치별 날씨가 실제 기획에 들어올 때 격자를 올리는 편이 안전하다.
