#include "UECoreMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UECoreCollisionSubsystem.h"

namespace
{
using namespace hhv::movement;

Vec3 CoreVector(const FVector &V)
{
	return {float(V.X), float(V.Y), float(V.Z)};
}

FVector UEVector(Vec3 V)
{
	return FVector(V.x, V.y, V.z);
}
} // namespace

UUECoreMovementComponent::UUECoreMovementComponent()
{
	MaxWalkSpeed = 260;
	MaxAcceleration = 2048;
	BrakingDecelerationWalking = 2048;
	GroundFriction = 8;
	JumpZVelocity = 420;
	AirControl = .35f;
	MaxStepHeight = 45;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bTickBeforeOwner = false;
}

void UUECoreMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	// Character input is gathered in actor Tick; consume it in the same frame.
	if (PawnOwner)
	{
		AddTickPrerequisiteActor(PawnOwner);
	}
	ResetFromActor();
}

void UUECoreMovementComponent::ResetFromActor()
{
	if (!PawnOwner)
	{
		return;
	}
	hhv::movement::State State;
	State.position = CoreVector(PawnOwner->GetActorLocation());
	State.velocity = CoreVector(Velocity);
	State.facing = PawnOwner->GetActorRotation().Yaw;
	State.mode = MovementMode == EUECoreMovementMode::Disabled ? hhv::movement::Mode::Disabled
	                                                           : hhv::movement::Mode::Falling;
	CoreState = State;
	History.clear();
	Accumulator = 0;
	PendingButtons = 0;
}

hhv::movement::Config UUECoreMovementComponent::MakeCoreConfig() const
{
	if (bNetworkSimulation)
	{
		return hhv::movement::Config{};
	}
	hhv::movement::Config C;

	if (PawnOwner)
	{
		C.radius = CastChecked<UCapsuleComponent>(UpdatedComponent)->GetScaledCapsuleRadius();
		C.halfHeight = CastChecked<UCapsuleComponent>(UpdatedComponent)->GetScaledCapsuleHalfHeight();
	}
	C.walkSpeed = FMath::Max(0.f, MaxWalkSpeed);
	C.runSpeed = FMath::Max(0.f, CoreRunSpeed);
	C.acceleration = FMath::Max(0.f, MaxAcceleration);
	C.braking = FMath::Max(0.f, BrakingDecelerationWalking);
	C.friction = FMath::Max(0.f, GroundFriction);
	C.gravity = FMath::Max(0.f, GravityAcceleration);
	C.jumpSpeed = FMath::Max(0.f, JumpZVelocity);
	C.airControl = FMath::Clamp(AirControl, 0.f, 1.f);
	C.terminalSpeed = FMath::Max(1.f, TerminalFallSpeed);
	C.stepHeight = FMath::Max(0.f, MaxStepHeight);
	C.slopeDegrees = FMath::Clamp(WalkableFloorAngle, 0.f, 89.f);
	C.floorSnap = FMath::Max(.1f, FloorSnapDistance);
	C.skin = FMath::Clamp(ContactSkin, .01f, 1.f);
	C.rotationSpeed = FMath::Abs(RotationSpeed);
	C.rollSpeed = FMath::Max(0.f, CoreRollSpeed);
	C.rollTicks = FMath::Clamp(FMath::RoundToInt(CoreRollDuration / hhv::movement::FixedDt), 1, 120);
	return C;
}

void UUECoreMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction *TickFunction)
{
	// UPawnMovementComponent handles component bookkeeping only, with no character physics.
	Super::TickComponent(DeltaTime, TickType, TickFunction);

	if (!PawnOwner || !Cast<UCapsuleComponent>(UpdatedComponent) || !IsActive() ||
	    ShouldSkipUpdate(DeltaTime))
	{
		return;
	}
	const FVector Input = ConsumeInputVector().GetClampedToMaxSize(1);
	if (bWaitingForNetwork)
	{
		return;
	}

	if (MovementMode == EUECoreMovementMode::Disabled)
	{
		return;
	}

	if (!PawnOwner->GetController() && !bRunWithoutController)
	{
		return;
	}

	if (!bNetworkSimulation &&
	    !UpdatedComponent->GetComponentLocation().Equals(UEVector(CoreState.position), .01f))
	{
		ResetFromActor();
	}

	if (hhv::movement::length(CoreState.velocity - CoreVector(Velocity)) > .0001f)
	{
		History.clear();
	}
	if (!bNetworkSimulation)
	{
		CoreState.velocity = CoreVector(Velocity);
	}
	const FVector OldLocation = UpdatedComponent->GetComponentLocation(), OldVelocity = Velocity;
	Accumulator = FMath::Clamp(Accumulator + DeltaTime, 0.f, .25f);
	auto *SharedWorld = GetWorld()->GetSubsystem<UUECoreCollisionSubsystem>();

	if (!SharedWorld || !SharedWorld->EnsureReady(CollisionFile.FilePath))
	{
		return;
	}
	const auto &Collision = *SharedWorld->GetCollision();

	if (CollisionHash != Collision.hash())
	{
		History.clear();
		CollisionHash = Collision.hash();
	}
	const auto Config = MakeCoreConfig();
	int32 Steps = 0;

	while (Accumulator + 1e-7f >= hhv::movement::FixedDt && Steps++ < 15)
	{
		hhv::movement::Input Command;
		Command.x = Input.X;
		Command.y = Input.Y;
		Command.buttons = PendingButtons | (bRunRequested ? hhv::movement::Run : 0);
		const auto Before = CoreState;
		if (bNetworkSimulation)
		{
			if (!Prediction.predict(Command, Config, Collision))
			{
				Accumulator = 0;
				break;
			}
			CoreState = Prediction.state;
			History.push_back(Prediction.history.back());
		}
		else
		{
			Command.sequence = NextSequence++;
			hhv::movement::simulate(CoreState, Command, Config, Collision);
			History.push_back({Command, Config, Before, CoreState});
		}

		if (History.size() > 600)
		{
			History.pop_front();
		}
		PendingButtons = 0;
		Accumulator -= hhv::movement::FixedDt;
		PublishState(true);
	}
	OnMovementUpdated.Broadcast(DeltaTime, OldLocation, OldVelocity);
}

void UUECoreMovementComponent::PublishState(bool bEvents)
{
	if (!PawnOwner || !UpdatedComponent)
	{
		return;
	}
	const auto &S = CoreState;
	const bool bWasFalling = IsFalling();
	const float OldZ = Velocity.Z;
	Velocity = UEVector(S.velocity);
	Acceleration = UEVector(S.acceleration);
	const EUECoreMovementMode NewMode =
	    S.mode == hhv::movement::Mode::Grounded   ? EUECoreMovementMode::Grounded
	    : S.mode == hhv::movement::Mode::Disabled ? EUECoreMovementMode::Disabled
	                                              : EUECoreMovementMode::Falling;
	MovementMode = NewMode;
	// No engine sweep or depenetration after the core has resolved collision.
	UpdatedComponent->SetWorldLocationAndRotation(UEVector(S.position), FRotator(0, S.facing, 0), false,
	                                              nullptr, ETeleportType::TeleportPhysics);
	UpdateComponentVelocity();
	FloorNormal = UEVector(S.floorNormal);

	if (bEvents && bWasFalling && NewMode == EUECoreMovementMode::Grounded)
	{
		FHitResult Hit;
		Hit.bBlockingHit = true;
		Hit.Normal = Hit.ImpactNormal = FloorNormal;
		OnLanded.Broadcast(Hit);
	}

	if (bEvents && bWasFalling && OldZ > 0 && Velocity.Z <= 0)
	{
		OnJumpApex.Broadcast();
	}
}

void UUECoreMovementComponent::SetMovementMode(EUECoreMovementMode NewMode)
{
	History.clear();
	MovementMode = NewMode;
	CoreState.mode = static_cast<hhv::movement::Mode>(NewMode);
}

void UUECoreMovementComponent::StopMovementImmediately()
{
	History.clear();
	Velocity = FVector::ZeroVector;
	Acceleration = FVector::ZeroVector;
	CoreState.velocity = {};
	CoreState.acceleration = {};
	CoreState.rollRemaining = 0;
	PendingButtons = 0;
	ConsumeInputVector();
	UpdateComponentVelocity();
}

void UUECoreMovementComponent::RequestDirectMove(const FVector &MoveVelocity, bool bForceMaxSpeed)
{
	AddInputVector(
	    MoveVelocity.GetSafeNormal() *
	    (bForceMaxSpeed ? 1.f : FMath::Clamp(MoveVelocity.Size() / FMath::Max(MaxWalkSpeed, 1.f), 0.f, 1.f)));
}

void UUECoreMovementComponent::OnTeleported()
{
	ResetFromActor();
}

bool UUECoreMovementComponent::ExportCoreReplay(const FString &BasePath) const
{
	const auto *SharedWorld = GetWorld()->GetSubsystem<UUECoreCollisionSubsystem>();

	if (!SharedWorld || !SharedWorld->GetCollision() ||
	    SharedWorld->GetCollision()->hash() != CollisionHash || History.empty())
	{
		return false;
	}
	const FString Path =
	    BasePath.IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("CoreMovement/LastReplay") : BasePath;

	if (!SharedWorld->SaveCollisionFile(Path + TEXT(".hhvcollision")))
	{
		return false;
	}
	std::ostringstream Stream;

	if (!hhv::movement::saveReplay(Stream, CollisionHash, History))
	{
		return false;
	}
	return FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(Stream.str().c_str()), *(Path + TEXT(".hhvreplay")),
	                                     FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void UUECoreMovementComponent::WaitForNetworkSimulation()
{
	bWaitingForNetwork = true;
	bNetworkSimulation = false;
	Accumulator = 0;
	PendingButtons = 0;
}

bool UUECoreMovementComponent::BeginNetworkSimulation(const hhv::movement::State &Initial, uint64 MapHash)
{
	auto *World = GetWorld()->GetSubsystem<UUECoreCollisionSubsystem>();
	if (!World || !World->EnsureReady(CollisionFile.FilePath) || World->GetCollision()->hash() != MapHash)
	{
		UE_LOG(LogTemp, Error,
		       TEXT("Movement collision mismatch: client and server must use the same exported map."));
		return false;
	}

	bNetworkSimulation = true;
	bWaitingForNetwork = false;
	CollisionHash = MapHash;
	Prediction.reset(Initial);
	CoreState = Initial;
	History.clear();
	Accumulator = 0;
	PendingButtons = 0;
	NextSequence = 1;
	PublishState(false);
	return true;
}

void UUECoreMovementComponent::AcknowledgeNetworkInput(uint32 Sequence, const hhv::movement::State &State)
{
	const auto *World = GetWorld()->GetSubsystem<UUECoreCollisionSubsystem>();
	if (bNetworkSimulation && World && World->GetCollision() &&
	    Prediction.acknowledge(Sequence, State, *World->GetCollision()))
	{
		if (!hhv::movement::replay::near(CoreState, Prediction.state, .0001f))
		{
			// A correction starts a new continuous replay segment.
			History = Prediction.history;
		}
		CoreState = Prediction.state;
		PublishState(false);
	}
}

std::vector<hhv::movement::PredictedInput> UUECoreMovementComponent::TakeNetworkInputs()
{
	return bNetworkSimulation ? Prediction.takeUnsent() : std::vector<hhv::movement::PredictedInput>{};
}

void UUECoreMovementComponent::RenderServerState(const hhv::movement::State &State)
{
	SetComponentTickEnabled(false);
	CoreState = State;
	PublishState(false);
}
