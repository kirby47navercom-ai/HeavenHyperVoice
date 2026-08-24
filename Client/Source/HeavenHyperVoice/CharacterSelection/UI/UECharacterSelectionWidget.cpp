#include "UECharacterSelectionWidget.h"

#include "../../System/UEGameInstance.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"

namespace UECharacterSelectionWidgetPrivate
{
	const FLinearColor DefaultButtonColor(0.10f, 0.14f, 0.17f, 1.0f);
	const FLinearColor SelectedButtonColor(0.90f, 0.62f, 0.12f, 1.0f);
}

void UUECharacterSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SlotButton0->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSlot0Clicked);
	SlotButton1->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSlot1Clicked);
	SlotButton2->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSlot2Clicked);
	StartButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStartClicked);
	RefreshSlots();
}

void UUECharacterSelectionWidget::SelectSlot(int32 SlotIndex)
{
	UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (!GameInstance || !GameInstance->SelectCharacterSlot(SlotIndex))
	{
		return;
	}

	if (!GameInstance->IsCharacterSlotOccupied(SlotIndex))
	{
		if (CustomizationLevel.IsNull())
		{
			UE_LOG(LogTemp, Error, TEXT("커스터마이징 레벨이 WBP_CharacterSelection 기본값에 지정되지 않았습니다."));
			return;
		}

		UGameplayStatics::OpenLevelBySoftObjectPtr(this, CustomizationLevel);
		return;
	}

	RefreshSlots();
}

void UUECharacterSelectionWidget::StartSelectedCharacter()
{
	UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (!GameInstance || !GameInstance->LoadSelectedSlotAppearance())
	{
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("먼저 생성된 캐릭터를 선택해 줘.")));
		}
		return;
	}

	if (GameplayLevel.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("게임 레벨이 WBP_CharacterSelection 기본값에 지정되지 않았습니다."));
		return;
	}
	if (!GameplayGameModeClass)
	{
		UE_LOG(LogTemp, Error, TEXT("게임 모드가 WBP_CharacterSelection 기본값에 지정되지 않았습니다."));
		return;
	}

	const FString TravelOptions = FString::Printf(
		TEXT("?game=%s"), *GameplayGameModeClass->GetPathName());
	UGameplayStatics::OpenLevelBySoftObjectPtr(
		this,
		GameplayLevel,
		true,
		TravelOptions);
}

void UUECharacterSelectionWidget::RefreshSlots()
{
	UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		return;
	}

	const int32 SelectedSlotIndex = GameInstance->GetSelectedCharacterSlotIndex();
	const TArray<UButton*> Buttons = GetSlotButtons();
	const TArray<UTextBlock*> StateTexts = GetSlotStateTexts();
	for (int32 SlotIndex = 0; SlotIndex < Buttons.Num(); ++SlotIndex)
	{
		const bool bOccupied = GameInstance->IsCharacterSlotOccupied(SlotIndex);
		Buttons[SlotIndex]->SetBackgroundColor(
			SlotIndex == SelectedSlotIndex
				? UECharacterSelectionWidgetPrivate::SelectedButtonColor
				: UECharacterSelectionWidgetPrivate::DefaultButtonColor);
		StateTexts[SlotIndex]->SetText(FText::FromString(
			bOccupied ? TEXT("생성됨") : TEXT("비어 있음\n선택해서 생성")));
	}

	const bool bCanStart = GameInstance->IsCharacterSlotOccupied(SelectedSlotIndex);
	StartButton->SetIsEnabled(bCanStart);
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(
			bCanStart ? TEXT("선택한 캐릭터로 시작할 수 있어.") : TEXT("캐릭터 슬롯을 선택해 줘.")));
	}
}

void UUECharacterSelectionWidget::HandleSlot0Clicked()
{
	SelectSlot(0);
}

void UUECharacterSelectionWidget::HandleSlot1Clicked()
{
	SelectSlot(1);
}

void UUECharacterSelectionWidget::HandleSlot2Clicked()
{
	SelectSlot(2);
}

void UUECharacterSelectionWidget::HandleStartClicked()
{
	StartSelectedCharacter();
}

TArray<UButton*> UUECharacterSelectionWidget::GetSlotButtons() const
{
	return {SlotButton0, SlotButton1, SlotButton2};
}

TArray<UTextBlock*> UUECharacterSelectionWidget::GetSlotStateTexts() const
{
	return {SlotState0, SlotState1, SlotState2};
}
