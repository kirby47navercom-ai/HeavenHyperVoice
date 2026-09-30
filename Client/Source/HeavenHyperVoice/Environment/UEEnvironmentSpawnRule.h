#pragma once
#include "CoreMinimal.h"
#include "UEEnvironmentSpawnRule.generated.h"
/** 기존 맵의 종족 목록 안에서 가중치만 바꿔요. 1이면 기존 확률 그대로예요. */
USTRUCT(BlueprintType)
struct FUEEnvironmentSpawnRule {
    GENERATED_BODY()
    // 서버 내부 배열 번호가 아닌 도감 번호예요.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1",ClampMax="65535")) int32 PokemonDex=1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="100")) double BaseWeight=1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="100")) double RainMultiplier=1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="100")) double SnowMultiplier=1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="100")) double NightMultiplier=1;
};
