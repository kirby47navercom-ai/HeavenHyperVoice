#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "MovementReplay.h"
#include "UECoreMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EUECoreMovementMode : uint8
{
	Grounded,
	Falling,
	Disabled
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FUECoreMovementUpdated, float, DeltaSeconds, FVector,
                                               OldLocation, FVector, OldVelocity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUECoreLanded, const FHitResult&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUECoreJumpApex);

/** Pawn adapter. No CharacterMovement dependency or engine character physics. */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUECoreMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	UUECoreMovementComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* TickFunction) override;
	virtual void StopMovementImmediately() override;
	virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;
	virtual void OnTeleported() override;

	virtual float GetMaxSpeed() const override
	{
		return bRunRequested ? CoreRunSpeed : MaxWalkSpeed;
	}

	UPROPERTY(BlueprintAssignable, Category = "Shared Movement|Events")
	FUECoreMovementUpdated OnMovementUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Shared Movement|Events")
	FUECoreLanded OnLanded;

	UPROPERTY(BlueprintAssignable, Category = "Shared Movement|Events")
	FUECoreJumpApex OnJumpApex;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Shared Movement|State")
	EUECoreMovementMode MovementMode = EUECoreMovementMode::Falling;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Shared Movement|State")
	FVector Acceleration = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Shared Movement|State")
	FVector FloorNormal = FVector::UpVector;

	UFUNCTION(BlueprintPure, Category = "Shared Movement|State")
	FVector GetCurrentAcceleration() const
	{
		return Acceleration;
	}

	bool IsFalling() const override
	{
		return MovementMode == EUECoreMovementMode::Falling;
	}

	bool IsMovingOnGround() const override
	{
		return MovementMode == EUECoreMovementMode::Grounded;
	}

	UFUNCTION(BlueprintCallable, Category = "Shared Movement|State")
	void SetMovementMode(EUECoreMovementMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Shared Movement|Input")
	void CancelCoreJump()
	{
		PendingButtons &= ~hhv::movement::Jump;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Walking", meta = (ClampMin = "0"))
	float MaxWalkSpeed = 260;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Walking", meta = (ClampMin = "0"))
	float MaxAcceleration = 2048;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Walking", meta = (ClampMin = "0"))
	float BrakingDecelerationWalking = 2048;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Walking", meta = (ClampMin = "0"))
	float GroundFriction = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Falling", meta = (ClampMin = "0"))
	float GravityAcceleration = 980;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Falling", meta = (ClampMin = "0"))
	float JumpZVelocity = 420;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Falling",
	          meta = (ClampMin = "0", ClampMax = "1"))
	float AirControl = .35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Ground", meta = (ClampMin = "0"))
	float MaxStepHeight = 45;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Ground",
	          meta = (ClampMin = "0", ClampMax = "89"))
	float WalkableFloorAngle = 44;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Walking", meta = (ClampMin = "0"))
	float RotationSpeed = 540;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement")
	bool bRunWithoutController = false;

	UFUNCTION(BlueprintCallable, Category = "Shared Movement|Input")
	void RequestCoreJump()
	{
		PendingButtons |= hhv::movement::Jump;
	}

	UFUNCTION(BlueprintCallable, Category = "Shared Movement|Input")
	void RequestCoreRoll()
	{
		PendingButtons |= hhv::movement::Roll;
	}

	UFUNCTION(BlueprintCallable, Category = "Shared Movement|Input")
	void SetCoreRunning(bool bRunning)
	{
		bRunRequested = bRunning;
	}

	UFUNCTION(BlueprintPure, Category = "Shared Movement|State")
	bool IsWallSliding() const
	{
		return CoreState.wallSliding;
	}

	UFUNCTION(BlueprintPure, Category = "Shared Movement|State")
	bool IsCoreRolling() const
	{
		return CoreState.rollRemaining > 0;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Walking", meta = (ClampMin = "0"))
	float CoreRunSpeed = 390;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Ground",
	          meta = (ClampMin = "0.1"))
	float FloorSnapDistance = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Ground",
	          meta = (ClampMin = "0.01", ClampMax = "1"))
	float ContactSkin = .1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Falling", meta = (ClampMin = "1"))
	float TerminalFallSpeed = 4000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Roll", meta = (ClampMin = "0"))
	float CoreRollSpeed = 600;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Movement|Roll",
	          meta = (ClampMin = "0.0167", ClampMax = "2"))
	float CoreRollDuration = .6f;
	// Empty: load Content/MovementCollision/<map package path>.hhvcollision, in PIE and packaged games.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shared Movement|Collision",
	          meta = (FilePathFilter = "hhvcollision"))
	FFilePath CollisionFile;

	UFUNCTION(BlueprintCallable, Category = "Shared Movement|Testing")
	bool ExportCoreReplay(const FString& BasePath) const;
	hhv::movement::Config MakeCoreConfig() const;

	const hhv::movement::State& GetCoreState() const
	{
		return CoreState;
	}

protected:
	virtual void BeginPlay() override;

private:
	void PublishState(bool bEvents);
	void ResetFromActor();
	hhv::movement::State CoreState;
	std::deque<hhv::movement::StepRecord> History;
	uint32 NextSequence = 1;
	uint64 CollisionHash = 0;
	float Accumulator = 0;
	uint8 PendingButtons = 0;
	bool bRunRequested = false;
};
