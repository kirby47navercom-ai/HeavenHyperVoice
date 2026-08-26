#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UEPokemonSummonEffectComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraSystem;
class USkeletalMeshComponent;
class UStaticMesh;
class ACharacter;

// 소환과 귀환 중 어느 방향으로 메시 크기와 발광을 보간할지 나타낸다.
enum class EUEPokemonSummonEffectPhase : uint8
{
	None,
	Spawning,
	Despawning
};

/**
 * 동행 포켓몬의 소환/귀환 화면 연출만 담당하는 클라이언트 컴포넌트다.
 * 실제 라이트를 만들지 않고 Overlay Material과 Niagara를 사용하므로
 * 월드 조명, 그림자, 서버 판정에는 영향을 주지 않는다.
 */
UCLASS(ClassGroup = (Pokemon), Config = Game, DefaultConfig, meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUEPokemonSummonEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUEPokemonSummonEffectComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// Duration이 0이면 DefaultEffectDuration을 사용하고, 실제 적용한 시간을 반환한다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Effects")
	float PlaySpawnEffect(float Duration = 0.0f);

	// 플레이어의 파괴 타이머가 이 반환값만큼 기다리면 귀환 연출이 잘리지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Effects")
	float PlayDespawnEffect(float Duration = 0.0f);

	UFUNCTION(BlueprintPure, Category = "Pokemon|Effects")
	bool IsEffectActive() const { return EffectPhase != EUEPokemonSummonEffectPhase::None; }

	// 소환 빛이 시작하고 귀환 빛이 끝날 플레이어를 명시적으로 지정한다.
	// 포켓몬 Owner에만 의존하지 않으므로 월드 등록 과정에서 Owner가 바뀌어도 몸 좌표를 잃지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Effects")
	void SetEffectBodyActor(AActor* InBodyActor);

private:
	float BeginEffect(EUEPokemonSummonEffectPhase NewPhase, float RequestedDuration);
	void FinishEffect();
	void SpawnBurst(const USkeletalMeshComponent* MeshComponent) const;
	void CreateLightTrail(const USkeletalMeshComponent* MeshComponent);
	void DestroyLightTrail();
	void UpdateLightTrail(float TravelProgress, bool bSpawning, bool bVisible) const;
	const USkeletalMeshComponent* FindVisibleBodyMesh(const ACharacter* BodyCharacter) const;
	FVector GetBodyEffectLocation() const;
	FVector GetPokemonEffectLocation() const;

	// 기존 포켓몬 머티리얼을 교체하지 않고 위에 한 번 더 그리는 비조명 발광 머티리얼이다.
	UPROPERTY(Config, EditAnywhere, Category = "Pokemon|Effects")
	TSoftObjectPtr<UMaterialInterface> GlowOverlayMaterial;

	// 소환 지점에서 한 번만 터지는 리본/입자다. Point Light Renderer는 사용하지 않는다.
	UPROPERTY(Config, EditAnywhere, Category = "Pokemon|Effects")
	TSoftObjectPtr<UNiagaraSystem> SummonBurstSystem;

	// 빛 궤적과 구체의 형상은 BP_Pokemon 기본값에서 지정한다.
	// 코드에 콘텐츠 경로를 넣지 않아 에셋 이동과 이름 변경에 안전하게 둔다.
	UPROPERTY(Config, EditAnywhere, Category = "Pokemon|Effects")
	TSoftObjectPtr<UStaticMesh> TrailMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Pokemon|Effects")
	TSoftObjectPtr<UStaticMesh> OrbMesh;

	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects", meta = (ClampMin = "0.05"))
	float DefaultEffectDuration = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float MinimumMeshScale = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects", meta = (ClampMin = "0.0"))
	float PeakGlowStrength = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects", meta = (ClampMin = "0.01"))
	float BurstScaleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects")
	FLinearColor GlowColor = FLinearColor(0.10f, 0.65f, 1.0f, 1.0f);

	// 전체 연출 시간 중 빛구슬이 베지어 곡선을 따라 이동하는 시간 비율이다.
	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects|Path", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float LightTravelFraction = 0.6f;

	// 모듈형 캐릭터의 보이는 몸 메시에서 찾을 가슴 뼈/소켓 이름이다.
	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects|Path")
	FName BodySocketName = TEXT("spine_03");

	// 가슴뼈 중심은 몸 안쪽이므로 시작점을 현재 카메라 쪽 몸 표면으로 꺼내는 거리(cm)다.
	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects|Path", meta = (ClampMin = "0.0"))
	float BodySurfaceCameraOffset = 18.0f;

	// 3차 베지어 제어점이 몸 앞쪽으로 뻗는 거리(cm)다.
	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects|Path", meta = (ClampMin = "0.0"))
	float CurveForwardOffset = 80.0f;

	// 곡선이 몸과 포켓몬 뒤에 가려지지 않도록 옆으로 휘는 거리(cm)다.
	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects|Path", meta = (ClampMin = "0.0"))
	float CurveSideOffset = 45.0f;

	// 곡선의 위쪽 높이(cm)다.
	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects|Path", meta = (ClampMin = "0.0"))
	float CurveHeight = 85.0f;

	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects|Path", meta = (ClampMin = "0.005"))
	float TrailThickness = 0.06f;

	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects|Path", meta = (ClampMin = "0.01"))
	float OrbScale = 0.16f;

	UPROPERTY(EditAnywhere, Category = "Pokemon|Effects|Path", meta = (ClampMin = "0.0"))
	float TrailGlowStrength = 5.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMaterialInstance = nullptr;

	// 소환/귀환 중 실제 몸 좌표를 매 프레임 다시 읽기 위한 약한 참조다.
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> EffectBodyActor;

	FVector OriginalMeshScale = FVector::OneVector;
	float EffectElapsedSeconds = 0.0f;
	float EffectDurationSeconds = 0.9f;
	EUEPokemonSummonEffectPhase EffectPhase = EUEPokemonSummonEffectPhase::None;
};
