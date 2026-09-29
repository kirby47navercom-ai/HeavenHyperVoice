#pragma once
#include "CoreMinimal.h"
#include "GerstnerWaterWaves.h"
#include "UEEnvironmentWaves.generated.h"
// Water의 파도 렌더링을 사용한다. 외해 조석 계산이나 서버 이동 판정을 이 클래스에서 하지 않는다.
UCLASS(Blueprintable, EditInlineNew)
class HEAVENHYPERVOICE_API UUEEnvironmentWaves : public UGerstnerWaterWaveGeneratorBase {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves") float HeightCm=20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves") float LengthCm=1200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves") float DirectionDegrees=0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves") float Steepness=.2f;
    virtual void GenerateGerstnerWaves_Implementation(TArray<FGerstnerWave>& OutWaves) const override;
};
