#pragma once

namespace heaven::instance {
// 방/지역의 환경 설정. 단위가 붙은 값을 서버 설정 파일과 에디터 데이터 에셋에서 공유한다.
// 태양과 조석은 게임용 주기 모델이며 특정 날짜/지역의 천문 예보가 아니다.
struct EnvironmentProfile {
    double daySeconds = 86400;       // 시뮬레이션 하루 길이. 현실 속도는 gameSecondsPerRealSecond가 결정한다.
    double yearDays = 120;           // 게임 내 1년의 일수
    double startHour = 9;            // 입장 시각
    double startYearFraction = 0.25; // 0~1, 북반구 봄 부근부터 시작
    // 위도. 북반구는 양수, 남반구는 음수이며 낮 길이와 계절이 달라진다.
    double latitudeDegrees = 35;
    double altitudeM = 0;           // 기후 기준점보다 높은 정도
    double seasonalAmplitudeC = 8;  // 계절에 따른 평균 기온 변동 폭
    double dailyAmplitudeC = 5;     // 낮밤에 따른 기온 변동 폭
    double thermalResponseSeconds = 3600; // 지표가 목표 온도를 따라가는 시간. 물은 크게, 모래는 작게 설정
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
    double tideAmplitudeM = 0;      // 0이면 해안이 아닌 인스턴스
    // 밀물에서 다음 밀물까지의 시뮬레이션 시간.
    double tidePeriodHours = 12.42;
    // 입장 시 조석 위상. 다른 해안의 물때를 다르게 설정한다.
    double tidePhaseDegrees = 0;
    // 강풍일 때 파도 높이의 상한. 파고이며 진폭의 두 배다.
    double waveMaxHeightM = 0.8;
    // 바람이 바뀐 뒤 파고가 따라가는 시간.
    double waveResponseSeconds = 180;
};
// 공통 모델에서도 유효 범위를 보장하여 BP의 잘못된 입력으로 계산이 깨지지 않게 한다.
void normalizeEnvironment(EnvironmentProfile& profile);
}
