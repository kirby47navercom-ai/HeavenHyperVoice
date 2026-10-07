# 퍼지 날씨 규칙: 실제 코드와 함께 읽는 설명

## 먼저 무엇을 바꿨는지

이 문서는 이번에 추가한 **날씨 판단 코드**를 설명해요. 비·눈·안개·모래폭풍을 대상으로 해요.
전투, 포획, 이동, 수영 같은 새 게임 규칙은 만들지 않았어요.

- 비·눈: 습도·기압·구름으로 강수 배율을 정하고, 기온으로 눈의 비중을 정해요.
- 안개: 저층 습도·잔잔한 바람·차가운 지표를 함께 판단해요.
- 모래폭풍: 토양 건조·강풍·표면 건조를 함께 판단해요.
- 기존 낮밤·계절·조석·파도·젖음·적설 계산은 계속 사용해요.

**기존 지구과학 코드 전체를 퍼지로 교체한 것은 아니에요. 날씨의 판단 부분을 퍼지로 바꿨어요.**
강수의 세기를 판단한 뒤 실제 구름물을 비·눈 저장소로 옮기는 기존 흐름도 유지해요.
이번 규칙과 숫자는 우리 게임에 맞게 조절할 초기 모델이며, 실제 기상 관측으로 검증한 예보 모델은 아니에요.

## 1. 퍼지 규칙이 무슨 뜻인지

일반 조건문을 이렇게 작성할 수 있어요.

```cpp
if (humidityPct >= 80) { /* 습함 */ }
```

이렇게 판단하면 79.9%와 80.0% 사이에서 조건이 갑자기 달라져요.
퍼지는 '습함'이라는 집합에 얼마나 속하는지를 숫자로 나타내요.
이번 강수 기준은 습도 65% 이하에서 0, 95% 이상에서 1이에요.

| 상층 상대 습도 | '습함' 소속도 | 의미 |
|---|---:|---|
| 65% | 0 | 설정한 습함 조건에 해당하지 않아요 |
| 72.5% | 0.25 | 조금 해당해요 |
| 80% | 0.5 | 절반 정도 해당해요 |
| 87.5% | 0.75 | 상당히 해당해요 |
| 95% | 1 | 완전히 해당해요 |

**소속도 0.75는 비가 올 확률 75%라는 뜻이 아니에요.**
'습함'이라는 조건에 해당하는 정도예요. 최종 강수 배율 역시 확률이 아니에요.

이번 방식은 **0차 Sugeno 퍼지 추론**이에요.
규칙의 결과를 '약한 비'라는 그림 모양의 집합으로 만들지 않고, `.35`처럼 일정한 숫자로 정해요.
각 규칙이 성립하는 정도로 그 숫자들을 가중 평균해요.

```text
실제 습도·기압·구름 값
  → 각각을 0~1의 소속도로 변환
  → 여러 IF 조건의 성립 정도 계산
  → 규칙별 결과를 가중 평균
  → 강수 배율·눈 비중·안개 강도·모래폭풍 목표 강도
```

소속 함수, AND/NOT, 규칙, 가중 평균은 각각 다른 역할이에요.
이후 화면이 서서히 변하게 만드는 시간 보간도 별도 역할이에요.
퍼지 자체가 시간을 흐르게 하거나 VFX 입자를 생성하지는 않아요.

## 2. 파일 구조와 실행 순서

```text
기존 환경 DA의 FuzzyWeather
 ├─ main: export_environment_profiles.py → 서버 ini → EnvironmentConfig.cpp
 └─ Yang2: UEYang2Environment.cpp → 같은 C++ 설정
                  ↓
        InstanceWeather::simulateStep()
          ├─ 기존 시간·대기·지표 갱신
          ├─ SurfaceWaterCycle.cpp → FuzzyWeather.cpp → 강수·비/눈 비중
          └─ DesertEnvironment.cpp → FuzzyWeather.cpp → 모래폭풍 목표
                  ↓
        WeatherSnapshot.cpp → 안개·눈 비중 포함
          ├─ main: FieldCodec.h → field.fbs 패킷 → HHVFieldConnection.cpp
          └─ Yang2: MakeYang2EnvironmentState()
                  ↓
        UEInstanceWeatherPresentationComponent.cpp
                  ↓
        기존 날씨 Director / Scene → Niagara / 머티리얼 / 안개
```

| 파일 | 하는 일 |
|---|---|
| `src/EarthScience/FuzzyWeather.h` | 조절할 설정, 입력값, 계산 결과의 모양을 선언해요 |
| `src/EarthScience/FuzzyWeather.cpp` | 소속도와 규칙의 성립 정도, 최종 결과를 계산해요 |
| `src/EarthScience/WeatherSnapshot.cpp` | 현재 방 상태를 퍼지 입력으로 모으고 서버 결과를 만들어요 |
| `src/EarthScience/SurfaceWaterCycle.cpp` | 퍼지 결과를 실제 강수 배율과 눈의 비중에 사용해요 |
| `src/EarthScience/DesertEnvironment.cpp` | 퍼지 결과를 모래폭풍 목표 강도에 사용해요 |
| `src/EarthScience/EnvironmentConfig.cpp` | 환경 ini의 퍼지 설정을 읽고 잘못된 값을 거절해요 |
| `src/EarthScience/WeatherPersistence.cpp` | 규칙 설정이 달라진 상태를 잘못 복구하지 않게 해요 |
| 클라이언트 `Environment/UEFuzzyWeatherProfile.h` | 언리얼 데이터 에셋에서 펼쳐 편집할 변수를 제공해요 |
| 클라이언트 `Environment/UEInstanceWeatherPresentationComponent.cpp` | 서버가 정한 눈 비중·안개 강도를 화면에 전달해요 |
| Yang2의 `UEYang2Environment.cpp` | DA를 로컬 계산기에 전달하고 결과를 공통 화면 상태로 옮겨요 |

main의 클라이언트는 `FuzzyWeather.cpp`를 실행하지 않아요.
Yang2만 기존 브랜치 전용 파일에서 실제 서버 구현을 함께 컴파일해 로컬로 실행해요.
공통 규칙은 main에 먼저 반영하고 main → Yang2로 가져와요. Yang2 권위 코드를 main으로 올리지 않아요.

## 3. FuzzyWeather.h — 어떤 변수가 있는지

### 설정 FuzzyWeatherProfile

```cpp
struct FuzzyWeatherProfile {
    double humidStartPct = 65, humidFullPct = 95;
    double fogHumidStartPct = 85, fogHumidFullPct = 100;
    double lowPressureFullDeficitHpa = 12;
    double cloudFullCover = .7;
    double fullSnowTemperatureC = -1, fullRainTemperatureC = 1;
    double fogCoolingFullC = 4;
    double fogCalmStartMps = .5, fogCalmEndMps = 5;
    double soilDryStart = .2, soilDryFull = .8;
    std::array<double, 8> rainRuleOutputs{0, 0, 0, 0, 0, .35, 0, 1};
    std::array<double, 8> fogRuleOutputs{0, 0, 0, 0, .05, .15, .65, 1};
    std::array<double, 8> dustRuleOutputs{0, 0, 0, 0, 0, 0, 0, 1};
};
```

이 구조체는 현재 날씨가 아니라 **판단 기준**이에요. 지역별 DA에서 값을 바꿀 수 있어요.

| 변수 | 기본값 | 어디에 사용하고, 바꾸면 어떻게 되는지 |
|---|---:|---|
| `humidStartPct` | 65 | 강수용 '습함' 소속도가 0에서 증가하기 시작하는 상층 습도예요 |
| `humidFullPct` | 95 | 강수용 '습함' 소속도가 1이 되는 상층 습도예요 |
| `fogHumidStartPct` | 85 | 안개용 '습함'이 시작하는 저층 습도예요 |
| `fogHumidFullPct` | 100 | 안개용 '습함'이 1이 되는 저층 습도예요 |
| `lowPressureFullDeficitHpa` | 12 | 지역 평균보다 기압이 12hPa 낮으면 '저기압' 소속도가 1이에요. 값을 줄이면 작은 기압 하강에도 강하게 반응해요 |
| `cloudFullCover` | 0.7 | 구름 덮임 0.7을 '구름 많음' 1로 봐요. 줄이면 적은 구름에도 강하게 반응해요 |
| `fullSnowTemperatureC` | −1°C | 이 온도 이하에서는 강수를 모두 눈으로 나눠요 |
| `fullRainTemperatureC` | +1°C | 이 온도 이상에서는 강수를 모두 비로 나눠요 |
| `fogCoolingFullC` | 4°C | 지표가 공기보다 4°C 차가우면 '차가운 지표' 소속도가 1이에요 |
| `fogCalmStartMps` | 0.5m/s | 이 풍속 이하에서는 '잔잔함' 소속도가 1이에요 |
| `fogCalmEndMps` | 5m/s | 이 풍속 이상에서는 '잔잔함' 소속도가 0이에요 |
| `soilDryStart` | 0.2 | 건조 비율 0.2부터 '마름' 소속도가 증가해요 |
| `soilDryFull` | 0.8 | 건조 비율 0.8부터 '마름' 소속도가 1이에요 |
| `rainRuleOutputs` | 8개 값 | 각 강수 규칙이 완전히 성립했을 때 선택할 강수 배율이에요 |
| `fogRuleOutputs` | 8개 값 | 각 안개 규칙의 목표 강도예요 |
| `dustRuleOutputs` | 8개 값 | 각 먼지 규칙의 목표 강도예요 |

`Start`와 `Full`은 반드시 Start < Full이어야 해요. 같으면 나눗셈의 분모가 0이 돼요.
풍속의 Start/End와 눈/비 온도에도 같은 순서 조건을 적용해요.

### 입력 FuzzyWeatherInputs

```cpp
struct FuzzyWeatherInputs {
    double temperatureC = 0;
    double upperHumidityPct = 0;
    double nearHumidityPct = 0;
    double pressureDeficitHpa = 0;
    double cloudCover = 0;
    double surfaceCoolingC = 0;
    double windSpeedMps = 0;
    double soilDryness = 0, surfaceDryness = 0;
    double dustWind = 0;
};
```

이 값들은 플레이어가 입력하는 설정이 아니라 **현재 방에서 계산된 상태**예요.

| 변수 | 뜻 | 지구과학과의 연결 |
|---|---|---|
| `temperatureC` | 지표 가까운 공기의 °C | 강수가 비인지 눈인지 나눌 기준이에요 |
| `upperHumidityPct` | 상층 상대 습도 % | 구름이 있는 대기층의 습함을 판단해요 |
| `nearHumidityPct` | 저층 상대 습도 % | 플레이어 주변 대기의 안개 조건을 판단해요 |
| `pressureDeficitHpa` | 지역 평균 기압 − 현재 기압 | 절대 기압 하나를 모든 지역에 강요하지 않고 지역 기준보다 낮은 정도를 봐요 |
| `cloudCover` | 기존 구름 덮임 0~1 | 강수에 사용할 구름이 얼마나 있는지 판단해요 |
| `surfaceCoolingC` | 공기 온도 − 지표 온도 | 지표가 차가워지는 안개 조건을 표현해요 |
| `windSpeedMps` | 실제 방 평균 풍속 m/s | 안개의 '잔잔함'을 판단해요 |
| `soilDryness` | 토양·지표수로 산출한 건조 비율 0~1 | 모래가 날릴 만큼 땅이 말랐는지 봐요 |
| `surfaceDryness` | 표면 물막·지표수로 산출한 건조 비율 0~1 | 토양 안쪽이 말라도 표면이 젖으면 먼지를 억제해요 |
| `dustWind` | 기존 먼지 풍속 기준으로 변환한 소속도 0~1 | 모래를 날리는 강풍 조건이에요 |

현재는 방 평균 날씨예요. 입력에 플레이어 위치나 지역 셀 좌표는 없어요.
상층·저층 상대 습도를 분리한 이유는 구름의 강수 조건과 지표 주변 안개 조건을 구분하기 위해서예요.

### 결과 FuzzyWeatherResult

```cpp
struct FuzzyWeatherResult {
    double precipitationStrength = 0;
    double snowFraction = 0;
    double fogDensity = 0, sandstormStrength = 0;
    std::array<double, 8> rainActivations{}, fogActivations{}, dustActivations{};
};
```

| 변수 | 다음 단계에서 어떻게 쓰는지 |
|---|---|
| `precipitationStrength` | 기존 구름물의 하강량에 곱해요. 0.3은 강수 배율 0.3이지, 비가 올 확률 30%가 아니에요 |
| `snowFraction` | 내리는 물 중 눈으로 보낼 비중이에요. 0.75라면 눈 75%, 비 25%로 나눠요 |
| `fogDensity` | 서버가 보내는 안개 목표 강도예요. 최종 안개 액터의 실제 밀도 단위는 기존 연출 설정에 따라요 |
| `sandstormStrength` | 모래폭풍의 목표 강도예요. 지역의 모래량·눈 덮임 제한을 거쳐 시간 보간해요 |
| 세 `*Activations` 배열 | 0~7번 규칙이 얼마나 성립했는지예요. 디버거에서 어떤 규칙이 결과에 기여했는지 볼 수 있어요 |

## 4. FuzzyWeather.cpp — 소속 함수 rising()

```cpp
double rising(double value, double start, double full) {
    return earth::clamp01((value - start) / (full - start));
}
```

- `value`: 현재 입력값이에요. 예를 들어 습도 80이에요.
- `start`: 조건에 속하기 시작하는 기준이에요. 강수 습도는 65예요.
- `full`: 조건에 완전히 속하는 기준이에요. 강수 습도는 95예요.
- `(80−65)/(95−65) = 15/30 = 0.5`: '습함' 소속도가 0.5예요.
- `clamp01`: 결과를 0~1 안에 묶어요. 습도 50이면 0, 100이면 1이에요.

곡선이 아니라 직선 소속 함수를 사용해요. 기준을 해석하고 조절하기 쉬워서예요.
습하지 않음은 새 함수를 만들지 않고 `1 - 습함`으로 정해요.

## 5. FuzzyWeather.cpp — 규칙을 합치는 infer()

```cpp
double infer(const std::array<double, 3>& membership,
             const std::array<double, 8>& outputs,
             std::array<double, 8>& activations) {
    double weighted = 0, total = 0;
    for (std::size_t rule = 0; rule < outputs.size(); ++rule) {
        double activation = 1;
        for (std::size_t term = 0; term < membership.size(); ++term) {
            activation *= (rule & (4u >> term))
                ? membership[term] : 1 - membership[term];
        }
        activations[rule] = activation;
        weighted += activation * outputs[rule];
        total += activation;
    }
    return total > 0 ? earth::clamp01(weighted / total) : 0;
}
```

코드와 의미를 순서대로 연결하면 다음과 같아요.

1. `membership`: 조건 3개의 소속도예요. 강수라면 습함·저기압·구름 많음이에요.
2. `outputs`: 8개 IF 규칙의 결과 숫자예요. DA의 `RainRuleOutputs` 등이 이 값이에요.
3. `activations`: 계산한 규칙 성립 정도를 돌려주는 배열이에요.
4. `weighted`: 규칙의 성립 정도 × 그 규칙의 결과를 누적해요.
5. `total`: 모든 규칙의 성립 정도를 누적해요.
6. `rule`: 지금 계산하는 0~7번 규칙이에요.
7. `term`: 그 규칙에서 첫째·둘째·셋째 조건 중 무엇을 계산하는지예요.
8. `4u >> term`: 순서대로 4, 2, 1이 돼요. 조건 3개를 3자리 이진수로 표현해요.
9. `rule & ...`: 해당 조건이 1이면 원래 소속도, 0이면 반대 소속도 `1-m`을 선택해요.
10. `activation *= ...`: AND를 곱으로 처리해요. 세 조건이 .5라면 .125예요.
11. 마지막에 `weighted / total`로 여러 규칙을 하나의 숫자로 바꿔요. 이것이 비퍼지화예요.

AND를 곱으로 쓰는 방법과 최솟값으로 쓰는 방법 모두 가능해요. 이번 코드는 곱을 사용해요.
조건이 부분적으로만 성립하면 강도가 함께 줄어드는 모델이에요.

참 조건과 반대 조건의 모든 조합을 포함하므로 성립 정도의 합은
`(m1+(1-m1)) × (m2+(1-m2)) × (m3+(1-m3)) = 1`이에요.
어느 조건 사이에서도 적용할 규칙이 사라지지 않아요. 부동소수점 오차를 고려해 실제 합으로 나눠요.

## 6. 강수 규칙 8개 — 습함·저기압·구름 많음

```cpp
result.precipitationStrength = infer({
    rising(in.upperHumidityPct, p.humidStartPct, p.humidFullPct),
    rising(in.pressureDeficitHpa, 0, p.lowPressureFullDeficitHpa),
    rising(in.cloudCover, 0, p.cloudFullCover)},
    p.rainRuleOutputs, result.rainActivations);
```

첫 조건은 상층 습함, 둘째는 저기압, 셋째는 구름 많음이에요.
여기서 0은 해당 조건의 **반대 소속도**예요. 경계에서 완전히 건조하거나 완전히 맑다는 뜻은 아니에요.

| 인덱스/비트 | IF 조건 | 기본 결과 |
|---|---|---:|
| 0 / 000 | 습함 반대 AND 저기압 반대 AND 구름 많음 반대 | 0 |
| 1 / 001 | 습함 반대 AND 저기압 반대 AND 구름 많음 | 0 |
| 2 / 010 | 습함 반대 AND 저기압 AND 구름 많음 반대 | 0 |
| 3 / 011 | 습함 반대 AND 저기압 AND 구름 많음 | 0 |
| 4 / 100 | 습함 AND 저기압 반대 AND 구름 많음 반대 | 0 |
| 5 / 101 | 습함 AND 저기압 반대 AND 구름 많음 | 0.35 |
| 6 / 110 | 습함 AND 저기압 AND 구름 많음 반대 | 0 |
| 7 / 111 | 습함 AND 저기압 AND 구름 많음 | 1 |

5번은 저기압이 아니어도 습하고 구름이 있으면 약한 강수를 허용하는 규칙이에요.
7번은 세 조건이 모두 강할 때 더 강한 강수를 허용해요.
다른 규칙은 기본적으로 강수를 만들지 않아요. 주인님이 DA에서 결과를 바꿀 수 있어요.

예를 들어 습도 80%, 지역 평균보다 기압이 6hPa 낮음, 구름 덮임 .35이면
세 소속도는 모두 .5예요. 각 규칙은 `.5³ = .125`만큼 성립해요.

```text
강수 배율 = (.125 × .35 + .125 × 1) / 1 = .16875
```

이 결과가 .16875여도 구름물이 없으면 실제 비는 없어요. 물이 없는 곳에서 물을 만들어내지 않아요.

## 7. 눈·비 규칙 2개 — 추움과 따뜻함

```cpp
const double warm = rising(in.temperatureC,
                          p.fullSnowTemperatureC, p.fullRainTemperatureC);
result.snowFraction = (1 - warm) * 1 + warm * 0;
```

`warm`은 '따뜻함' 소속도예요. `1-warm`은 '추움' 소속도예요.
'추우면 눈 비중 1', '따뜻하면 눈 비중 0'이라는 두 규칙을 섞어요.
두 규칙 성립 정도의 합이 1이라 별도 나눗셈은 필요 없어요.

| 기온 | warm | 눈 비중 | 비 비중 |
|---|---:|---:|---:|
| −1°C 이하 | 0 | 1 | 0 |
| −0.5°C | 0.25 | 0.75 | 0.25 |
| 0°C | 0.5 | 0.5 | 0.5 |
| +0.5°C | 0.75 | 0.25 | 0.75 |
| +1°C 이상 | 1 | 0 | 1 |

기본값에서는 이전 온도 분배식과 수치가 같아요. 퍼지를 추가했다고 이 기본 경계가 저절로 더 현실적으로 바뀌지는 않아요.
달라진 점은 경계를 DA에서 조절하고, 서버의 같은 비중을 적설과 화면에 함께 사용한다는 점이에요.

## 8. 안개 규칙 8개 — 습함·잔잔함·차가운 지표

```cpp
result.fogDensity = infer({
    rising(in.nearHumidityPct, p.fogHumidStartPct, p.fogHumidFullPct),
    1 - rising(in.windSpeedMps, p.fogCalmStartMps, p.fogCalmEndMps),
    rising(in.surfaceCoolingC, 0, p.fogCoolingFullC)},
    p.fogRuleOutputs, result.fogActivations);
```

풍속이 증가할수록 '잔잔함'은 감소해야 하므로 `1-rising(...)`을 사용해요.
지표가 공기보다 차가울수록 마지막 조건의 소속도가 올라가요.
낮밤의 기존 지표 온도 변화가 안개 입력으로 이어져요. '밤이면 무조건 안개'라는 규칙은 아니에요.

| 인덱스/비트 | IF 조건 | 기본 안개 강도 |
|---|---|---:|
| 0 / 000 | 습함 반대 AND 잔잔함 반대 AND 차가운 지표 반대 | 0 |
| 1 / 001 | 습함 반대 AND 잔잔함 반대 AND 차가운 지표 | 0 |
| 2 / 010 | 습함 반대 AND 잔잔함 AND 차가운 지표 반대 | 0 |
| 3 / 011 | 습함 반대 AND 잔잔함 AND 차가운 지표 | 0 |
| 4 / 100 | 습함 AND 잔잔함 반대 AND 차가운 지표 반대 | 0.05 |
| 5 / 101 | 습함 AND 잔잔함 반대 AND 차가운 지표 | 0.15 |
| 6 / 110 | 습함 AND 잔잔함 AND 차가운 지표 반대 | 0.65 |
| 7 / 111 | 습함 AND 잔잔함 AND 차가운 지표 | 1 |

습기가 강하면 약한 안개를 허용하되, 잔잔함과 냉각이 더해지면 짙게 만드는 게임용 규칙이에요.
복사안개·이류안개 등 실제 모든 발생 원인을 각각 계산하는 모델은 아니에요.

저층 습도92.5%, 풍속2.75m/s, 공기보다 지표가2°C 차가우면 세 소속도가 모두.5예요.
8개 규칙이 각각.125씩 성립하므로 결과는 `(.05+.15+.65+1)/8 = .23125`예요.

## 9. 모래폭풍 규칙 8개 — 토양 마름·강풍·표면 마름

```cpp
result.sandstormStrength = infer({
    rising(in.soilDryness, p.soilDryStart, p.soilDryFull),
    earth::clamp01(in.dustWind), earth::clamp01(in.surfaceDryness)},
    p.dustRuleOutputs, result.dustActivations);
```

첫 조건은 흙 속까지 얼마나 말랐는지, 둘째는 모래가 날릴 정도로 바람이 강한지,
셋째는 표면의 물막이 없는지를 봐요. 비가 그친 직후 표면이 젖어 있으면 먼지를 억제할 수 있어요.

| 인덱스/비트 | IF 조건 | 기본 목표 강도 |
|---|---|---:|
| 0 / 000 | 토양 마름 반대 AND 강풍 반대 AND 표면 마름 반대 | 0 |
| 1 / 001 | 토양 마름 반대 AND 강풍 반대 AND 표면 마름 | 0 |
| 2 / 010 | 토양 마름 반대 AND 강풍 AND 표면 마름 반대 | 0 |
| 3 / 011 | 토양 마름 반대 AND 강풍 AND 표면 마름 | 0 |
| 4 / 100 | 토양 마름 AND 강풍 반대 AND 표면 마름 반대 | 0 |
| 5 / 101 | 토양 마름 AND 강풍 반대 AND 표면 마름 | 0 |
| 6 / 110 | 토양 마름 AND 강풍 AND 표면 마름 반대 | 0 |
| 7 / 111 | 토양 마름 AND 강풍 AND 표면 마름 | 1 |

기본은7번만 결과1이라 세 소속도의 곱과 같아요. 곱셈 자체가 새로운 현상은 아니에요.
이전 건조도 제곱식을 명시적인 소속 기준과 규칙표로 바꾸고, 다른 조합의 결과도 DA에서 바꿀 수 있게 했어요.

토양 건조 비율.5는 `(.5−.2)/(.8−.2)=.5`가 돼요.
풍속8.5m/s는 기존5~12m/s 기준에서.5가 돼요.
표면 마름도.5면 모래폭풍 목표는.125예요.

## 10. WeatherSnapshot.cpp — 방 상태에서 입력을 만드는 함수

```cpp
inputs.pressureDeficitHpa = profile_.meanPressureHpa - pressureHpa_;
inputs.surfaceCoolingC = nearAir_.temperatureC - ground_.temperatureC;
inputs.soilDryness = 1 - clamp01(
    (ground_.waterKgM2 + ground_.soilKgM2) / p.soilCapacityKgM2);
inputs.surfaceDryness = 1 - clamp01(
    (ground_.waterKgM2 + ground_.filmKgM2) / p.surfaceFilmCapacityKgM2);
inputs.dustWind = clamp01((windSpeedMps_ - p.dustStartWindMps)
                         / (p.dustFullWindMps - p.dustStartWindMps));
```

이 코드는 `InstanceWeather::fuzzyWeatherInputs()`에 있어요.
`profile_`은 방 설정, `nearAir_`는 저층 공기, `ground_`는 지표 저장 상태예요.
별도로 새로운 날씨를 만들지 않고 기존 방 상태를 읽어요.

- 평균1013hPa, 현재1007hPa면 기압 부족분은6hPa예요.
- 공기10°C, 지표8°C면 냉각 차이는2°C예요. 지표가 더 따뜻하면 음수이고 냉각 소속도는0이에요.
- 토양 용량20kg/m²에 물10kg/m²면 건조 비율은.5예요.
- 물막 용량.2kg/m²에 표면 물.1kg/m²면 표면 마름은.5예요.
- 풍속8.5m/s는 `(8.5−5)/(12−5)=.5`예요.

상대 습도는 각 대기층의 실제 수증기량을 포화 수증기량으로 나눠100을 곱해요.
분모의 작은 하한은0으로 나누는 것을 막아요. 이것은 퍼지 규칙과는 별도의 입력 변환이에요.

`snapshot()`은 현재 입력으로 눈 비중과 안개를 구해 `EnvironmentState`에 넣어요.
이 결과는 상태를 읽어 계산하는 값이라 새로운 저장소를 만들거나 물의 양을 바꾸지 않아요.

## 11. SurfaceWaterCycle.cpp — 강수에 실제로 사용하는 부분

```cpp
const auto fuzzy = evaluateFuzzyWeather(
    profile_.environment.fuzzyWeather, fuzzyWeatherInputs());
const double cloudExcess = std::max(0.0, upperAir_.liquidKgM2 - 0.05);
const double falling = cloudExcess * (-std::expm1(-dt / 600.0))
                       * fuzzy.precipitationStrength;
const double snowFraction = fuzzy.snowFraction;
```

`cloudExcess`는 내릴 수 있는 상층 구름물이에요.
`dt`는 고정 계산 단계의 시뮬레이션 경과 초예요. 기존값10초를 사용해요.
`expm1` 부분은 구름물이 한 번에 전부 떨어지지 않게 하는 기존 시간 비율이에요.
이번에는 여기에 퍼지가 정한 강수 배율을 곱해요.

다음 기존 `transfer()`가 `falling × snowFraction`을 눈 저장소로,
`falling × (1−snowFraction)`을 지표 물 저장소로 옮겨요.
`transfer()`는 원래 저장된 물보다 많이 옮기지 않아요.
퍼지는 강수의 반응을 정하고, 기존 코드는 그 결과를 젖음·적설 상태에 연결해요.

## 12. DesertEnvironment.cpp — 강도를 천천히 바꾸는 부분

```cpp
const auto fuzzy = evaluateFuzzyWeather(p.fuzzyWeather, fuzzyWeatherInputs());
const double cover = 1 - earth::clamp01(ground_.snowKgM2 + ground_.iceKgM2);
const double target = p.sandAvailability * fuzzy.sandstormStrength * cover;
environment_.sandstormIntensity = earth::relax(
    environment_.sandstormIntensity, target, dt, p.dustResponseSeconds);
```

- `p.sandAvailability`: 그 지역에서 날릴 모래의 상대량이에요.0이면 모래폭풍이 없어요.
- `cover`: 눈·얼음 덮임이 적을수록1에 가까워요. 기존 방 평균 제한을 사용해요.
- `target`: 모래량과 덮임 제한까지 반영한 목표 강도예요.
- `sandstormIntensity`: 지금 실제로 보내는 강도예요.
- `relax`: 현재 강도가 목표로 서서히 다가가게 하는 기존 함수예요.
- `dustResponseSeconds`: 기본120시뮬레이션 초예요. 약120초 뒤 목표까지 차이의 약63%를 이동해요.

따라서 목표가0에서1로 바뀌어도 모래폭풍이 즉시 최대가 되지는 않아요.
규칙 판단과 시간 변화는 분리되어 있어요. 화면에서도 기존 보간을 계속 사용해요.

## 13. 검증 함수 두 개 — 잘못된 기준 처리

```cpp
bool validFuzzyWeatherProfile(const FuzzyWeatherProfile& profile);
void normalizeFuzzyWeatherProfile(FuzzyWeatherProfile& profile);
```

`validFuzzyWeatherProfile()`은 습도·온도·풍속 범위와 시작<완료 관계를 확인해요.
세 규칙 배열의 결과가 유한한0~1인지도 확인해요.
`range()`는 두 기준의 순서를, `number()`는 값 하나의 허용 범위를 검사하는 내부 보조 함수예요.

서버 ini는 잘못된 설정이면 예외로 거절해요. 오타·중복키·누락된 규칙 값도 거절해요.
Yang2의 DA 변환은 잘못된 값이면 경고를 남기고 기본 퍼지 설정으로 바꿔요.
기준 일부만 바꾸면 상호 관계가 깨질 수 있어, 퍼지 설정 묶음 전체를 기본으로 되돌려요.
유효한 설정은 그대로 사용해요.

## 14. DA에서 무엇을 바꾸는지

새 폴더나 새로운 DA를 만들지 않았어요.
기존 `/Game/VFX/Weather/DA_Environment_Temperate`, `DA_Environment_Coast`, `DA_Environment_Desert`의
**Fuzzy Weather** 항목에 설정이 추가되어 있어요.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Fuzzy")
FUEFuzzyWeatherProfile FuzzyWeather;
```

`UEEnvironmentProfile.h`의 이 변수는 지역 환경 DA 안에 퍼지 설정을 담아요.
구조체 내부의 항목은 `UEFuzzyWeatherProfile.h`에 있고, 서버의 같은 이름 변수와 대응해요.

예를 들어 강수5번 규칙을 바꾸려면 `Rain Rule Outputs`의 인덱스5를 바꿔요.
`.35 → .8`로 높이면 습하고 구름이 있으면서 저기압 소속도가 낮을 때 강수 배율이 커져요.
모든 규칙의 결과는0~1, 규칙 배열은 각각8개예요. 배열 크기는 에디터에서 고정해요.

- 강풍 시 안개를 더 줄이고 싶으면 `FogRuleOutputs[4]`, `[5]`를 줄여요.
- 습한 토양에서도 작은 먼지를 허용하려면 `DustRuleOutputs[3]`을 조금 올려요.
- 더 추운 곳에서만 눈을 내리고 싶으면 눈/비 온도 경계를 함께 낮춰요.
- 낮은 풍속부터 모래가 날리게 하려면 기존 `DustStartWindMps`를 줄여요.

main 서버는 uasset을 직접 읽지 않아요.
DA 편집 후 기존 `export_environment_profiles.py`로 내보내고 서버 설정에 반영해야 해요.
내보낼 때도 잘못된 기준은 파일을 쓰기 전에 거절해요.
Yang2는 입장할 때 DA를 직접 읽어요. 진행 중인 방에 실시간 재적용하는 기능은 만들지 않았어요.

ini의 예시는 다음과 같아요.

```ini
fuzzy.humidStartPct=65
fuzzy.humidFullPct=95
fuzzy.rainRuleOutputs=0,0,0,0,0,0.35,0,1
fuzzy.fogRuleOutputs=0,0,0,0,0.05,0.15,0.65,1
fuzzy.dustRuleOutputs=0,0,0,0,0,0,0,1
```

`EnvironmentConfig.cpp`는 스칼라 키를 해당 변수에 넣고,
규칙 배열 키는 정확히8개의 숫자를 읽어서 `std::array`에 넣어요.
기존 ini에 새 키가 없으면 C++의 기본 퍼지 설정을 사용해요.

## 15. main의 네트워크와 Yang2의 차이

서버 `EnvironmentState`와 클라이언트 `FUEEnvironmentState`에 다음 세 값이 추가됐어요.

```cpp
bool fuzzyWeatherEnabled = false;
double snowFraction = 0;
double fogDensity = 0;
```

`fuzzyWeatherEnabled`는 이번 규칙 결과가 들어있는지 알려줘요.
값이 없는 구버전 서버 패킷은 false로 읽히므로 이전 화면 규칙을 사용할 수 있어요.
새 서버의 결과는 true라서 클라이언트가 눈·안개를 다른 기준으로 재판단하지 않아요.

`field.fbs`의 기존 필드 뒤에 추가했어요. `FieldCodec.h`가 서버값을 쓰고
`HHVFieldConnection.cpp`가 읽어요. 생성 헤더는 flatc로 갱신했어요.
형식의 읽기 호환은 유지하지만, 구버전 클라이언트는 새 안개 규칙을 보여 주지 못해요.
새 결과를 동일하게 보려면 서버와 클라이언트를 함께 갱신해요.

```cpp
const float RainFraction = Weather.Environment.FuzzyWeatherEnabled
    ? 1.0f - FMath::Clamp(static_cast<float>(Weather.Environment.SnowFraction), 0.0f, 1.0f)
    : FMath::Clamp((Weather.TemperatureC - FullSnowTemperatureC) / TemperatureRange, 0.0f, 1.0f);
```

이 부분은 `UEInstanceWeatherPresentationComponent.cpp`에 있어요.
새 패킷이면 서버 눈 비중을 그대로 사용하고, 구버전 패킷이면 기존 온도 규칙으로 돌아가요.
안개도 같은 방식이에요. 비·눈의 화면 강도는 기존 강수량 정규화와 결합해요.
BP의 기존 `FullSnowTemperatureC`, `FullRainTemperatureC`, 안개 습도 항목은 구버전용이에요.
새 규칙은 환경 DA의 `FuzzyWeather`에서 조절해야 해요.

Yang2의 `ApplyYang2EnvironmentProfile()`은 이 DA 설정을 같은 서버 C++ 구조체에 복사해요.
`MakeYang2EnvironmentState()`는 계산 결과의 세 필드를 공통 화면 상태에 옮겨요.
`UEYang2InstanceWeatherImplementation.cpp`는 새 `FuzzyWeather.cpp`도 포함해요.
로컬에서 규칙을 다시 구현한 복사본을 만들지 않았어요.

## 16. 저장·복구와 기존 기능

안개와 눈 비중은 복구된 기온·습도·지표 상태로 다시 구해요. 별도 입자 위치를 저장하지 않아요.
기존 `CLIMATE1` 저장 형식은 유지해요.
기본 퍼지 설정은 기존 설정 키를 유지하므로 기존 기본 기후 저장 상태를 이어갈 수 있어요.
이어서 진행할 때부터 새 강수·먼지 규칙을 적용해요.
사용자가 퍼지 기준이나 규칙을 바꾸면 설정 키에 그 값도 포함해요.
다른 설정으로 저장된 상태를 잘못 복구하는 경우는 기존 절차대로 거절하고 파일을 지우지 않아요.

기존 야생 등장 가중치는 새로운 눈 비중을 읽도록 맞췄어요.
새 등장 규칙이나 전투 기능을 추가한 것은 아니에요. 날씨 화면과 기존 적설/눈 판정의 기준을 맞춘 변경이에요.

## 17. 계산 비용과 한계

규칙8개 × 조건3개로 계산해요. 사용하는 컨테이너는 `std::array`라 규칙 계산 중 힙 할당이 없어요.
입자 하나씩 계산하거나 플레이어 수만큼 같은 날씨를 다시 계산하지 않아요.
고정 단계의 강수/먼지 처리와 기존 스냅샷 생성 때 방 평균 상태를 읽어요.
실제 다중 방 서버의 처리량이나 멀티플레이 부하는 이번에 측정하지 않았어요.

이 규칙은 수증기 이동의 공간 셀, 구름의 실제 이동 위치, 눈의 실제 쌓인 형상,
지형마다 다른 지역 기후를 새로 만들지 않아요.
기존 VFX·머티리얼의 외형도 이번에 제작하거나 바꾸지 않았어요.
규칙을 추가했다고 현실성이 자동으로 보장되지는 않아요. 원하는 환경 반응에 맞게 DA를 조절해야 해요.

## 18. 주인님이 실행할 수 있는 확인 코드

기존 `Server/tests/InstanceEnvironmentTests.cpp`에 숫자 계산 검사를 추가했어요.
위에서 설명한 균등.5 소속도 사례의 강수.16875, 안개.23125, 먼지.125를 검사해요.
완전 조건/차단 조건/사용자 규칙 변경/잘못된 ini/복구 설정 차이도 확인하게 해놓았어요.
클라이언트 기존 `InstanceWeatherTests.cpp`에는 서버의 눈·안개 결과를 화면이 우선 사용하는 검사를 추가했어요.

이번 작업에서는 게임 플레이와 자동 검사를 실행하지 않았어요. 컴파일·링크와 에디터에서 DA 연결을 확인한 범위만 보고해요.

## 참고한 개념

- [MathWorks: Fuzzy Inference Process](https://www.mathworks.com/help/fuzzy/fuzzy-inference-process.html)
- [MathWorks: Mamdani and Sugeno Fuzzy Inference Systems](https://www.mathworks.com/help/fuzzy/types-of-fuzzy-inference-systems.html)

개념을 참고해 우리 게임의 작은 C++ 규칙 계산기를 작성했어요. MATLAB이나 추가 퍼지 라이브러리를 설치하거나 사용하지 않았어요.
