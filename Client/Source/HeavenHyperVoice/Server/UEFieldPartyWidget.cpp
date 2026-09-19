#include "UEFieldPartyWidget.h"
#include "../Data/UEProjectAssets.h"


#include "UEFieldServerBridgeComponent.h"
#include "../Character/UEPlayerCharacter.h"
#include "../Pokemon/UEPokemonSpeciesCatalog.h"
#include "../Pokemon/UEPokemonSpeciesData.h"
#include "../System/UEGameInstance.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WrapBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"

void UUEFieldPartyEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (SelectButton)
	{
		SelectButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
	}
	ApplyEntryData();
}

void UUEFieldPartyEntryWidget::NativeDestruct()
{
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
	}
	Super::NativeDestruct();
}

void UUEFieldPartyEntryWidget::Setup(UUEFieldPartyEntryData* InEntryData)
{
	EntryData = InEntryData;
	ApplyEntryData();
}

void UUEFieldPartyEntryWidget::ApplyEntryData()
{
	if (!EntryData)
	{
		return;
	}

	if (LabelText)
	{
		LabelText->SetText(EntryData->Label);
		LabelText->SetColorAndOpacity(
			EntryData->bLocked ? FSlateColor(FLinearColor(0.30f, 0.30f, 0.34f, 1.0f)) : FSlateColor(FLinearColor::White));
	}

	if (IconImage)
	{
		UTexture2D* Portrait = EntryData->Species ? EntryData->Species->ProfileIcon : nullptr;
		if (Portrait)
		{
			// 크기는 SizeBox 가 정한다. 텍스처 크기를 따라가면 칸마다 들쭉날쭉해진다.
			IconImage->SetBrushFromTexture(Portrait, /*bMatchSize=*/false);
			IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);

			// 해금 전에는 회색으로 죽인다. 무엇이 있는지는 보이되 고를 수 없다.
			IconImage->SetColorAndOpacity(EntryData->bLocked ? FLinearColor(0.45f, 0.45f, 0.45f) : FLinearColor::White);
		}
		else
		{
			// 초상화가 없는 종족. 빈 브러시를 그리면 흰 사각형이 남는다.
			IconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (SelectButton)
	{
		SelectButton->SetBackgroundColor(EntryData->bLocked ? FLinearColor(0.75f, 0.75f, 0.75f) : FLinearColor::White);

		// 해금하지 않은 칸은 눌리지 않는다. 눌러 봐야 서버가 거절할 뿐이다.
		SelectButton->SetIsEnabled(!EntryData->bLocked);
	}

	if (SelectionBorder)
	{
		const FLinearColor Border = EntryData->bActive
			? FLinearColor(1.0f, 0.85f, 0.35f, 1.0f)
			: (EntryData->PartySlot > 0 ? FLinearColor(0.86f, 0.62f, 0.16f, 1.0f) : FLinearColor::Transparent);
		SelectionBorder->SetBrushColor(Border);
	}

	if (SlotBadge)
	{
		SlotBadge->SetVisibility(EntryData->PartySlot > 0
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
		SlotBadge->SetBrushColor(EntryData->bActive ? FLinearColor(1.0f, 0.85f, 0.35f, 1.0f) : FLinearColor(0.86f, 0.62f, 0.16f, 1.0f));
	}
	if (SlotBadgeText && EntryData->PartySlot > 0)
	{
		SlotBadgeText->SetText(FText::AsNumber(EntryData->PartySlot));
		SlotBadgeText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	}
}

void UUEFieldPartyEntryWidget::HandleClicked()
{
	if (EntryData && EntryData->Owner)
	{
		EntryData->Owner->ToggleMember(EntryData->DexNumber);
	}
}

// --- 화면 ---------------------------------------------------------------

void UUEFieldPartyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ResetButton)
	{
		ResetButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResetClicked);
	}
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}

	BuildTypeTabs();

	if (UUEFieldServerBridgeComponent* Bridge = FindBridge())
	{
		Bridge->OnPartyStateChanged.AddUniqueDynamic(this, &ThisClass::HandlePartyStateChanged);
	}

	// 열 때는 서버가 마지막으로 알려준 상태에서 시작한다.
	HandlePartyStateChanged();


}

FReply UUEFieldPartyWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		if (UUEFieldServerBridgeComponent* Bridge = FindBridge())
		{
			Bridge->TogglePartyWidget();
		}
		else
		{
			Close();
		}
		return FReply::Handled();
	}

	// 파티 자리 번호로 꺼낸다. 화면의 배지 번호와 같은 키다.
	const FKey Key = KeyEvent.GetKey();
	if (Key == EKeys::One || Key == EKeys::NumPadOne)
	{
		SetActiveSlot(1);
		return FReply::Handled();
	}
	if (Key == EKeys::Two || Key == EKeys::NumPadTwo)
	{
		SetActiveSlot(2);
		return FReply::Handled();
	}
	if (Key == EKeys::Three || Key == EKeys::NumPadThree)
	{
		SetActiveSlot(3);
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(Geometry, KeyEvent);
}

void UUEFieldPartyWidget::NativeDestruct()
{
	if (ResetButton)
	{
		ResetButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetClicked);
	}
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}
	if (UUEFieldServerBridgeComponent* Bridge = FindBridge())
	{
		// 창을 닫을 때 확인 버튼을 누르지 않았더라도 바뀐 내용만 서버에 저장한다.
		// 단순히 열었다 닫은 경우에는 같은 값을 다시 보내지 않아 불필요한 DB 쓰기를 막는다.
		const FUEFieldPartyState& Saved = Bridge->GetPartyState();
		if (PendingParty != Saved.Party || PendingActive != Saved.ActiveDex)
		{
			Bridge->SendSetParty(PendingParty, PendingActive);
		}

		Bridge->OnPartyStateChanged.RemoveDynamic(this, &ThisClass::HandlePartyStateChanged);
	}


	Super::NativeDestruct();
}

UUEFieldServerBridgeComponent* UUEFieldPartyWidget::FindBridge() const
{
	return UUEFieldServerBridgeComponent::Find(GetOwningPlayer());
}

UUEPokemonSpeciesCatalog* UUEFieldPartyWidget::ResolveCatalog() const
{
	if (SpeciesCatalog)
	{
		return SpeciesCatalog;
	}

	// 별도 카탈로그를 지정하지 않으면 로비가 쓰는 표를 빌린다.
	const UWorld* World = GetWorld();
	UUEGameInstance* GameInstance =
		World ? Cast<UUEGameInstance>(World->GetGameInstance()) : nullptr;
	return GameInstance ? GameInstance->GetPartySpeciesCatalog() : nullptr;
}

void UUEFieldPartyWidget::ToggleMember(int32 DexNumber)
{
	if (DexNumber <= 0)
	{
		return;
	}

	const int32 Index = PendingParty.Find(DexNumber);
	if (Index != INDEX_NONE)
	{
		PendingParty.RemoveAt(Index);

		// 꺼내 놓은 것을 뺐으면 나와 있는 것이 없어진다. 남은 것으로 멋대로
		// 옮기지 않는다 — 무엇이 나올지는 1/2/3 키로 사용자가 정한다.
		if (PendingActive == DexNumber)
		{
			PendingActive = 0;
		}
		RebuildList();
		return;
	}

	if (PendingParty.Num() >= MaxPartySize)
	{
		SetStatus(NSLOCTEXT("HHV", "PartyFull", "파티는 3마리까지입니다"));
		return;
	}

	PendingParty.Add(DexNumber);

	// 첫 마리는 자동으로 꺼낸다. 파티만 채우고 아무도 안 꺼낸 채 확인을
	// 누르면 파트너가 사라져서 실수처럼 보인다.
	if (PendingActive == 0)
	{
		PendingActive = DexNumber;
	}
	RebuildList();
}

void UUEFieldPartyWidget::SetActiveSlot(int32 SlotNumber)
{
	if (!PendingParty.IsValidIndex(SlotNumber - 1))
	{
		SetStatus(NSLOCTEXT("HHV", "PartySlotEmpty", "그 자리에 포켓몬이 없습니다"));
		return;
	}

	const int32 DexNumber = PendingParty[SlotNumber - 1];

	// 이미 나와 있으면 도로 집어넣는다. 같은 키가 꺼내기와 집어넣기를 겸한다.
	PendingActive = PendingActive == DexNumber ? 0 : DexNumber;
	RebuildList();
}

void UUEFieldPartyWidget::Confirm()
{
	UUEFieldServerBridgeComponent* Bridge = FindBridge();
	if (!Bridge || !Bridge->SendSetParty(PendingParty, PendingActive))
	{
		SetStatus(NSLOCTEXT("HHV", "PartyNoConnection", "필드 서버에 연결되어 있지 않습니다"));
	}
}

void UUEFieldPartyWidget::ResetParty()
{
	PendingParty.Reset();
	PendingActive = 0;
	RebuildList();

	// 비운 결과는 아직 편집 중이며, 확인하거나 닫을 때 서버에 전달된다.
	SetStatus(NSLOCTEXT("HHV", "PartyResetDone", "파티를 비웠습니다"));
}

void UUEFieldPartyWidget::Close()
{
	RemoveFromParent();
}

void UUEFieldPartyWidget::HandleResetClicked()
{
	ResetParty();
}

void UUEFieldPartyWidget::HandleConfirmClicked()
{
	Confirm();
}

void UUEFieldPartyWidget::HandleCloseClicked()
{
	Close();
}

void UUEFieldPartyWidget::HandlePartyStateChanged()
{
	const UUEFieldServerBridgeComponent* Bridge = FindBridge();
	if (!Bridge)
	{
		return;
	}

	// 서버 상태가 권위다. 편집 중이던 내용은 버린다 — 거절당한 변경을 화면에
	// 남겨 두면 사용자는 저장된 줄 안다.
	const FUEFieldPartyState& State = Bridge->GetPartyState();
	PendingParty = State.Party;
	PendingActive = State.ActiveDex;

	if (!State.Message.IsEmpty())
	{
		SetStatus(FText::FromString(State.Message));
	}
	RebuildList();
}

namespace
{
	constexpr float kTypeIconSize = 34.f;

	UTexture2D* LoadTypeIcon(EUEPokemonType Type)
	{
		const UUEProjectAssets* Assets = UUEProjectAssetSettings::GetProjectAssets();
		const auto* Icon = Assets ? Assets->TypeIcons.Find(Type) : nullptr;
		return Icon ? Icon->LoadSynchronous() : nullptr;
	}

	// 화면에 쓰는 이름은 UENUM 의 DisplayName 이다 (불꽃, 물). 한글을 코드에
	// 두 번 적지 않는다.
	FText TypeDisplayName(EUEPokemonType Type)
	{
		const UEnum* Enum = StaticEnum<EUEPokemonType>();
		return Enum ? Enum->GetDisplayNameTextByValue(static_cast<int64>(Type)) : FText::GetEmpty();
	}
}

void UUEFieldPartyWidget::BuildTypeTabs()
{
	if (!TypeTabBar || !WidgetTree)
	{
		// WBP 에 자리가 없다. 탭 없이 전체 목록만 나온다.
		return;
	}

	TypeTabBar->ClearChildren();
	TypeTabButtons.Reset();

	// AddUniqueDynamic 은 함수 이름을 컴파일 타임에 문자열로 박는다. 표에 담은
	// 런타임 함수 포인터로는 걸 수 없어서 여섯 번 손으로 건다.
	if (UButton* Button = MakeTypeTab(EUEPokemonType::None))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UUEFieldPartyWidget::FilterAll);
	}
	if (UButton* Button = MakeTypeTab(EUEPokemonType::Fire))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UUEFieldPartyWidget::FilterFire);
	}
	if (UButton* Button = MakeTypeTab(EUEPokemonType::Water))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UUEFieldPartyWidget::FilterWater);
	}
	if (UButton* Button = MakeTypeTab(EUEPokemonType::Grass))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UUEFieldPartyWidget::FilterGrass);
	}
	if (UButton* Button = MakeTypeTab(EUEPokemonType::Electric))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UUEFieldPartyWidget::FilterElectric);
	}
	if (UButton* Button = MakeTypeTab(EUEPokemonType::Normal))
	{
		Button->OnClicked.AddUniqueDynamic(this, &UUEFieldPartyWidget::FilterNormal);
	}

	// 처음에는 전체가 눌려 있다.
	SetTypeFilter(EUEPokemonType::None);
}

UButton* UUEFieldPartyWidget::MakeTypeTab(EUEPokemonType Type)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	if (!Button)
	{
		return nullptr;
	}

	if (UTexture2D* Texture = LoadTypeIcon(Type))
	{
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (Icon)
		{
			// 크기는 여기서 정한다. 텍스처 크기를 따라가면 탭마다 들쭉날쭉해진다.
			Icon->SetBrushFromTexture(Texture, /*bMatchSize=*/false);
			Icon->SetDesiredSizeOverride(FVector2D(kTypeIconSize, kTypeIconSize));
			Button->AddChild(Icon);
		}
	}
	else
	{
		// 전체 탭에는 아이콘이 없다. 글자로 둔다.
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (Label)
		{
			Label->SetText(NSLOCTEXT("HHV", "PartyTypeAll", "전체"));
			Button->AddChild(Label);
		}
	}

	TypeTabBar->AddChild(Button);
	TypeTabButtons.Add(Button);
	return Button;
}

void UUEFieldPartyWidget::SetTypeFilter(EUEPokemonType Type)
{
	TypeFilter = Type;

	// 고른 탭만 밝게 둔다. 목록 칸이 잠금을 표시하는 것과 같은 방식이다.
	for (int32 Index = 0; Index < TypeTabButtons.Num(); ++Index)
	{
		if (UButton* Button = TypeTabButtons[Index])
		{
			const bool bSelected = Index == static_cast<int32>(Type);
			Button->SetBackgroundColor(bSelected ? FLinearColor::White
			                                     : FLinearColor(0.55f, 0.55f, 0.6f));
		}
	}

	RebuildList();
}

void UUEFieldPartyWidget::FilterAll() { SetTypeFilter(EUEPokemonType::None); }
void UUEFieldPartyWidget::FilterFire() { SetTypeFilter(EUEPokemonType::Fire); }
void UUEFieldPartyWidget::FilterWater() { SetTypeFilter(EUEPokemonType::Water); }
void UUEFieldPartyWidget::FilterGrass() { SetTypeFilter(EUEPokemonType::Grass); }
void UUEFieldPartyWidget::FilterElectric() { SetTypeFilter(EUEPokemonType::Electric); }
void UUEFieldPartyWidget::FilterNormal() { SetTypeFilter(EUEPokemonType::Normal); }

void UUEFieldPartyWidget::RebuildList()
{
	const UUEFieldServerBridgeComponent* Bridge = FindBridge();
	if (!Bridge || !PokemonList || !EntryWidgetClass)
	{
		return;
	}

	UUEPokemonSpeciesCatalog* Catalog = ResolveCatalog();
	if (!Catalog)
	{
		SetStatus(NSLOCTEXT("HHV", "PartyNoCatalog",
			"종족 카탈로그가 지정되지 않았습니다 (공통 에셋 연결표의 SpeciesCatalog)"));
		return;
	}

	// 속성을 지정한 종족만 표시한다. PokemonType=None은 아직 필드에 싣지 않은 종족이다.
	TArray<UUEPokemonSpeciesData*> Ordered;
	Ordered.Reserve(Catalog->Species.Num());
	for (UUEPokemonSpeciesData* Entry : Catalog->Species)
	{
		if (!Entry || Entry->DexNumber <= 0 || Entry->PokemonType == EUEPokemonType::None)
		{
			continue;
		}
		// 전체 탭(None)이면 거르지 않는다.
		if (TypeFilter != EUEPokemonType::None && Entry->PokemonType != TypeFilter)
		{
			continue;
		}
		Ordered.Add(Entry);
	}

	// 속성별로 모은 뒤 같은 속성 안에서는 도감번호 순으로 정렬한다.
	// 카탈로그 배열 자체는 바꾸지 않아 배열 위치를 쓰는 기존 경로에 영향을 주지 않는다.
	Ordered.Sort([](const UUEPokemonSpeciesData& Left, const UUEPokemonSpeciesData& Right)
	{
		if (Left.PokemonType != Right.PokemonType)
		{
			return static_cast<uint8>(Left.PokemonType) < static_cast<uint8>(Right.PokemonType);
		}
		return Left.DexNumber < Right.DexNumber;
	});

	// 서버가 모르는 종족이 카탈로그에 들어 있으면 영영 잠긴 칸으로 보인다.
	// 잘못 눌러도 서버가 거절하므로 조용히 그대로 둔다.
	const TArray<int32>& Unlocked = Bridge->GetPartyState().Unlocked;

	PokemonList->ClearChildren();

	// 속성이 바뀌는 자리마다 머리글을 끼운다. 줄바꿈을 강제해야 머리글이 앞
	// 구역 마지막 줄에 얹히지 않는다. 97765ba6 이 화면을 WBP 로 옮기면서 이
	// 구역 나누기가 통째로 사라졌었다.
	EUEPokemonType LastType = EUEPokemonType::None;

	for (UUEPokemonSpeciesData* Species : Ordered)
	{
		const bool bStartsGroup = Species->PokemonType != LastType;
		if (bStartsGroup && WidgetTree)
		{
			LastType = Species->PokemonType;
			UHorizontalBox* Header =
				WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UTexture2D* Texture = LoadTypeIcon(LastType))
			{
				UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				Icon->SetBrushFromTexture(Texture, /*bMatchSize=*/false);
				Icon->SetDesiredSizeOverride(FVector2D(kTypeIconSize, kTypeIconSize));
				Header->AddChild(Icon);
			}
			UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Name->SetText(TypeDisplayName(LastType));
			Header->AddChild(Name);

			if (UWrapBoxSlot* HeaderSlot = Cast<UWrapBoxSlot>(PokemonList->AddChild(Header)))
			{
				HeaderSlot->SetNewLine(true);
				HeaderSlot->SetFillEmptySpace(true);
				HeaderSlot->SetPadding(FMargin(6.0f, 12.0f, 6.0f, 4.0f));
			}
		}

		UUEFieldPartyEntryData* Entry = NewObject<UUEFieldPartyEntryData>(this);
		Entry->DexNumber = Species->DexNumber;
		Entry->Species = Species;
		Entry->Owner = this;
		Entry->bLocked = !Unlocked.Contains(Species->DexNumber);
		Entry->PartySlot = PendingParty.Find(Species->DexNumber) + 1;  // 못 찾으면 0
		Entry->bActive = PendingActive == Species->DexNumber;

		// 종족 데이터가 표시 이름을 들고 있으면 그것을 쓴다. 비어 있으면
		// 에셋 이름에서 접두사만 떼어 쓴다 — 이름을 코드에 박지 않는다.
		if (!Species->DisplayName.IsEmpty())
		{
			Entry->Label = Species->DisplayName;
		}
		else
		{
			FString DisplayName = Species->GetName();
			DisplayName.RemoveFromStart(TEXT("DA_"));
			Entry->Label = FText::FromString(DisplayName);
		}

		UUEFieldPartyEntryWidget* EntryWidget =
			CreateWidget<UUEFieldPartyEntryWidget>(this, EntryWidgetClass);
		if (!EntryWidget)
		{
			continue;
		}
		EntryWidget->Setup(Entry);

		if (UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(PokemonList->AddChild(EntryWidget)))
		{
			WrapSlot->SetPadding(FMargin(4.0f));

			// 머리글 바로 뒤 칸은 새 줄에서 시작한다. 머리글이 가로로 늘어나
			// 있어도 첫 칸이 그 옆에 끼지 않는다.
			WrapSlot->SetNewLine(bStartsGroup);
		}
	}
}

void UUEFieldPartyWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
}
