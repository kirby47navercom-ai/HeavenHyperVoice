#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UEInstanceWeatherPresentationComponent.h"
#include "UEEnvironmentScene.generated.h"
class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class AVolumetricCloud;
class AWaterBodyOcean;
class UUEEnvironmentProfile;
class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialParameterCollection;
class UMaterialInstanceDynamic;
class UGerstnerWaterWaves;
class UWaterWavesBase;
class UUEEnvironmentWaves;

/** 맵의 빛·안개·바다를 연결하는 C++ 기반 BP. 위치/에셋은 레벨과 BP에서 지정한다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEEnvironmentScene : public AActor {
    GENERATED_BODY()
public:
    AUEEnvironmentScene();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment") TObjectPtr<UUEInstanceWeatherPresentationComponent> Weather;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment") TObjectPtr<UNiagaraComponent> Dust;
    // main은 이 프로필로 계산하지 않는다. 서버용 파일을 내보내거나 Yang2에서만 계산 입력으로 사용한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment") TObjectPtr<UUEEnvironmentProfile> Profile;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Environment|Bindings") TObjectPtr<ADirectionalLight> Sun;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Environment|Bindings") TObjectPtr<ADirectionalLight> Moon;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Environment|Bindings") TObjectPtr<ASkyLight> Sky;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Environment|Bindings") TObjectPtr<AExponentialHeightFog> Fog;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Environment|Bindings") TObjectPtr<AVolumetricCloud> Clouds;
    /** 고정 낮 하늘 돔이 SkyAtmosphere를 가리면 여기에 연결한다. 환경 활성 중에만 숨긴다. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Environment|Bindings") TObjectPtr<AActor> LegacySkyDome;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Environment|Bindings") TObjectPtr<AWaterBodyOcean> Ocean;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Effects") TObjectPtr<UNiagaraSystem> DustSystem;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Effects") TObjectPtr<UMaterialParameterCollection> Parameters;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Light", meta=(ClampMin="0")) float DayLux=10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Light", meta=(ClampMin="0")) float NightLux=.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Light") FLinearColor DawnColor=FLinearColor(1,.43f,.18f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Light") FLinearColor DayColor=FLinearColor(1,.96f,.85f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Light") FLinearColor NightColor=FLinearColor(.2f,.32f,.6f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Fog", meta=(ClampMin="0")) float ClearFog=.002f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Fog", meta=(ClampMin="0")) float DenseFog=.04f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Fog") FLinearColor DustColor=FLinearColor(.48f,.31f,.13f);
    // 사용하는 구름 머티리얼의 밀도 파라미터 이름을 BP에서 바꿀 수 있다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Cloud") FName CloudDensityParameter=TEXT("WeatherCloudDensity");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Cloud") float CloudDensityScale=1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Ocean") float WaveLengthCm=1200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Ocean") float WaveSteepness=.2f;
    // Water 높이/파도 변경은 물 구역과 GPU 데이터를 다시 만들어요. 허용 오차와 주기를 BP에서 조정해요.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Ocean", meta=(ClampMin="0.1",ClampMax="10")) float OceanUpdateSeconds=.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Ocean", meta=(ClampMin="0.25")) float TideToleranceCm=1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Ocean", meta=(ClampMin="0.1")) float WaveToleranceCm=2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment|Ocean", meta=(ClampMin="0.1")) float WaveDirectionToleranceDegrees=2;
    UPROPERTY(BlueprintReadOnly, Category="Environment") FUEInstanceWeatherPresentationState State;
private:
    void UpdateLighting();
    void UpdateOcean();
    void UpdateDust();
    void RestoreScene();
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CloudMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> OriginalCloudMaterial;
    UPROPERTY(Transient) TObjectPtr<UGerstnerWaterWaves> RuntimeWaves;
    UPROPERTY(Transient) TObjectPtr<UWaterWavesBase> OriginalWaves;
    UPROPERTY(Transient) TObjectPtr<UUEEnvironmentWaves> WaveGenerator;
    FRotator SunRotation,MoonRotation;
    FLinearColor SunColor,MoonColor,FogColor;
    float SunIntensity=0,MoonIntensity=0,SkyIntensity=0,FogDensity=0,OceanOffset=0;
    // Water의 높이 오프셋은 음수를 허용하지 않아, 기준 위치를 낮추고 같은 양을 더해 둔다.
    FVector OceanLocation=FVector::ZeroVector;
    float TideBiasCm=0;
    EComponentMobility::Type OceanMobility=EComponentMobility::Static;
    bool bApplied=false;
    float OceanTimer=0;
    bool bSkyDomeWasHidden=false;
};
