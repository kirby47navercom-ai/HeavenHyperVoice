#include "UECustomizationPlayerController.h"

#include "../Preview/UECustomizationPreviewActor.h"
#include "../UI/UECustomizationWidget.h"

#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogUECustomization, Log, All);

AUECustomizationPlayerController::AUECustomizationPlayerController()
{
	// The Blueprint child remains designer-editable, while the native class is a safe fallback.
	static ConstructorHelpers::FClassFinder<UUECustomizationWidget> WidgetFinder(
		TEXT("/Game/CharacterCustomization/UI/WBP_CharacterCustomization"));
	CustomizationWidgetClass = UUECustomizationWidget::StaticClass();
	if (WidgetFinder.Succeeded())
	{
		CustomizationWidgetClass = WidgetFinder.Class;
	}
	bShowMouseCursor = true;
}

void AUECustomizationPlayerController::BeginPlay()
{
	Super::BeginPlay();

	for (TActorIterator<AUECustomizationPreviewActor> It(GetWorld()); It; ++It)
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
		UE_LOG(LogUECustomization, Warning, TEXT("Customization preview actor was not found in the current level."));
	}

	TSubclassOf<UUECustomizationWidget> WidgetClassToCreate = CustomizationWidgetClass;
	if (!WidgetClassToCreate)
	{
		WidgetClassToCreate = UUECustomizationWidget::StaticClass();
	}

	CustomizationWidget = CreateWidget<UUECustomizationWidget>(this, WidgetClassToCreate);
	if (CustomizationWidget)
	{
		CustomizationWidget->SetPreviewActor(PreviewActor);
		CustomizationWidget->AddToViewport(WidgetZOrder);
		UE_LOG(LogUECustomization, Log, TEXT("Customization widget '%s' was added to the viewport."),
			*GetNameSafe(CustomizationWidget));
	}
	else
	{
		UE_LOG(LogUECustomization, Error, TEXT("Failed to create customization widget from class '%s'."),
			*GetNameSafe(WidgetClassToCreate));
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
