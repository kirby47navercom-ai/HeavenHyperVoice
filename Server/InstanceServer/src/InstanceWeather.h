#pragma once

// 인스턴스 방 하나가 소유하는 날씨 계산기.
//
// 필드 전체에 날씨 하나를 두는 구조가 아니다. Room 안에 이 객체가 하나씩
// 들어가므로 같은 종류의 던전이라도 room_id 가 다르면 서로 다른 초기 습도와
// 구름을 갖는다. 계산은 RoomManager 가 이미 방에 고정해 둔 틱 스레드에서만
// 실행한다.

#include <cstdint>

namespace heaven::instance {

// 인스턴스 종류별 기후 설정. InstanceType 에 들어가며 새 방을 만들 때 복사한다.
// 단위가 이름에 붙어 있으므로 블루프린트나 설정 파일로 옮길 때도 값의 뜻이
// 흐려지지 않는다.
struct InstanceWeatherProfile {
    double meanTemperatureC = 18.0;
    double initialRelativeHumidityPct = 72.0;
    double meanPressureHpa = 1013.25;
    double initialSurfaceWaterKgM2 = 2.0;
    double initialSoilWaterKgM2 = 12.0;

    // 현실 1초 동안 흐르는 시뮬레이션 초. 낮밤 렌더링과는 연결하지 않는다.
    double gameSecondsPerRealSecond = 60.0;
};

// 서버가 클라이언트에 보내는 방 단위 날씨 결과.
struct InstanceWeatherSnapshot {
    std::uint32_t roomId = 0;
    std::uint32_t revision = 0;
    double simulationTimeSeconds = 0.0;
    float temperatureC = 0.f;
    float relativeHumidityPct = 0.f;
    float pressureHpa = 0.f;
    float cloudCover = 0.f;
    float precipitationMmPerHour = 0.f;
    float windSpeedMps = 0.f;
    float windDirectionDegrees = 0.f;
    float groundWetness = 0.f;
    float snowDepthM = 0.f;
    float waterBalanceErrorKgM2 = 0.f;
};

class InstanceWeather {
  public:
    // roomId 를 씨앗에 섞기 때문에 같은 type 의 복제 던전도 서로 다른 상태로 시작한다.
    void initialize(std::uint32_t type, std::uint32_t roomId,
                    const InstanceWeatherProfile &profile);

    // 현실 경과 시간을 받는다. 내부에서는 10초짜리 시뮬레이션 단계로 잘라
    // 증발→응결→강수→침투를 순서대로 계산한다.
    void advance(double realDeltaSeconds);

    InstanceWeatherSnapshot snapshot() const;

  private:
    // ponytail: 현재는 방 평균 기단 한 칸이다. 플레이어 위치별 날씨가 실제
    // 게임 규칙에 들어올 때만 이 상태를 4x4 또는 8x8 셀로 확장한다.
    struct AirLayer {
        double temperatureC = 0.0;
        double depthM = 100.0;
        double vaporKgM2 = 0.0;
        double liquidKgM2 = 0.0;
    };

    struct Surface {
        double temperatureC = 0.0;
        double waterKgM2 = 0.0;
        double soilKgM2 = 0.0;
        double snowKgM2 = 0.0;
        double iceKgM2 = 0.0;
    };

    double simulateStep(double dt);
    double totalWaterKgM2() const;

    InstanceWeatherProfile profile_;
    std::uint32_t roomId_ = 0;
    std::uint32_t revision_ = 0;
    double simulationTimeSeconds_ = 0.0;
    double pendingSimulationSeconds_ = 0.0;
    double weatherPhase_ = 0.0;

    AirLayer nearAir_;
    AirLayer upperAir_;
    Surface ground_;

    double pressureHpa_ = 1013.25;
    double windSpeedMps_ = 0.0;
    double windDirectionDegrees_ = 0.0;
    double precipitationMmPerHour_ = 0.0;

    double initialWaterKgM2_ = 0.0;
    double drainedWaterKgM2_ = 0.0;
};

} // namespace heaven::instance
