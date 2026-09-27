#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UEInstanceWeatherPresentationComponent.h"
#include "UEInstanceWeatherDirector.generated.h"
class UNiagaraSystem;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UDecalComponent;
class AUEWeatherExclusionVolume;

/** 외형은 자식 BP에서 에셋으로 지정한다. C++은 낙하 경로와 표면 샘플링만 맡는다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEInstanceWeatherDirector : public AActor
{
    GENERATED_BODY()
public:
    AUEInstanceWeatherDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weather")
    TObjectPtr<UUEInstanceWeatherPresentationComponent> Presentation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Effects")
    TObjectPtr<UNiagaraSystem> Rain;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Effects")
    TObjectPtr<UNiagaraSystem> Snow;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Effects")
    TObjectPtr<UNiagaraSystem> RainImpact;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Effects")
    TObjectPtr<UNiagaraSystem> WallSplash;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Effects")
    TObjectPtr<UNiagaraSystem> WaterRipple;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Effects")
    TObjectPtr<UNiagaraSystem> SnowImpact;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Effects")
    TObjectPtr<UNiagaraSystem> SnowChunks;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Surface")
    TObjectPtr<UMaterialInterface> WetImpactMaterial;
    /** 피격 자국은 잠시 남았다 사라진다. 실제 물의 저장량이나 전투 판정은 아니다. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather|Surface", meta=(ClampMin="0",ClampMax="64"))
    int32 MaxWetMarks = 32;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather|Surface", meta=(ClampMin="0.2",ClampMax="10"))
    float WetMarkLifetime = 4;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Surface")
    TObjectPtr<UMaterialInterface> SurfaceMaterial;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weather|Surface")
    TObjectPtr<UMaterialParameterCollection> Parameters;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather|Sampling", meta=(ClampMin="100",ClampMax="2000",Units="cm"))
    float Radius = 700;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather|Sampling", meta=(ClampMin="100",ClampMax="2000",Units="cm"))
    float FallHeight = 600;
    /** 실제 빗방울 수가 아닌 시각 샘플 수. 프레임당 예산과 대기 충돌 수를 별도로 제한한다. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather|Sampling", meta=(ClampMin="1",ClampMax="200"))
    float MaxDropsPerSecond = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather|Sampling", meta=(ClampMin="1000",ClampMax="100000",Units="cm"))
    float SkyTraceHeight = 20000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather|Sampling")
    TEnumAsByte<ECollisionChannel> WeatherCollisionChannel = ECC_Visibility;
    /** 수면 메시에 이 태그를 붙이면 물 튀김 대신 파문을 쓰며 눈 덮임을 만들지 않는다. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather|Surface")
    FName WaterSurfaceTag = TEXT("WeatherWater");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather|Surface", meta=(ClampMin="50",ClampMax="400",Units="cm"))
    float SurfaceTileSize = 160;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather|Surface", meta=(ClampMin="1",ClampMax="6"))
    int32 SurfaceGridRadius = 5;
    UPROPERTY(BlueprintReadOnly, Category="Weather|Status")
    bool bCameraSheltered = false;
    /** 추가 하늘/음향은 BP에서 이 이벤트로 연결. 전투 판정에는 서버 원본 값을 사용한다. */
    UFUNCTION(BlueprintImplementableEvent, Category="Weather")
    void OnWeatherVisualsUpdated(const FUEInstanceWeatherPresentationState& VisualState, bool Sheltered);
private:
    friend class FInstanceWeatherTest;
    struct FPendingImpact
    {
        double Due;
        FVector Start;
        FVector End;
        bool bSnow;
    };
    TArray<FPendingImpact> PendingImpacts;
    TArray<TWeakObjectPtr<AUEWeatherExclusionVolume>> Exclusions;
    TArray<TWeakObjectPtr<UDecalComponent>> WetMarks;
    UPROPERTY(Transient) TArray<TObjectPtr<UDecalComponent>> Tiles;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;
    UPROPERTY(Transient) TObjectPtr<UUEFieldServerBridgeComponent> Source;
    FUEInstanceWeatherPresentationState State;
    float DropBudget = 0;
    float SurfaceTimer = 0;
    float ExclusionTimer = 0;
    int32 TileCursor = 0;
    bool bHadWeather = false;
    bool Trace(const FVector& Start, const FVector& End, FHitResult& Hit) const;
    bool IsExcluded(const FVector& Start, const FVector& End) const;
    bool IsWater(const FHitResult& Hit) const;
    bool UsesWeatherMaterial(const FHitResult& Hit) const;
    void SpawnImpact(const FHitResult& Hit, bool bSnow);
    void UpdateExclusionParameters();
    void SpawnDrop(const FVector& Center, bool bSnow);
    void UpdateSurface(const FVector& Center);
    void ApplyParameters();
    void ClearWeather();
};
