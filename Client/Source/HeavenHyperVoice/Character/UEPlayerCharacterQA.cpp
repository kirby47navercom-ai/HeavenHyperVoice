#include "UEPlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void AUEPlayerCharacter::StartGameplayQAIfRequested()
{
#if WITH_EDITOR
	if (!FParse::Param(FCommandLine::Get(), TEXT("HHVGameplayQA")))
	{
		return;
	}

	FUEHHVAppearance QAType;
	if (FParse::Param(FCommandLine::Get(), TEXT("HHVQATypeB")))
	{
		QAType.Gender = EUEHHVGender::TypeB;
		QAType.BodyIndex = 2;
	}
	else
	{
		QAType.Gender = EUEHHVGender::TypeA;
		QAType.BodyIndex = 1;
	}
	QAType.BodyEquipmentIndex = 1;
	QAType.HeadIndex = 0;
	QAType.HairIndex = 0;
	QAType.EyeIndex = 0;
	ApplyHHVAppearance(QAType);
	GameplayQAStartLocation = GetActorLocation();

	// 숨김 검증에서는 정면에서 전신을 보도록 카메라만 고정한다.
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->SetControlRotation(FRotator(-4.0f, 180.0f, 0.0f));
	}
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = 300.0f;
		CameraBoom->TargetOffset = FVector(0.0f, 0.0f, 72.0f);
		CameraBoom->bDoCollisionTest = false;
	}

	GameplayQAPhase = 0;
	GetWorldTimerManager().SetTimer(
		GameplayQATimerHandle,
		this,
		&ThisClass::AdvanceGameplayQA,
		1.5f,
		false);
#endif
}

void AUEPlayerCharacter::AdvanceGameplayQA()
{
#if WITH_EDITOR
	if (GameplayQAPhase >= 11)
	{
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			PlayerController->ConsoleCommand(TEXT("quit"), true);
		}
		FPlatformMisc::RequestExit(false);
		return;
	}

	float CaptureDelay = 0.8f;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (GameplayQAPhase <= 2 && Movement)
	{
		static constexpr float QASpeeds[] = {0.0f, 260.0f, 390.0f};
		const float Speed = QASpeeds[GameplayQAPhase];
		// 실제 AnimInstance가 읽는 Velocity만 설정해 1D Blend Space 구간을 검증한다.
		Movement->SetMovementMode(MOVE_Flying);
		Movement->Velocity = GetActorForwardVector() * Speed;
		bIsRunning = GameplayQAPhase == 2;
	}
	else if (GameplayQAPhase == 3 && Movement)
	{
		Movement->SetMovementMode(MOVE_Walking);
		Movement->Velocity = FVector::ZeroVector;
		bIsRunning = false;
		Jump();
		CaptureDelay = 0.2f;
	}
	else if (GameplayQAPhase == 4 && Movement)
	{
		bLandingStateActive = false;
		GetWorldTimerManager().ClearTimer(LandingStateTimerHandle);
		SetActorLocation(GameplayQAStartLocation + FVector(0.0f, 0.0f, 600.0f), false, nullptr, ETeleportType::TeleportPhysics);
		Movement->SetMovementMode(MOVE_Falling);
		Movement->Velocity = FVector(0.0f, 0.0f, -100.0f);
		CaptureDelay = 0.2f;
	}
	else if (GameplayQAPhase == 5 && Movement)
	{
		SetActorLocation(GameplayQAStartLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Movement->SetMovementMode(MOVE_Walking);
		Movement->Velocity = FVector::ZeroVector;
		Landed(FHitResult());
		CaptureDelay = 0.08f;
	}
	else if (GameplayQAPhase == 6)
	{
		CaptureDelay = 0.18f;
	}
	else if (GameplayQAPhase == 7 && Movement)
	{
		SetActorLocation(GameplayQAStartLocation, false, nullptr, ETeleportType::TeleportPhysics);
		bLandingStateActive = false;
		GetWorldTimerManager().ClearTimer(LandingStateTimerHandle);
		Movement->SetMovementMode(MOVE_Walking);
		Movement->Velocity = FVector::ZeroVector;
		Roll();
		CaptureDelay = 0.15f;
	}
	else if (GameplayQAPhase >= 8 && GameplayQAPhase <= 10)
	{
		CaptureDelay = 0.05f;
	}

	GetWorldTimerManager().SetTimer(
		GameplayQATimerHandle,
		this,
		&ThisClass::CaptureGameplayQAFrame,
		CaptureDelay,
		false);
#endif
}

void AUEPlayerCharacter::CaptureGameplayQAFrame()
{
#if WITH_EDITOR
	static const TCHAR* PhaseNames[] = {
		TEXT("Idle"),
		TEXT("Walk"),
		TEXT("Run"),
		TEXT("JumpStart"),
		TEXT("FallLoop"),
		TEXT("LandingStart"),
		TEXT("LandingRecovery"),
		TEXT("RollStart"),
		TEXT("RollMid"),
		TEXT("RollLate"),
		TEXT("RollComplete")};
	static constexpr int32 PhaseCount = UE_ARRAY_COUNT(PhaseNames);
	FName AnimState = NAME_None;
	if (const UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		const int32 StateMachineIndex = AnimInstance->GetStateMachineIndex(TEXT("PlayerStateMachine"));
		if (StateMachineIndex != INDEX_NONE)
		{
			AnimState = AnimInstance->GetCurrentStateName(StateMachineIndex);
		}
	}
	const TCHAR* GenderName = CurrentCustomizationGender == EUEHHVGender::TypeA ? TEXT("TypeA") : TEXT("TypeB");
	UE_LOG(
		LogTemp,
		Display,
		TEXT("HHVGameplayQA Gender=%s Phase=%s State=%s AnimState=%s VelocityZ=%.2f Distance=%.2f"),
		GenderName,
		PhaseNames[FMath::Clamp(GameplayQAPhase, 0, PhaseCount - 1)],
		*CharacterStateTag.ToString(),
		*AnimState.ToString(),
		GetVelocity().Z,
		FVector::Dist2D(GetActorLocation(), GameplayQAStartLocation));
	const FString ScreenshotPath = FPaths::ProjectSavedDir() /
		FString::Printf(
			TEXT("Screenshots/Gameplay/HHV_%s_%s.png"),
			GenderName,
			PhaseNames[FMath::Clamp(GameplayQAPhase, 0, PhaseCount - 1)]);
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);

	const int32 CapturedPhase = GameplayQAPhase;
	++GameplayQAPhase;
	float AdvanceDelay = 0.8f;
	if (CapturedPhase == 5)
	{
		AdvanceDelay = 0.15f;
	}
	else if (CapturedPhase == 7)
	{
		AdvanceDelay = 0.25f;
	}
	else if (CapturedPhase == 8)
	{
		AdvanceDelay = 0.5f;
	}
	else if (CapturedPhase == 9)
	{
		AdvanceDelay = 1.1f;
	}
	GetWorldTimerManager().SetTimer(
		GameplayQATimerHandle,
		this,
		&ThisClass::AdvanceGameplayQA,
		AdvanceDelay,
		false);
#endif
}
