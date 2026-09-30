#pragma once
#include "CoreMinimal.h"
#include "MovementEnvironment.h"
#include "UEEnvironmentWaterRegion.generated.h"

// 물이 실제로 존재하는 XY 범위예요. 수심은 공통 충돌 지형과 현재 조석으로 계산해요.
USTRUCT(BlueprintType)
struct FUEEnvironmentWaterRegion {
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Water") FVector2D Minimum=FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Water") FVector2D Maximum=FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Water") float SeaLevelCm=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Water",meta=(ClampMin="20",ClampMax="300")) float SwimDepthCm=90;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Water") bool CanSwim=true;
    hhv::movement::WaterRegion ToCore() const { return {float(Minimum.X),float(Minimum.Y),float(Maximum.X),float(Maximum.Y),SeaLevelCm,SwimDepthCm,CanSwim}; }
};
