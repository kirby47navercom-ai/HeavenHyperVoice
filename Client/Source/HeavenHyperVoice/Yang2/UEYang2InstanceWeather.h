#pragma once

// YANG2_CLIENT_AUTHORITY_ONLY
//
// Yang2 브랜치에서만 서버의 순수 C++ 날씨 계산기를 클라이언트에 연결한다.
// 이 파일이나 이를 포함하는 브릿지 변경을 main 으로 올리면 서버 권위가 깨진다.
// 자세한 금지 범위는 저장소 루트의 YANG2_ONLY.md 를 먼저 읽을 것.

#include "CoreMinimal.h"
#include "../../../../Server/InstanceServer/src/InstanceWeather.h"
#include "UEYang2InstanceWeather.generated.h"

/** YANG2_CLIENT_AUTHORITY_ONLY: 서버 실행 옵션 대신 로컬 시험에 사용할 기후값. */
USTRUCT(BlueprintType)
struct FUEYang2InstanceWeatherProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yang2|Weather")
	double MeanTemperatureC = 18.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yang2|Weather")
	double InitialRelativeHumidityPct = 72.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yang2|Weather")
	double MeanPressureHpa = 1013.25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yang2|Weather")
	double InitialSurfaceWaterKgM2 = 2.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yang2|Weather")
	double InitialSoilWaterKgM2 = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yang2|Weather")
	double GameSecondsPerRealSecond = 60.0;
};
