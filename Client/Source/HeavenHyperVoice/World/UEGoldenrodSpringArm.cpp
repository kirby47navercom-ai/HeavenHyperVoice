#include "UEGoldenrodSpringArm.h"
#include "UEGoldenrodCity.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "../Player/UEPlayerController.h"

void UUEGoldenrodSpringArm::BeginPlay()
{
	Super::BeginPlay();
	for (TActorIterator<AUEGoldenrodCity> It(GetWorld()); It; ++It)
	{
		City = *It;
		break;
	}
}

FRotator UUEGoldenrodSpringArm::GetDesiredRotation() const
{
	return bApplyingBoundary ? BoundaryRotation : Super::GetDesiredRotation();
}

void UUEGoldenrodSpringArm::UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag,
	bool bDoRotationLag, float DeltaTime)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AUEGoldenrodCity* Boundary = City.Get();
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		BoundaryWeight = 0.f;
		Super::UpdateDesiredArmLocation(bDoTrace, bDoLocationLag, bDoRotationLag, DeltaTime);
		return;
	}
	const AUEPlayerController* Controller = Cast<AUEPlayerController>(Pawn->GetController());
	const bool bPhoto = Controller && Controller->IsPhotoModeActive();
	MenuWeight = bPhoto ? 0.f : FMath::FInterpTo(MenuWeight,
		Controller && Controller->IsOptionsCameraActive() ? 1.f : 0.f, DeltaTime, 7.f);
	BoundaryWeight = bPhoto ? 0.f : FMath::FInterpTo(BoundaryWeight,
		Boundary ? Boundary->GetBoundaryCameraWeight(Pawn->GetActorLocation()) : 0.f, DeltaTime, 3.f);
	BoundaryRotation = GetTargetRotation().GetNormalized();
	if (Boundary)
		BoundaryRotation.Pitch = FMath::Lerp(BoundaryRotation.Pitch,
			FMath::Min(BoundaryRotation.Pitch, Boundary->BoundaryCameraPitch), BoundaryWeight);
	if (MenuWeight > KINDA_SMALL_NUMBER)
	{
		const FRotator Portrait(-8.f, Pawn->GetActorRotation().Yaw + 155.f, 0.f);
		BoundaryRotation = FMath::LerpRange(BoundaryRotation, Portrait, MenuWeight);
	}

	// 카메라 계산 중에만 회전을 덮어쓴다. 플레이어의 시점 입력과 이동 방향은 보존한다.
	const bool bOriginalControlRotation = bUsePawnControlRotation;
	const float OriginalArmLength = TargetArmLength;
	const FVector OriginalTargetOffset = TargetOffset;
	const FVector OriginalSocketOffset = SocketOffset;
	bApplyingBoundary = true;
	bUsePawnControlRotation = false;
	TargetArmLength = FMath::Lerp(TargetArmLength + (Boundary ? Boundary->BoundaryCameraExtraDistance * BoundaryWeight : 0.f), 145.f, MenuWeight);
	TargetOffset.Z += 42.f * MenuWeight;
	SocketOffset.Y -= 35.f * MenuWeight;
	if (bPhoto)
	{
		TargetArmLength = 0.f;
		TargetOffset = FVector(0, 0, Pawn->BaseEyeHeight);
		SocketOffset = FVector::ZeroVector;
	}
	Super::UpdateDesiredArmLocation(bDoTrace && !bPhoto, bDoLocationLag && !bPhoto, bDoRotationLag && !bPhoto, DeltaTime);
	TargetArmLength = OriginalArmLength;
	TargetOffset = OriginalTargetOffset;
	SocketOffset = OriginalSocketOffset;
	bUsePawnControlRotation = bOriginalControlRotation;
	bApplyingBoundary = false;
}
