#include "UEPlayerCharacter.h"

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

	// 숨김 검증에서는 정면에서 전신을 보도록 카메라만 고정한다.
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->SetControlRotation(FRotator(-4.0f, 180.0f, 0.0f));
	}
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = 300.0f;
		CameraBoom->TargetOffset = FVector(0.0f, 0.0f, 72.0f);
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
	if (GameplayQAPhase >= 3)
	{
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			PlayerController->ConsoleCommand(TEXT("quit"), true);
		}
		FPlatformMisc::RequestExit(false);
		return;
	}

	static constexpr float QASpeeds[] = {0.0f, 260.0f, 390.0f};
	const float Speed = QASpeeds[GameplayQAPhase];
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// 실제 AnimInstance가 읽는 Velocity만 설정해 1D Blend Space 구간을 검증한다.
		Movement->SetMovementMode(MOVE_Flying);
		Movement->Velocity = GetActorForwardVector() * Speed;
	}
	bIsRunning = GameplayQAPhase == 2;

	GetWorldTimerManager().SetTimer(
		GameplayQATimerHandle,
		this,
		&ThisClass::CaptureGameplayQAFrame,
		0.8f,
		false);
#endif
}

void AUEPlayerCharacter::CaptureGameplayQAFrame()
{
#if WITH_EDITOR
	static const TCHAR* PhaseNames[] = {TEXT("Idle"), TEXT("Walk"), TEXT("Run")};
	const TCHAR* GenderName = CurrentCustomizationGender == EUEHHVGender::TypeA ? TEXT("TypeA") : TEXT("TypeB");
	const FString ScreenshotPath = FPaths::ProjectSavedDir() /
		FString::Printf(
			TEXT("Screenshots/Gameplay/HHV_%s_%s.png"),
			GenderName,
			PhaseNames[FMath::Clamp(GameplayQAPhase, 0, 2)]);
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);

	++GameplayQAPhase;
	GetWorldTimerManager().SetTimer(
		GameplayQATimerHandle,
		this,
		&ThisClass::AdvanceGameplayQA,
		0.8f,
		false);
#endif
}
