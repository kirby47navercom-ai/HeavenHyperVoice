#pragma once
#include "MovementEnvironment.h"
#include "FuzzyWeather.h"

namespace heaven::instance {
// 방/지역의 환경 설정. 단위가 붙은 값을 서버 설정 파일과 에디터 데이터 에셋에서 공유한다.
// 태양과 조석은 게임용 주기 모델이며 특정 날짜/지역의 천문 예보가 아니다.
struct EnvironmentProfile {
    FuzzyWeatherProfile fuzzyWeather; // 비·눈·안개·모래폭풍을 판단하는 규칙 설정이에요.
    std::vector<hhv::movement::WaterRegion> waterRegions;
    double iceTractionMultiplier=.12; // 얼음의 가속/제동/마찰 배율이에요.
    double swimSpeedCmPerSecond=160;
    double wildRespawnSeconds=30; // 리스폰 때의 날씨로 가중치를 다시 계산해요.
    double daySeconds = 86400;       // 시뮬레이션 하루 길이. 현실 속도는 gameSecondsPerRealSecond가 결정한다.
    double yearDays = 120;           // 게임 내 1년의 일수
    double startHour = 9;            // 공용 시계 시작 시각
    double startYearFraction = 0.25; // 0~1, 북반구 봄 부근부터 시작
    // 위도. 북반구는 양수, 남반구는 음수이며 낮 길이와 계절이 달라진다.
    double latitudeDegrees = 35;
    double altitudeM = 0;           // 기후 기준점보다 높은 정도
    double seasonalAmplitudeC = 8;  // 계절에 따른 평균 기온 변동 폭
    double dailyAmplitudeC = 5;     // 낮밤에 따른 기온 변동 폭
    double thermalResponseSeconds = 3600; // 지표 열용량의 반응 시간. 물은 크게, 모래는 작게 설정
    // 흙이 저장할 수 있는 물의 최대량. 포화되면 추가 침투가 멈춘다.
    double soilCapacityKgM2 = 20;
    // 중력 배수 뒤에도 흙에 남는 물의 기준량.
    double fieldCapacityKgM2 = 12;
    // 표면에서 흙으로 들어갈 수 있는 초당 물의 최대량.
    double infiltrationKgM2PerSecond = 0.0001;
    // 토양의 초과 수분이 빠지는 시간. 클수록 천천히 마른다.
    double drainageSeconds = 86400;
    // 기본 풍속. 기압 변화와 돌풍이 여기에 더해진다.
    double baseWindMps = 2;
    // 돌풍으로 추가되는 풍속의 최대 폭.
    double gustAmplitudeMps = 3;
    double sandAvailability = 0;    // 0이면 사막 효과 없음. 입자 위치가 아니라 날릴 모래의 상대량
    // 마른 모래가 날리기 시작하는 풍속.
    double dustStartWindMps = 5;
    // 모래가 충분하고 마른 땅에서 최대 폭풍에 도달하는 풍속.
    double dustFullWindMps = 12;
    // 모래폭풍이 목표 강도에 서서히 도달하는 시간.
    double dustResponseSeconds = 120;
    double tideAmplitudeM = 0;      // 0이면 조석 변화 없음. 파도는 독립적
    // 밀물에서 다음 밀물까지의 시뮬레이션 시간.
    double tidePeriodHours = 12.42;
    // 입장 시 조석 위상. 다른 해안의 물때를 다르게 설정한다.
    double tidePhaseDegrees = 0;
    // 강풍일 때 파도 높이의 상한. 파고이며 진폭의 두 배다.
    double waveMaxHeightM = 0.8;
    // 바람이 바뀐 뒤 파고가 따라가는 시간.
    double waveResponseSeconds = 180;
    // 표면에 붙어 남는 얇은 물막 용량. 흙 속 수분과 화면 젖음을 분리해요.
    double surfaceFilmCapacityKgM2 = 0.2;
    // 주변 기단과 수증기를 교환하는 시간. 길수록 방의 날씨가 오래 유지돼요.
    double moistureExchangeSeconds = 21600;
    // 맑은 하늘에서 지표에 도달하는 최대 일사 W/m²예요.
    double solarPeakWm2 = 500;
    // 햇빛 반사율. 눈이나 밝은 모래는 크게 설정해요.
    double surfaceAlbedo = 0.22;
    // 지표의 장파 복사 방출률이에요.
    double surfaceEmissivity = 0.95;
    // 맑은 하늘로 빠지는 장파 복사 W/m². 구름이 많으면 줄어요.
    double clearSkyCoolingWm2 = 45;
    // 지표와 대기의 온도차 1도당 열 교환 W/m²예요.
    double airHeatTransferWm2K = 25;
    // 사리에서 다음 사리까지 게임 일수. 천문 예보 대신 조석 크기 주기를 표현해요.
    double springNeapPeriodDays = 14.765;
    // 조금 때 사리 대비 조석 진폭 비율이에요.
    double neapTideFraction = 0.5;
    // 공용 시계 시작 시 사리/조금 위상이에요.
    double springNeapPhaseDegrees = 0;
    // 기준 해수면 대비 관찰할 해변 높이. 해당 지점의 잠김 깊이를 서버가 계산해요.
    double shoreHeightM = 0;
    // 젖은 지표에서 야생 이동 속도 배율. 1이면 영향이 없어요.
    double wetMovementMultiplier = 0.95;
    // 얼음 지표에서 야생 이동 속도 배율이에요.
    double iceMovementMultiplier = 0.85;
    // 최대 모래폭풍에서 야생 탐지 범위 배율이에요.
    double sandVisibilityMultiplier = 0.55;
};
// 공통 모델에서도 유효 범위를 보장하여 BP의 잘못된 입력으로 계산이 깨지지 않게 한다.
void normalizeEnvironment(EnvironmentProfile& profile);
}
