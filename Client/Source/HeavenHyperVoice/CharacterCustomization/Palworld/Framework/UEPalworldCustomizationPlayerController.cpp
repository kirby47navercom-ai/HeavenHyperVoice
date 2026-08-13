#include "UEPalworldCustomizationPlayerController.h"

#include "../Preview/UEPalworldCustomizationPreviewActor.h"
#include "../UI/UEPalworldCustomizationWidget.h"

#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogUEPalworldCustomization, Log, All);

AUEPalworldCustomizationPlayerController::AUEPalworldCustomizationPlayerController()
{
	static ConstructorHelpers::FClassFinder<UUEPalworldCustomizationWidget> WidgetFinder(
		TEXT("/Game/CharacterCustomization/Palworld/Blueprints/WBP_PalworldCustomization"));
	CustomizationWidgetClass = UUEPalworldCustomizationWidget::StaticClass();
	if (WidgetFinder.Succeeded())
	{
		CustomizationWidgetClass = WidgetFinder.Class;
	}
	bShowMouseCursor = true;
}

void AUEPalworldCustomizationPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (APawn* DefaultPawn = GetPawn())
	{
		DefaultPawn->SetActorHiddenInGame(true);
		DefaultPawn->SetActorEnableCollision(false);
		DefaultPawn->SetActorTickEnabled(false);
	}

	for (TActorIterator<AUEPalworldCustomizationPreviewActor> It(GetWorld()); It; ++It)
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
		UE_LOG(LogUEPalworldCustomization, Warning, TEXT("Palworld customization preview actor was not found."));
	}

	TSubclassOf<UUEPalworldCustomizationWidget> WidgetClassToCreate = CustomizationWidgetClass;
	if (!WidgetClassToCreate)
	{
		WidgetClassToCreate = UUEPalworldCustomizationWidget::StaticClass();
	}

	CustomizationWidget = CreateWidget<UUEPalworldCustomizationWidget>(this, WidgetClassToCreate);
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
		UE_LOG(LogUEPalworldCustomization, Error, TEXT("Failed to create Palworld customization widget."));
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
