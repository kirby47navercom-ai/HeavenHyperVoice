#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "UEGoldenrodSpringArm.generated.h"

class AUEGoldenrodCity;

/** Keeps normal spring-arm collision/lag while gently looking down near the coast. */
UCLASS()
class HEAVENHYPERVOICE_API UUEGoldenrodSpringArm : public USpringArmComponent
{
	GENERATED_BODY()
protected:
	virtual void BeginPlay() override;
	virtual FRotator GetDesiredRotation() const override;
	virtual void UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag,
		bool bDoRotationLag, float DeltaTime) override;
private:
	TWeakObjectPtr<AUEGoldenrodCity> City;
	float BoundaryWeight = 0.f;
	float MenuWeight = 0.f;
	bool bApplyingBoundary = false;
	FRotator BoundaryRotation = FRotator::ZeroRotator;
};
