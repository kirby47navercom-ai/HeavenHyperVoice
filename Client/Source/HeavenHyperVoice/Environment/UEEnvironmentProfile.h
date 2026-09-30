#pragma once
#include "UEEnvironmentWaterRegion.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UEEnvironmentSpawnRule.h"
#include "UEEnvironmentProfile.generated.h"

/** 지역 환경 원본. 서버에는 설정 파일로 내보내고 Yang2는 로컬에서 읽는다. */
UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUEEnvironmentProfile : public UDataAsset {
    GENERATED_BODY()
public:
    // 비/눈/밤에 따른 종족 가중치. 빈 배열은 기존 종족 확률을 유지해요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Gameplay") TArray<FUEEnvironmentSpawnRule> SpawnRules;
    // 지역 평균 기온. 계절, 고도, 낮밤 변화의 기준이에요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climate", meta=(ClampMin="-60",ClampMax="60")) double MeanTemperatureC=18;
    // 방 생성 시 상대 습도. 이후 물순환 계산으로 달라져요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climate", meta=(ClampMin="0",ClampMax="100")) double InitialRelativeHumidityPct=72;
    // 기단 변화가 더해지는 기준 기압이에요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climate", meta=(ClampMin="800",ClampMax="1100")) double MeanPressureHpa=1013.25;
    // 처음 지표에 저장된 물이에요. 1 kg/m² = 1 mm예요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climate", meta=(ClampMin="0",ClampMax="1000")) double InitialSurfaceWaterKgM2=2;
    // 처음 흙에 저장된 물이에요. 토양 용량 이하로 설정해요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climate", meta=(ClampMin="0",ClampMax="10000")) double InitialSoilWaterKgM2=12;
    // 현실 1초에 진행할 시뮬레이션 초. 0이면 정지해요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climate", meta=(ClampMin="0",ClampMax="3600")) double GameSecondsPerRealSecond=60;
    // 시뮬레이션 하루 길이. 현실 속도는 gameSecondsPerRealSecond가 결정한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="60",ClampMax="864000")) double DaySeconds=86400;
    // 게임 내 1년의 일수
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="1",ClampMax="1000")) double YearDays=120;
    // 공용 시계 시작 시각
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0",ClampMax="24")) double StartHour=9;
    // 0~1, 북반구 봄 부근부터 시작
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0",ClampMax="1")) double StartYearFraction=0.25;

    // 위도. 북반구는 양수, 남반구는 음수이며 낮 길이와 계절이 달라진다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="-85",ClampMax="85")) double LatitudeDegrees=35;
    // 해안/호수의 실제 구역. 물이 없는 도시는 비워 두세요. 슬롯 순서대로 겹침을 판정해요.
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Environment|Water") TArray<FUEEnvironmentWaterRegion> WaterRegions;
    // 기후 기준점보다 높은 정도
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="-500",ClampMax="9000")) double AltitudeM=0;
    // 계절에 따른 평균 기온 변동 폭
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0",ClampMax="40")) double SeasonalAmplitudeC=8;
    // 낮밤에 따른 기온 변동 폭
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0",ClampMax="40")) double DailyAmplitudeC=5;
    // 지표 열용량의 반응 시간. 물은 크게, 모래는 작게 설정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="1",ClampMax="864000")) double ThermalResponseSeconds=3600;

    // 흙이 저장할 수 있는 물의 최대량. 포화되면 추가 침투가 멈춘다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0.001",ClampMax="10000")) double SoilCapacityKgM2=20;

    // 중력 배수 뒤에도 흙에 남는 물의 기준량.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0",ClampMax="10000")) double FieldCapacityKgM2=12;

    // 표면에서 흙으로 들어갈 수 있는 초당 물의 최대량.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0",ClampMax="10")) double InfiltrationKgM2PerSecond=0.0001;

    // 토양의 초과 수분이 빠지는 시간. 클수록 천천히 마른다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="1",ClampMax="86400000")) double DrainageSeconds=86400;

    // 기본 풍속. 기압 변화와 돌풍이 여기에 더해진다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0",ClampMax="100")) double BaseWindMps=2;

    // 돌풍으로 추가되는 풍속의 최대 폭.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0",ClampMax="100")) double GustAmplitudeMps=3;
    // 0이면 사막 효과 없음. 입자 위치가 아니라 날릴 모래의 상대량
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0",ClampMax="1")) double SandAvailability=0;

    // 마른 모래가 날리기 시작하는 풍속.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0",ClampMax="100")) double DustStartWindMps=5;

    // 모래가 충분하고 마른 땅에서 최대 폭풍에 도달하는 풍속.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0.1",ClampMax="200")) double DustFullWindMps=12;

    // 모래폭풍이 목표 강도에 서서히 도달하는 시간.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="1",ClampMax="86400")) double DustResponseSeconds=120;
    // 0이면 조석 변화 없음. 파도는 독립적
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0",ClampMax="20")) double TideAmplitudeM=0;

    // 밀물에서 다음 밀물까지의 시뮬레이션 시간.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0.01",ClampMax="1000")) double TidePeriodHours=12.42;

    // 입장 시 조석 위상. 다른 해안의 물때를 다르게 설정한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0",ClampMax="360")) double TidePhaseDegrees=0;

    // 강풍일 때 파도 높이의 상한. 파고이며 진폭의 두 배다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="0",ClampMax="20")) double WaveMaxHeightM=0.8;

    // 바람이 바뀐 뒤 파고가 따라가는 시간.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment", meta=(ClampMin="1",ClampMax="86400")) double WaveResponseSeconds=180;
    // 표면에 붙어 남는 얇은 물막 용량. 흙 속 수분과 화면 젖음을 분리해요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="0.001",ClampMax="10")) double SurfaceFilmCapacityKgM2=0.2;
    // 주변 기단과 수증기를 교환하는 시간. 길수록 방의 날씨가 오래 유지돼요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="60",ClampMax="8640000")) double MoistureExchangeSeconds=21600;
    // 맑은 하늘에서 지표에 도달하는 최대 일사 W/m²예요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="0",ClampMax="1400")) double SolarPeakWm2=500;
    // 햇빛 반사율. 눈이나 밝은 모래는 크게 설정해요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="0",ClampMax="1")) double SurfaceAlbedo=0.22;
    // 지표의 장파 복사 방출률이에요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="0.01",ClampMax="1")) double SurfaceEmissivity=0.95;
    // 맑은 하늘로 빠지는 장파 복사 W/m². 구름이 많으면 줄어요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="0",ClampMax="200")) double ClearSkyCoolingWm2=45;
    // 지표와 대기의 온도차 1도당 열 교환 W/m²예요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="1",ClampMax="200")) double AirHeatTransferWm2K=25;
    // 사리에서 다음 사리까지 게임 일수. 천문 예보 대신 조석 크기 주기를 표현해요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="0.1",ClampMax="1000")) double SpringNeapPeriodDays=14.765;
    // 조금 때 사리 대비 조석 진폭 비율이에요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="0",ClampMax="1")) double NeapTideFraction=0.5;
    // 공용 시계 시작 시 사리/조금 위상이에요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="0",ClampMax="360")) double SpringNeapPhaseDegrees=0;
    // 기준 해수면 대비 관찰할 해변 높이. 해당 지점의 잠김 깊이를 서버가 계산해요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="-100",ClampMax="100")) double ShoreHeightM=0;
    // 젖은 지표에서 야생 이동 속도 배율. 1이면 영향이 없어요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="0.1",ClampMax="1")) double WetMovementMultiplier=0.95;
    // 얼음 지표에서 야생 이동 속도 배율이에요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="0.1",ClampMax="1")) double IceMovementMultiplier=0.85;
    // 최대 모래폭풍에서 야생 탐지 범위 배율이에요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Advanced", meta=(ClampMin="0.1",ClampMax="1")) double SandVisibilityMultiplier=0.55;
    // 얼음에서 가속/제동/마찰의 배율. 작을수록 방향 전환과 멈춤이 늦어요.
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Environment|Gameplay",meta=(ClampMin="0.02",ClampMax="1")) double IceTractionMultiplier=0.12;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Environment|Gameplay",meta=(ClampMin="10",ClampMax="1000")) double SwimSpeedCmPerSecond=160;
    // 쓰러지거나 포획된 야생 슬롯을 다시 채우기까지 현실 초예요.
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Environment|Gameplay",meta=(ClampMin="1",ClampMax="86400")) double WildRespawnSeconds=30;
};
