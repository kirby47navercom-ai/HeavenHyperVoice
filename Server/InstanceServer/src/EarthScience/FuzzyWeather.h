#pragma once
#include <array>

namespace heaven::instance {
// 데이터 에셋/서버 ini에서 조절하는 기준이에요. 소속도는 확률이 아니에요.
struct FuzzyWeatherProfile {
    double humidStartPct = 65, humidFullPct = 95;
    double fogHumidStartPct = 85, fogHumidFullPct = 100;
    double lowPressureFullDeficitHpa = 12; // 지역 평균보다 얼마나 낮은 기압인지예요.
    double cloudFullCover = .7;
    double fullSnowTemperatureC = -1, fullRainTemperatureC = 1;
    double fogCoolingFullC = 4; // 지표가 공기보다 차가운 정도예요.
    double fogCalmStartMps = .5, fogCalmEndMps = 5;
    double soilDryStart = .2, soilDryFull = .8;
    // 000~111 순서의 8개 규칙 결과예요. 각 비트의 뜻은 아래 입력 주석과 안내서를 봐요.
    std::array<double, 8> rainRuleOutputs{0, 0, 0, 0, 0, .35, 0, 1};
    std::array<double, 8> fogRuleOutputs{0, 0, 0, 0, .05, .15, .65, 1};
    std::array<double, 8> dustRuleOutputs{0, 0, 0, 0, 0, 0, 0, 1};
};

struct FuzzyWeatherInputs {
    double temperatureC = 0;
    double upperHumidityPct = 0; // 강수 구름이 있는 상층 습도예요.
    double nearHumidityPct = 0;  // 플레이어가 있는 저층의 안개 조건이에요.
    double pressureDeficitHpa = 0;
    double cloudCover = 0;
    double surfaceCoolingC = 0;
    double windSpeedMps = 0;
    double soilDryness = 0, surfaceDryness = 0;
    double dustWind = 0; // 기존 DustStart/FullWindMps로 정규화한 강풍 소속도예요.
};

struct FuzzyWeatherResult {
    double precipitationStrength = 0; // 구름물이 내려오는 비율의 배율이에요.
    double snowFraction = 0; // 강수 중 눈의 비중. 비의 비중은 1-snowFraction이에요.
    double fogDensity = 0, sandstormStrength = 0;
    // 규칙이 얼마나 성립했는지 보여 주는 진단값이에요. 개별 입자 위치는 계산하지 않아요.
    std::array<double, 8> rainActivations{}, fogActivations{}, dustActivations{};
};

bool validFuzzyWeatherProfile(const FuzzyWeatherProfile& profile);
void normalizeFuzzyWeatherProfile(FuzzyWeatherProfile& profile);
// 설정 검증/정규화가 끝난 방 상태를 받아요. 현재 입력은 모두 유한한 값이어야 해요.
FuzzyWeatherResult evaluateFuzzyWeather(const FuzzyWeatherProfile& profile,
                                      const FuzzyWeatherInputs& inputs);
}
