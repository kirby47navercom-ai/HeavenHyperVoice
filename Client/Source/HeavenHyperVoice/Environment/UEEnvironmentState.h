#pragma once
#include "CoreMinimal.h"
#include "UEEnvironmentState.generated.h"

/** 서버의 시간·해안·사막 상태. main 클라이언트는 값을 표시하고 보간한다. */
USTRUCT(BlueprintType)
struct FUEEnvironmentState {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Environment") bool Enabled=false;
    // 자정=0, 정오=0.5
    UPROPERTY(BlueprintReadOnly, Category="Environment") double DayFraction=0;
    // 공전/계절 주기 0~1
    UPROPERTY(BlueprintReadOnly, Category="Environment") double YearFraction=0;
    // 태양 고도 각도. 수평선이 0도
    UPROPERTY(BlueprintReadOnly, Category="Environment") double SunElevationDegrees=0;
    // 북쪽 기준 태양 방위
    UPROPERTY(BlueprintReadOnly, Category="Environment") double SunAzimuthDegrees=0;
    // BP에서 지정한 해수면 기준 높이에 더하는 상대 수위
    UPROPERTY(BlueprintReadOnly, Category="Environment") double TideLevelM=0;
    // 바람이 만드는 평균 파고
    UPROPERTY(BlueprintReadOnly, Category="Environment") double WaveHeightM=0;
    // 모래폭풍의 상대 농도 0~1
    UPROPERTY(BlueprintReadOnly, Category="Environment") double SandstormIntensity=0;
    // 공기와 별도로 반응하는 지표 온도
    UPROPERTY(BlueprintReadOnly, Category="Environment") double GroundTemperatureC=0;
    // 아직 침투하지 않은 지표수 깊이
    UPROPERTY(BlueprintReadOnly, Category="Environment") double SurfaceWaterMm=0;
    // 얼음의 물 환산 두께
    UPROPERTY(BlueprintReadOnly, Category="Environment") double IceMm=0;
    // 토양 용량 대비 수분 비율
    UPROPERTY(BlueprintReadOnly, Category="Environment") double SoilMoisture=0;
    // 서버 시계의 현실 대비 배속
    UPROPERTY(BlueprintReadOnly, Category="Environment") double TimeScale=0;
    // 지표의 순 열 유입 W/m²
    UPROPERTY(BlueprintReadOnly, Category="Environment") double SurfaceHeatFluxWm2=0;
    // 외부에서 들어온 누적 수분
    UPROPERTY(BlueprintReadOnly, Category="Environment") double ImportedWaterKgM2=0;
    // 외부로 빠져나간 누적 수분. 토양 배수는 별도 물 수지에 포함
    UPROPERTY(BlueprintReadOnly, Category="Environment") double ExportedWaterKgM2=0;
    // 현재 사리/조금의 조석 진폭
    UPROPERTY(BlueprintReadOnly, Category="Environment") double TideEnvelopeM=0;
    // 프로필의 해변 기준점에 물이 덮인 깊이
    UPROPERTY(BlueprintReadOnly, Category="Environment") double ShoreWaterDepthM=0;
    // 서버가 확정한 야생 이동 배율
    UPROPERTY(BlueprintReadOnly, Category="Environment") double MovementMultiplier=1;
    // 서버가 확정한 야생 탐지 배율
    UPROPERTY(BlueprintReadOnly, Category="Environment") double VisibilityMultiplier=1;
};
// 자정·연말·북쪽에서 긴 경로로 역회전하지 않게 순환 값을 보간한다.
void BlendEnvironment(FUEEnvironmentState& current,const FUEEnvironmentState& target,double alpha);
