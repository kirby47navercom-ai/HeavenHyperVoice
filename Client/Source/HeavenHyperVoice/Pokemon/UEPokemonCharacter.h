// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UEPokemonCharacter.generated.h"

class UUEPokemonTestServerComponent;

USTRUCT(BlueprintType)
struct FUEPokemonServerMoveSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pokemon|Server")
	bool bTeleported = false;
};

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEPokemonCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AUEPokemonCharacter();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void ApplyServerMoveSnapshot(const FUEPokemonServerMoveSnapshot& Snapshot);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Server")
	void ApplyServerMoveTarget(const FVector& ServerLocation, const FVector& ServerVelocity, const FRotator& ServerRotation, bool bTeleported);

protected:
	virtual void BeginPlay() override;

private:
	void UpdateServerDrivenMovement(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pokemon|Test Server", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEPokemonTestServerComponent> TestServerComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerLocationInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerRotationInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ServerHardSnapDistance = 300.0f;

	UPROPERTY(Transient)
	FVector TargetServerLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector TargetServerVelocity = FVector::ZeroVector;

	UPROPERTY(Transient)
	FRotator TargetServerRotation = FRotator::ZeroRotator;

	bool bHasServerMoveTarget = false;
};
