#pragma once
#include "CoreMinimal.h"
#include "UEFuzzyWeatherProfile.generated.h"

/** 환경 DA에 펼쳐서 편집하는 퍼지 규칙. 실행 권위는 main 서버 / Yang2 로컬이에요. */
USTRUCT(BlueprintType)
struct FUEFuzzyWeatherProfile {
    GENERATED_BODY()
    // 상층 습도에서 '습함'이 시작/완료되는 백분율이에요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0",ClampMax="100")) double HumidStartPct=65;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0",ClampMax="100")) double HumidFullPct=95;
    // 지역 평균 기압보다 이만큼 낮으면 저기압 소속도가 1이에요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0.01",ClampMax="100")) double LowPressureFullDeficitHpa=12;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0.001",ClampMax="1")) double CloudFullCover=0.7;
    // 두 온도 사이에서는 비와 눈이 섞여요. Snow < Rain이어야 해요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Snow", meta=(ClampMin="-60",ClampMax="60")) double FullSnowTemperatureC=-1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Snow", meta=(ClampMin="-60",ClampMax="60")) double FullRainTemperatureC=1;
    // 저층 상대 습도가 안개 조건에 속하는 정도예요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fog", meta=(ClampMin="0",ClampMax="100")) double FogHumidStartPct=85;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fog", meta=(ClampMin="0",ClampMax="100")) double FogHumidFullPct=100;
    // 공기보다 지표가 이 온도만큼 차가우면 냉각 조건이 1이에요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fog", meta=(ClampMin="0.01",ClampMax="60")) double FogCoolingFullC=4;
    // Start 이하=잔잔함 1, End 이상=잔잔함 0이에요. 단위는 m/s예요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fog", meta=(ClampMin="0",ClampMax="200")) double FogCalmStartMps=0.5;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fog", meta=(ClampMin="0",ClampMax="200")) double FogCalmEndMps=5;
    // 토양 건조 비율에서 '마름'이 시작/완료되는 값이에요. 강풍 기준은 기존 DustStart/FullWindMps예요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dust", meta=(ClampMin="0",ClampMax="1")) double SoilDryStart=0.2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dust", meta=(ClampMin="0",ClampMax="1")) double SoilDryFull=0.8;
    // 배열 인덱스 0~7 = 조건 000~111이에요. 1은 조건, 0은 반대 조건이에요.
    // 강수 조건: 습함 / 저기압 / 구름 많음. 값은 강수 배율 0~1이에요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, EditFixedSize, Category="Rules", meta=(ClampMin="0",ClampMax="1")) TArray<double> RainRuleOutputs{0,0,0,0,0,0.35,0,1};
    // 안개 조건: 습함 / 잔잔함 / 지표 차가움. 값은 안개 강도 0~1이에요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, EditFixedSize, Category="Rules", meta=(ClampMin="0",ClampMax="1")) TArray<double> FogRuleOutputs{0,0,0,0,0.05,0.15,0.65,1};
    // 먼지 조건: 토양 마름 / 강풍 / 표면 마름. 값은 모래폭풍 강도 0~1이에요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, EditFixedSize, Category="Rules", meta=(ClampMin="0",ClampMax="1")) TArray<double> DustRuleOutputs{0,0,0,0,0,0,0,1};
};
