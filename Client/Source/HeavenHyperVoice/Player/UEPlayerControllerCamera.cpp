#include "UEPlayerController.h"
#include "../Character/UEPlayerCharacter.h"
#include "../UI/Options/UEPhotoModeWidget.h"
#include "../World/UEGoldenrodCity.h"
#include "../UEGameplayTags.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <ShlObj.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

void AUEPlayerController::ToggleChatVisibility()
{
	ToggleChatVisible();
}

void AUEPlayerController::BindPhotoInput(UEnhancedInputComponent* Input)
{
	if (PhotoCaptureAction) Input->BindAction(PhotoCaptureAction, ETriggerEvent::Started, this, &ThisClass::TakePhoto);
	if (PhotoZoomAction) Input->BindAction(PhotoZoomAction, ETriggerEvent::Triggered, this, &ThisClass::HandlePhotoZoom);
	if (PhotoLookAction)
	{
		Input->BindAction(PhotoLookAction, ETriggerEvent::Started, this, &ThisClass::HandlePhotoLookStarted);
		Input->BindAction(PhotoLookAction, ETriggerEvent::Completed, this, &ThisClass::HandlePhotoLookStopped);
		Input->BindAction(PhotoLookAction, ETriggerEvent::Canceled, this, &ThisClass::HandlePhotoLookStopped);
	}
}

bool AUEPlayerController::IsNearPhotoBoundary() const
{
	return PhotoCity.IsValid() && GetPawn()
		&& PhotoCity->GetBoundaryCameraWeight(GetPawn()->GetActorLocation()) >= .8f;
}

void AUEPlayerController::EnterPhotoMode()
{
	AUEPlayerCharacter* PlayerCharacter = GetControlledPlayerCharacter();
	if (bPhotoMode || bReturningToFrontend || !IsLocalController() || !PlayerCharacter || !PhotoModeWidgetClass || !PhotoMappingContext) return;
	if (bInCombat || PlayerCharacter->IsRolling() || !PlayerCharacter->GetCharacterMovement()->IsMovingOnGround()
		|| PlayerCharacter->GetCharacterStateTag() == UEGameplayTags::State_Character_Damage
		|| PlayerCharacter->GetCharacterStateTag() == UEGameplayTags::State_Character_Death)
	{
		AddSystemMessage(TEXT("지금은 카메라를 사용할 수 없습니다."));
		return;
	}
	PhotoCity.Reset();
	for (TActorIterator<AUEGoldenrodCity> It(GetWorld()); It; ++It) { PhotoCity = *It; break; }
	if (IsNearPhotoBoundary())
	{
		AddSystemMessage(TEXT("맵 경계에서 조금 떨어진 뒤 카메라를 열어 주세요."));
		return;
	}
	PhotoCamera = PlayerCharacter->FindComponentByClass<UCameraComponent>();
	if (!PhotoCamera.IsValid()) return;
	PhotoWidget = CreateWidget<UUEPhotoModeWidget>(this, PhotoModeWidgetClass);
	if (!PhotoWidget) return;
	CloseChatInput(false);
	CloseOptionsMenu();
	bMouseViewHeld = false;
	PhotoPreviousWindowOrder = UIWindowOrder;
	UIWindowOrder.Empty();
	RefreshWindowInput(false);
	PlayerCharacter->SetRunning(false);
	PlayerCharacter->StopJumping();
	PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
	PendingMovementInput = FVector2D::ZeroVector;
	PushMovementInputToCharacter();
	PhotoPawn = PlayerCharacter;
	PhotoOriginalFOV = PhotoCamera->FieldOfView;
	bPhotoMode = true;
	bPhotoLookHeld = false;
	bPhotoSavedClickEvents = bEnableClickEvents;
	bPhotoSavedMouseOverEvents = bEnableMouseOverEvents;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	if (AHUD* HUD = GetHUD()) { bPhotoSavedHUD = HUD->bShowHUD; HUD->bShowHUD = false; }
	HideGameplayUIForPhoto();
	PhotoWidget->AddToViewport(1000);
	SetPhotoZoom(0.f);
	PhotoCamera->SetFieldOfView(PhotoWideFOV);
	if (auto* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		FModifyContextOptions Options;
		Options.bIgnoreAllPressedKeysUntilRelease = true;
		Input->AddMappingContext(PhotoMappingContext, 100, Options);
	}
	RefreshPhotoInput();
}

void AUEPlayerController::ExitPhotoMode()
{
	if (!bPhotoMode) return;
	bPhotoMode = false;
	bPhotoLookHeld = false;
	bEnableClickEvents = bPhotoSavedClickEvents;
	bEnableMouseOverEvents = bPhotoSavedMouseOverEvents;
	if (auto* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		Input->RemoveMappingContext(PhotoMappingContext);
	if (PhotoProcessedHandle.IsValid())
	{
		FScreenshotRequest::OnScreenshotRequestProcessed().Remove(PhotoProcessedHandle);
		PhotoProcessedHandle.Reset();
		// Cancel an unfinished photo before restoring world UI or the third-person camera.
		if (!PendingPhotoFilename.IsEmpty()) FScreenshotRequest::Reset();
		PendingPhotoFilename.Empty();
	}
	if (PhotoWidget) { PhotoWidget->RemoveFromParent(); PhotoWidget = nullptr; }
	for (auto& Entry : PhotoHiddenWidgets)
		if (Entry.Key.IsValid()) Entry.Key->SetVisibility(Entry.Value);
	for (auto& Entry : PhotoHiddenWorldWidgets)
		if (Entry.Key.IsValid()) Entry.Key->SetRenderOpacity(Entry.Value);
	PhotoHiddenWidgets.Empty();
	PhotoHiddenWorldWidgets.Empty();
	UIWindowOrder = MoveTemp(PhotoPreviousWindowOrder);
	if (AHUD* HUD = GetHUD()) HUD->bShowHUD = bPhotoSavedHUD;
	if (PhotoCamera.IsValid()) PhotoCamera->SetFieldOfView(PhotoOriginalFOV);
	PhotoCamera.Reset();
	PhotoPawn.Reset();
	PendingMovementInput = FVector2D::ZeroVector;
	PushMovementInputToCharacter();
	if (auto* PlayerCharacter = GetControlledPlayerCharacter()) PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
	RefreshWindowInput();
}

void AUEPlayerController::SetInCombat(bool bNewInCombat)
{
	bInCombat = bNewInCombat;
	if (bInCombat) ExitPhotoMode();
}

void AUEPlayerController::HideGameplayUIForPhoto()
{
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), true);
	for (UUserWidget* Widget : Widgets)
	{
		if (Widget == PhotoWidget || (Widget->GetOwningPlayer() && Widget->GetOwningPlayer() != this)) continue;
		if (!PhotoHiddenWidgets.Contains(Widget)) PhotoHiddenWidgets.Add(Widget, Widget->GetVisibility());
		Widget->SetVisibility(ESlateVisibility::Collapsed);
	}
	// Screen-space WidgetComponents are separate from AddToViewport widgets.
	for (TObjectIterator<UWidgetComponent> It; It; ++It)
	{
		if (It->GetWorld() != GetWorld()) continue;
		if (UUserWidget* Widget = It->GetUserWidgetObject())
		{
			if (!PhotoHiddenWorldWidgets.Contains(Widget)) PhotoHiddenWorldWidgets.Add(Widget, Widget->GetRenderOpacity());
			Widget->SetRenderOpacity(0.f);
		}
	}
}

void AUEPlayerController::UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents)
{
	Super::UpdateHiddenComponents(ViewLocation, HiddenComponents);
	if (!bPhotoMode || !GetPawn()) return;
	// Hide only in this player's view; remote players still see the photographer normally.
	TInlineComponentArray<UPrimitiveComponent*> Components(GetPawn());
	for (UPrimitiveComponent* Component : Components) HiddenComponents.Add(Component->GetPrimitiveSceneId());
}

void AUEPlayerController::TickPhotoMode(float DeltaSeconds)
{
	if (!bPhotoMode) return;
	auto* PlayerCharacter = GetControlledPlayerCharacter();
	if (!PlayerCharacter || PhotoPawn.Get() != PlayerCharacter || !PhotoCamera.IsValid() || bInCombat || IsNearPhotoBoundary()
		|| PlayerCharacter->GetCharacterStateTag() == UEGameplayTags::State_Character_Damage
		|| PlayerCharacter->GetCharacterStateTag() == UEGameplayTags::State_Character_Death)
	{
		ExitPhotoMode();
		return;
	}
	PlayerCharacter->SetRunning(false);
	HideGameplayUIForPhoto();
	if (AHUD* HUD = GetHUD()) HUD->bShowHUD = false;
	const float Target = FMath::Lerp(PhotoWideFOV, FMath::Min(PhotoTeleFOV, PhotoWideFOV), PhotoZoom);
	PhotoCamera->SetFieldOfView(FMath::FInterpTo(PhotoCamera->FieldOfView, Target, DeltaSeconds, 12.f));
}

void AUEPlayerController::RefreshPhotoInput()
{
	bShowMouseCursor = !bPhotoLookHeld;
	if (bPhotoLookHeld)
	{
		SetInputMode(FInputModeGameOnly());
	}
	else
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen);
		if (PhotoWidget) Mode.SetWidgetToFocus(PhotoWidget->TakeWidget());
		SetInputMode(Mode);
	}
}

void AUEPlayerController::HandlePhotoLookStarted()
{
	if (!bPhotoMode) return;
	bPhotoLookHeld = true;
	RefreshPhotoInput();
}

void AUEPlayerController::HandlePhotoLookStopped()
{
	if (!bPhotoMode || !bPhotoLookHeld) return;
	bPhotoLookHeld = false;
	RefreshPhotoInput();
}

void AUEPlayerController::HandlePhotoZoom(const FInputActionValue& Value)
{
	if (bPhotoMode) SetPhotoZoom(PhotoZoom + Value.Get<float>() * .08f);
}

void AUEPlayerController::SetPhotoZoom(float Value)
{
	if (!bPhotoMode) return;
	PhotoZoom = FMath::Clamp(Value, 0.f, 1.f);
	const float Target = FMath::Lerp(PhotoWideFOV, FMath::Min(PhotoTeleFOV, PhotoWideFOV), PhotoZoom);
	const float Magnification = FMath::Tan(FMath::DegreesToRadians(PhotoWideFOV * .5f)) / FMath::Tan(FMath::DegreesToRadians(Target * .5f));
	if (PhotoWidget) PhotoWidget->UpdateZoom(PhotoZoom, Magnification);
}

void AUEPlayerController::TakePhoto()
{
	if (!bPhotoMode || !PendingPhotoFilename.IsEmpty() || FScreenshotRequest::IsScreenshotRequested()) return;
	FString Folder;
#if PLATFORM_WINDOWS
	PWSTR PicturesPath = nullptr;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, KF_FLAG_CREATE, nullptr, &PicturesPath))) Folder = PicturesPath;
	CoTaskMemFree(PicturesPath);
#endif
	if (Folder.IsEmpty()) Folder = FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir());
	Folder = FPaths::Combine(Folder, TEXT("HeavenHyperVoice"));
	if (!IFileManager::Get().MakeDirectory(*Folder, true))
	{
		if (PhotoWidget) PhotoWidget->SetStatus(FText::FromString(TEXT("사진 폴더를 만들지 못했습니다.")));
		return;
	}
	PendingPhotoFilename = FPaths::Combine(Folder, TEXT("HHV_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_"))
		+ FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8) + TEXT(".png"));
	PhotoProcessedHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &ThisClass::OnPhotoProcessed);
	if (PhotoWidget) PhotoWidget->SetStatus(FText::FromString(TEXT("사진 저장 중…")));
	FScreenshotRequest::RequestScreenshot(PendingPhotoFilename, false, false);
}

void AUEPlayerController::OnPhotoProcessed()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(PhotoProcessedHandle);
	PhotoProcessedHandle.Reset();
	const bool bSaved = IFileManager::Get().FileSize(*PendingPhotoFilename) > 0;
	if (PhotoWidget) PhotoWidget->SetStatus(FText::FromString(bSaved
		? TEXT("사진 저장됨 · ") + PendingPhotoFilename : TEXT("사진을 저장하지 못했습니다.")));
	UE_LOG(LogTemp, Display, TEXT("Photo %s: %s"), bSaved ? TEXT("saved") : TEXT("failed"), *PendingPhotoFilename);
	PendingPhotoFilename.Empty();
}
