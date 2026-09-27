#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UEWeatherExclusionVolume.generated.h"
class UBoxComponent;

/** 실내, 동굴, 특수 구역에 배치하는 날씨 차단 상자. 이동 충돌은 만들지 않는다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEWeatherExclusionVolume : public AActor
{
    GENERATED_BODY()
public:
    AUEWeatherExclusionVolume();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weather")
    TObjectPtr<UBoxComponent> Bounds;
    bool ContainsPoint(const FVector& Point) const;
    bool IntersectsPath(const FVector& Start, const FVector& End) const;
    /** 머티리얼에서도 같은 회전 상자를 검사하도록 월드→정규화 상자 변환을 보낸다. */
    void GetMaterialRows(FLinearColor& X, FLinearColor& Y, FLinearColor& Z) const;
};
