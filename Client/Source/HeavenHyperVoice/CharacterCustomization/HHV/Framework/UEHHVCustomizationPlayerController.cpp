#include "UEHHVCustomizationPlayerController.h"

#include "../Preview/UEHHVCustomizationPreviewActor.h"
#include "../UI/UEHHVCustomizationWidget.h"

#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogUEHHVCustomization, Log, All);

AUEHHVCustomizationPlayerController::AUEHHVCustomizationPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FClassFinder<UUEHHVCustomizationWidget> WidgetFinder(
		TEXT("/Game/CharacterCustomization/HHV/Blueprints/WBP_HHVCustomization"));
	CustomizationWidgetClass = UUEHHVCustomizationWidget::StaticClass();
	if (WidgetFinder.Succeeded())
	{
		CustomizationWidgetClass = WidgetFinder.Class;
	}
	bShowMouseCursor = true;
}

void AUEHHVCustomizationPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (APawn* DefaultPawn = GetPawn())
	{
		DefaultPawn->SetActorHiddenInGame(true);
		DefaultPawn->SetActorEnableCollision(false);
		DefaultPawn->SetActorTickEnabled(false);
	}

	for (TActorIterator<AUEHHVCustomizationPreviewActor> It(GetWorld()); It; ++It)
	{
		PreviewActor = *It;
		break;
	}

	if (PreviewActor)
	{
		SetViewTarget(PreviewActor);
	}
	else
	{
		UE_LOG(LogUEHHVCustomization, Warning, TEXT("HeavenHyperVoice customization preview actor was not found."));
	}

	TSubclassOf<UUEHHVCustomizationWidget> WidgetClassToCreate = CustomizationWidgetClass;
	if (!WidgetClassToCreate)
	{
		WidgetClassToCreate = UUEHHVCustomizationWidget::StaticClass();
	}

	CustomizationWidget = CreateWidget<UUEHHVCustomizationWidget>(this, WidgetClassToCreate);
	if (CustomizationWidget)
	{
		CustomizationWidget->SetPreviewActor(PreviewActor);
		if (PreviewActor)
		{
			CustomizationWidget->SetCatalog(PreviewActor->GetCatalog());
		}
		CustomizationWidget->AddToViewport(WidgetZOrder);
	}
	else
	{
		UE_LOG(LogUEHHVCustomization, Error, TEXT("Failed to create HeavenHyperVoice customization widget."));
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (CustomizationWidget)
	{
		InputMode.SetWidgetToFocus(CustomizationWidget->TakeWidget());
	}
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void AUEHHVCustomizationPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	// 마우스 휠은 Input Mapping Context 없이도 커마 화면에서 바로 확대/축소되게 묶는다.
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ThisClass::HandlePreviewZoomIn);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ThisClass::HandlePreviewZoomOut);
}

void AUEHHVCustomizationPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	HandlePreviewMouseDrag();
}

bool AUEHHVCustomizationPlayerController::IsMouseOverPreviewArea(const FVector2D& MousePosition) const
{
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return false;
	}

	// 좌우 UI 패널 위에서는 버튼/스크롤을 우선하고, 가운데 캐릭터 영역에서만 드래그를 받는다.
	return MousePosition.X > ViewportX * 0.16f &&
		MousePosition.X < ViewportX * 0.86f &&
		MousePosition.Y > ViewportY * 0.02f &&
		MousePosition.Y < ViewportY * 0.98f;
}

void AUEHHVCustomizationPlayerController::HandlePreviewMouseDrag()
{
	if (!PreviewActor)
	{
		bDraggingPreview = false;
		bPanningPreview = false;
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		bDraggingPreview = false;
		bPanningPreview = false;
		return;
	}

	const FVector2D CurrentMousePosition(MouseX, MouseY);
	const bool bLeftDown = IsInputKeyDown(EKeys::LeftMouseButton);
	const bool bPanDown = IsInputKeyDown(EKeys::RightMouseButton) || IsInputKeyDown(EKeys::MiddleMouseButton);

	if (!bLeftDown && !bPanDown)
	{
		bDraggingPreview = false;
		bPanningPreview = false;
		LastMousePosition = CurrentMousePosition;
		return;
	}

	if (!bDraggingPreview && !bPanningPreview)
	{
		if (!IsMouseOverPreviewArea(CurrentMousePosition))
		{
			LastMousePosition = CurrentMousePosition;
			return;
		}

		bDraggingPreview = bLeftDown;
		bPanningPreview = bPanDown;
		LastMousePosition = CurrentMousePosition;
		return;
	}

	const FVector2D Delta = CurrentMousePosition - LastMousePosition;
	LastMousePosition = CurrentMousePosition;

	if (bDraggingPreview)
	{
		PreviewActor->AddPreviewYaw(Delta.X * 0.28f);
	}
	else if (bPanningPreview)
	{
		PreviewActor->AddPreviewPan(Delta);
	}
}

void AUEHHVCustomizationPlayerController::HandlePreviewZoomIn()
{
	if (PreviewActor)
	{
		PreviewActor->AddPreviewZoom(-0.12f);
	}
}

void AUEHHVCustomizationPlayerController::HandlePreviewZoomOut()
{
	if (PreviewActor)
	{
		PreviewActor->AddPreviewZoom(0.12f);
	}
}
