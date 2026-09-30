#pragma once
namespace heaven::instance {
// 서버가 확정한 환경 상태. 클라이언트는 이것을 표시할 뿐 main에서는 다시 날씨를 계산하지 않는다.
struct EnvironmentState {
    double dayFraction = 0;      // 자정=0, 정오=0.5
    double yearFraction = 0;
    double sunElevationDegrees = 0;
    double sunAzimuthDegrees = 0;
    double tideLevelM = 0;      // BP에서 지정한 해수면 기준 높이에 더하는 상대 수위
    double waveHeightM = 0;
    double sandstormIntensity = 0;
    double groundTemperatureC = 0;
    double surfaceWaterMm = 0;
    double iceMm = 0;
    double soilMoisture = 0;
    double timeScale = 0;
    double surfaceHeatFluxWm2 = 0; // 지표의 순 열 유입 W/m²
    double importedWaterKgM2 = 0; // 외부에서 들어온 누적 수분
    double exportedWaterKgM2 = 0; // 외부로 빠져나간 누적 수분. 토양 배수는 별도 물 수지에 포함
    double tideEnvelopeM = 0; // 현재 사리/조금의 조석 진폭
    double shoreWaterDepthM = 0; // 프로필의 해변 기준점에 물이 덮인 깊이
    double movementMultiplier = 1; // 서버가 확정한 야생 이동 배율
    double visibilityMultiplier = 1; // 서버가 확정한 야생 탐지 배율
};
}
