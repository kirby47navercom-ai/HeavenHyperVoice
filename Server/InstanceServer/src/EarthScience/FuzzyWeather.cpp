#include "FuzzyWeather.h"
#include "WeatherMath.h"

namespace heaven::instance {
namespace {
// 시작 이하=0, 완료 이상=1, 그 사이를 직선으로 연결하는 소속 함수예요.
double rising(double value, double start, double full) {
    return earth::clamp01((value - start) / (full - start));
}
bool range(double start, double full, double low, double high) {
    return std::isfinite(start) && std::isfinite(full) && start >= low && full <= high && start < full;
}
bool number(double value, double low, double high) {
    return std::isfinite(value) && value >= low && value <= high;
}
// 0차 Sugeno 추론이에요. AND는 곱, NOT은 1-소속도, 결과는 규칙 결과의 가중 평균이에요.
double infer(const std::array<double, 3>& membership, const std::array<double, 8>& outputs,
             std::array<double, 8>& activations) {
    double weighted = 0, total = 0;
    for (std::size_t rule = 0; rule < outputs.size(); ++rule) {
        double activation = 1;
        for (std::size_t term = 0; term < membership.size(); ++term) {
            // 첫 조건=4, 두 번째=2, 세 번째=1 비트예요. 0 비트는 반대 조건이에요.
            activation *= (rule & (4u >> term)) ? membership[term] : 1 - membership[term];
        }
        activations[rule] = activation;
        weighted += activation * outputs[rule];
        total += activation;
    }
    // 보완 관계인 두 집합의 모든 조합을 포함해서 total은 항상 1이에요.
    return total > 0 ? earth::clamp01(weighted / total) : 0;
}
}

bool validFuzzyWeatherProfile(const FuzzyWeatherProfile& p) {
    if (!range(p.humidStartPct, p.humidFullPct, 0, 100) ||
        !range(p.fogHumidStartPct, p.fogHumidFullPct, 0, 100) ||
        !range(p.fullSnowTemperatureC, p.fullRainTemperatureC, -60, 60) ||
        !range(p.fogCalmStartMps, p.fogCalmEndMps, 0, 200) ||
        !range(p.soilDryStart, p.soilDryFull, 0, 1) ||
        !number(p.lowPressureFullDeficitHpa, .01, 100) ||
        !number(p.cloudFullCover, .001, 1) || !number(p.fogCoolingFullC, .01, 60)) return false;
    for (const auto* rules : {&p.rainRuleOutputs, &p.fogRuleOutputs, &p.dustRuleOutputs})
        for (double output : *rules) if (!number(output, 0, 1)) return false;
    return true;
}
void normalizeFuzzyWeatherProfile(FuzzyWeatherProfile& profile) {
    if (!validFuzzyWeatherProfile(profile)) profile = FuzzyWeatherProfile{};
}

FuzzyWeatherResult evaluateFuzzyWeather(const FuzzyWeatherProfile& p, const FuzzyWeatherInputs& in) {
    FuzzyWeatherResult result;
    // 8개 강수 규칙: 습함 / 저기압 / 구름 많음의 참·반대 조합이에요.
    result.precipitationStrength = infer({
        rising(in.upperHumidityPct, p.humidStartPct, p.humidFullPct),
        rising(in.pressureDeficitHpa, 0, p.lowPressureFullDeficitHpa),
        rising(in.cloudCover, 0, p.cloudFullCover)}, p.rainRuleOutputs, result.rainActivations);
    // 두 눈/비 규칙: 추움→눈 1, 따뜻함→눈 0. 두 소속도의 합은 1이에요.
    const double warm = rising(in.temperatureC, p.fullSnowTemperatureC, p.fullRainTemperatureC);
    result.snowFraction = (1 - warm) * 1 + warm * 0;
    // 8개 안개 규칙: 저층 습함 / 바람 잔잔함 / 지표가 차가움이에요.
    result.fogDensity = infer({
        rising(in.nearHumidityPct, p.fogHumidStartPct, p.fogHumidFullPct),
        1 - rising(in.windSpeedMps, p.fogCalmStartMps, p.fogCalmEndMps),
        rising(in.surfaceCoolingC, 0, p.fogCoolingFullC)}, p.fogRuleOutputs, result.fogActivations);
    // 8개 먼지 규칙: 토양 건조 / 강풍 / 표면 건조. 모래량/눈 덮임은 지역 제한이에요.
    result.sandstormStrength = infer({
        rising(in.soilDryness, p.soilDryStart, p.soilDryFull),
        earth::clamp01(in.dustWind), earth::clamp01(in.surfaceDryness)},
        p.dustRuleOutputs, result.dustActivations);
    return result;
}
}
