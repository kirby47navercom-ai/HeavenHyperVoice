#include "UECharacterLobbySlotWidget.h"

#include "../../Character/UEPlayerCharacter.h"
#include "../../Pokemon/UEPokemonCharacter.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Viewport.h"

void UUECharacterLobbySlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너에서는 저장 데이터 대신 블루프린트에 지정한 샘플 값을 보여 준다.
	if (IsDesignTime())
	{
		ApplyViewData(DesignerPreviewData);
	}
}

void UUECharacterLobbySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	OccupiedActionButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleActionClicked);
	AvailableActionButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleActionClicked);
	DeleteButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDeleteClicked);
	ConfirmDeleteButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmDeleteClicked);
	CancelDeleteButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelDeleteClicked);
	SetDeleteConfirmationVisible(false);
	ApplyViewData(ViewData);
}

void UUECharacterLobbySlotWidget::NativeDestruct()
{
	if (OccupiedActionButton)
	{
		OccupiedActionButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleActionClicked);
	}
	if (AvailableActionButton)
	{
		AvailableActionButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleActionClicked);
	}
	if (DeleteButton)
	{
		DeleteButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleDeleteClicked);
	}
	if (ConfirmDeleteButton)
	{
		ConfirmDeleteButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmDeleteClicked);
	}
	if (CancelDeleteButton)
	{
		CancelDeleteButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCancelDeleteClicked);
	}
	DestroyPreviewActors();
	Super::NativeDestruct();
}

void UUECharacterLobbySlotWidget::InitializeSlot(int32 InSlotIndex)
{
	SlotIndex = InSlotIndex;
	SetIntegerText(SlotNumberValueText, SlotIndex + 1);
}

void UUECharacterLobbySlotWidget::ApplyViewData(const FUECharacterLobbySlotViewData& InViewData)
{
	ViewData = InViewData;

	const bool bOccupied = ViewData.State == EUECharacterLobbySlotState::Occupied;
	const bool bAvailable = ViewData.State == EUECharacterLobbySlotState::Available;
	OccupiedPanel->SetVisibility(bOccupied ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	AvailablePanel->SetVisibility(bAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	LockedPanel->SetVisibility(
		ViewData.State == EUECharacterLobbySlotState::Locked
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	SetDeleteConfirmationVisible(false);

	CharacterNameText->SetText(ViewData.CharacterName);
	PartnerNameText->SetText(ViewData.PartnerName);

	if (!IsDesignTime())
	{
		RefreshPreview();
	}
}

void UUECharacterLobbySlotWidget::HandleActionClicked()
{
	if (SlotIndex != INDEX_NONE && ViewData.State != EUECharacterLobbySlotState::Locked)
	{
		OnActionRequested.Broadcast(SlotIndex);
	}
}

void UUECharacterLobbySlotWidget::HandleDeleteClicked()
{
	if (ViewData.State == EUECharacterLobbySlotState::Occupied)
	{
		SetDeleteConfirmationVisible(true);
	}
}

void UUECharacterLobbySlotWidget::HandleConfirmDeleteClicked()
{
	if (SlotIndex != INDEX_NONE && ViewData.State == EUECharacterLobbySlotState::Occupied)
	{
		OnDeleteRequested.Broadcast(SlotIndex);
	}
	SetDeleteConfirmationVisible(false);
}

void UUECharacterLobbySlotWidget::HandleCancelDeleteClicked()
{
	SetDeleteConfirmationVisible(false);
}

void UUECharacterLobbySlotWidget::SetDeleteConfirmationVisible(bool bVisible)
{
	DeleteConfirmationPanel->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	OccupiedActionButton->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	DeleteButton->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UUECharacterLobbySlotWidget::RefreshPreview()
{
	DestroyPreviewActors();
	if (!PreviewViewport || ViewData.State != EUECharacterLobbySlotState::Occupied)
	{
		return;
	}

	// 카메라와 배경 값은 모두 WBP 클래스 기본값에서 넘어온다.
	PreviewViewport->SetBackgroundColor(PreviewBackgroundColor);
	PreviewViewport->SetLightIntensity(PreviewLightIntensity);
	PreviewViewport->SetSkyIntensity(PreviewSkyIntensity);
	PreviewViewport->SetViewLocation(PreviewCameraLocation);
	PreviewViewport->SetViewRotation(PreviewCameraRotation);

	if (CharacterPreviewActorClass)
	{
		AActor* PreviewActor = PreviewViewport->Spawn(CharacterPreviewActorClass);
		if (AUEPlayerCharacter* CharacterPreview = Cast<AUEPlayerCharacter>(PreviewActor))
		{
			CharacterPreview->SetActorTransform(CharacterPreviewTransform);
			CharacterPreview->ApplyHHVAppearance(ViewData.Appearance);
			CharacterPreview->SetActorEnableCollision(false);
			CharacterPreview->SetActorTickEnabled(false);
			SpawnedCharacterPreview = CharacterPreview;
		}
	}

	if (PartnerPreviewActorClass)
	{
		AActor* PreviewActor = PreviewViewport->Spawn(PartnerPreviewActorClass);
		if (AUEPokemonCharacter* PartnerPreview = Cast<AUEPokemonCharacter>(PreviewActor))
		{
			PartnerPreview->SetActorTransform(PartnerPreviewTransform);
			PartnerPreview->SetPokemonSpeciesData(ViewData.PartnerSpecies);
			PartnerPreview->SetActorEnableCollision(false);
			PartnerPreview->SetActorTickEnabled(false);
			SpawnedPartnerPreview = PartnerPreview;
		}
	}
}

void UUECharacterLobbySlotWidget::DestroyPreviewActors()
{
	if (SpawnedCharacterPreview.IsValid())
	{
		SpawnedCharacterPreview->Destroy();
	}
	if (SpawnedPartnerPreview.IsValid())
	{
		SpawnedPartnerPreview->Destroy();
	}

	SpawnedCharacterPreview.Reset();
	SpawnedPartnerPreview.Reset();
}

void UUECharacterLobbySlotWidget::SetIntegerText(UTextBlock* TextBlock, int32 Value) const
{
	if (TextBlock)
	{
		TextBlock->SetText(FText::AsNumber(Value));
	}
}
