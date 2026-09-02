#include "UEFieldPartyWidget.h"

#include "UEFieldServerBridgeComponent.h"
#include "../Character/UEPlayerCharacter.h"
#include "../Pokemon/UEPokemonSpeciesCatalog.h"
#include "../Pokemon/UEPokemonSpeciesData.h"
#include "../System/UEGameInstance.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "GameFramework/PlayerController.h"

namespace
{
// 기본 배치에서만 쓰는 값들. WBP 를 만들면 전부 그쪽 것이 된다.
//
// 스무 종이 일곱 칸씩 세 줄에 다 들어가게 잡았다. 종족이 늘면 스크롤이 생긴다.
constexpr float kEntryWidth = 150.0f;
constexpr float kEntryHeight = 158.0f;
constexpr float kIconSize = 102.0f;
constexpr float kBorderThickness = 3.0f;
constexpr float kBadgeSize = 28.0f;

constexpr float kPanelWidth = 1240.0f;
constexpr float kPanelHeight = 900.0f;

const FLinearColor kPanelColor(0.02f, 0.02f, 0.04f, 0.94f);
const FLinearColor kEntryColor(0.14f, 0.14f, 0.18f, 1.0f);
const FLinearColor kLockedColor(0.09f, 0.09f, 0.11f, 1.0f);
const FLinearColor kLockedTint(0.30f, 0.30f, 0.34f, 1.0f);

// 파티에 든 것은 노란 테두리, 그중 꺼내 놓은 한 마리는 더 밝게.
const FLinearColor kPartyBorder(0.86f, 0.62f, 0.16f, 1.0f);
const FLinearColor kActiveBorder(1.0f, 0.85f, 0.35f, 1.0f);
const FLinearColor kNoBorder(0.0f, 0.0f, 0.0f, 0.0f);

UTextBlock* MakeLabel(UWidgetTree& Tree, const FText& Text)
{
	UTextBlock* Label = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Label->SetText(Text);
	return Label;
}
}  // namespace

// --- 항목 ---------------------------------------------------------------

TSharedRef<SWidget> UUEFieldPartyEntryWidget::RebuildWidget()
{
	// WBP 가 자식을 들고 있으면 그대로 쓴다. 비어 있을 때만 기본 배치를 만든다.
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		USizeBox* Sizer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Sizer->SetWidthOverride(kEntryWidth);
		Sizer->SetHeightOverride(kEntryHeight);
		WidgetTree->RootWidget = Sizer;

		// 테두리를 버튼 바깥에 두른다. 버튼 색으로 표시하면 눌린 상태와 선택
		// 상태가 같은 색을 두고 다툰다.
		SelectionBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
			TEXT("SelectionBorder"));
		SelectionBorder->SetPadding(FMargin(kBorderThickness));
		Sizer->AddChild(SelectionBorder);

		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		SelectionBorder->AddChild(Stack);

		UButton* Button =
			WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SelectButton"));
		SelectButton = Button;
		if (UOverlaySlot* ButtonSlot = Cast<UOverlaySlot>(Stack->AddChild(Button)))
		{
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// 초상화 위에 이름. 버튼은 자식 하나만 받으므로 세로 상자를 끼운다.
		UVerticalBox* Column =
			WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Button->AddChild(Column);

		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconBox->SetWidthOverride(kIconSize);
		IconBox->SetHeightOverride(kIconSize);
		if (UVerticalBoxSlot* IconSlot = Cast<UVerticalBoxSlot>(Column->AddChild(IconBox)))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
		}

		IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("IconImage"));
		IconBox->AddChild(IconImage);

		LabelText = MakeLabel(*WidgetTree, FText::GetEmpty());
		LabelText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* LabelSlot = Cast<UVerticalBoxSlot>(Column->AddChild(LabelText)))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
		}

		// 파티 번호는 칸 위에 겹쳐 띄운다. 흐름에 넣으면 파티에 든 칸만 키가 달라진다.
		USizeBox* BadgeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BadgeBox->SetWidthOverride(kBadgeSize);
		BadgeBox->SetHeightOverride(kBadgeSize);
		if (UOverlaySlot* BadgeSlot = Cast<UOverlaySlot>(Stack->AddChild(BadgeBox)))
		{
			BadgeSlot->SetHorizontalAlignment(HAlign_Left);
			BadgeSlot->SetVerticalAlignment(VAlign_Top);
		}

		SlotBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotBadge"));
		BadgeBox->AddChild(SlotBadge);

		SlotBadgeText = MakeLabel(*WidgetTree, FText::GetEmpty());
		SlotBadgeText->SetJustification(ETextJustify::Center);
		SlotBadge->AddChild(SlotBadgeText);
	}

	return Super::RebuildWidget();
}

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
			EntryData->bLocked ? FSlateColor(kLockedTint) : FSlateColor(FLinearColor::White));
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
			IconImage->SetColorAndOpacity(EntryData->bLocked ? kLockedTint : FLinearColor::White);
		}
		else
		{
			// 초상화가 없는 종족. 빈 브러시를 그리면 흰 사각형이 남는다.
			IconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (SelectButton)
	{
		SelectButton->SetBackgroundColor(EntryData->bLocked ? kLockedColor : kEntryColor);

		// 해금하지 않은 칸은 눌리지 않는다. 눌러 봐야 서버가 거절할 뿐이다.
		SelectButton->SetIsEnabled(!EntryData->bLocked);
	}

	if (SelectionBorder)
	{
		const FLinearColor Border = EntryData->bActive
			? kActiveBorder
			: (EntryData->PartySlot > 0 ? kPartyBorder : kNoBorder);
		SelectionBorder->SetBrushColor(Border);
	}

	if (SlotBadge)
	{
		SlotBadge->SetVisibility(EntryData->PartySlot > 0
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
		SlotBadge->SetBrushColor(EntryData->bActive ? kActiveBorder : kPartyBorder);
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

TSharedRef<SWidget> UUEFieldPartyWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		UCanvasPanel* Canvas =
			WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(kPanelColor);
		Panel->SetPadding(FMargin(24.0f));

		if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(Canvas->AddChild(Panel)))
		{
			// 화면 한가운데. 해상도가 달라져도 자리가 유지된다.
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetSize(FVector2D(kPanelWidth, kPanelHeight));
		}

		UVerticalBox* Column =
			WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->AddChild(Column);

		Column->AddChild(MakeLabel(*WidgetTree, NSLOCTEXT("HHV", "PartyTitle", "모든 포켓몬")));
		Column->AddChild(MakeLabel(*WidgetTree, NSLOCTEXT("HHV", "PartyHint",
			"눌러서 파티에 넣고 빼기 (최대 3마리) · 1 2 3 키로 꺼내고 집어넣기")));

		// 종족이 늘어나면 화면 밖으로 넘친다. 목록만 스크롤한다.
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		if (UVerticalBoxSlot* ScrollSlot = Cast<UVerticalBoxSlot>(Column->AddChild(Scroll)))
		{
			ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		UWrapBox* Grid =
			WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("PokemonList"));
		PokemonList = Grid;
		Scroll->AddChild(Grid);

		StatusText = MakeLabel(*WidgetTree, FText::GetEmpty());
		Column->AddChild(StatusText);

		UHorizontalBox* Buttons =
			WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChild(Buttons);

		ConfirmButton =
			WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmButton"));
		ConfirmButton->AddChild(MakeLabel(*WidgetTree, NSLOCTEXT("HHV", "PartyConfirm", "확인")));
		Buttons->AddChild(ConfirmButton);

		CloseButton =
			WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
		CloseButton->AddChild(MakeLabel(*WidgetTree, NSLOCTEXT("HHV", "PartyClose", "닫기")));
		Buttons->AddChild(CloseButton);
	}

	return Super::RebuildWidget();
}

void UUEFieldPartyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}

	if (UUEFieldServerBridgeComponent* Bridge = FindBridge())
	{
		Bridge->OnPartyStateChanged.AddUniqueDynamic(this, &ThisClass::HandlePartyStateChanged);
	}

	// 열 때는 서버가 마지막으로 알려준 상태에서 시작한다.
	HandlePartyStateChanged();

	// 커서만 켜면 마우스가 그대로 게임으로도 간다 — 목록을 고르는 동안 뒤에서
	// 카메라가 같이 돈다. 창이 떠 있는 동안은 입력을 UI 가 독점한다.
	SetIsFocusable(true);
	if (APlayerController* Controller = GetOwningPlayer())
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(Mode);
		Controller->SetShowMouseCursor(true);
	}
}

FReply UUEFieldPartyWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	// UI 가 입력을 독점하는 동안에는 열 때 쓴 키가 안 먹는다. 나갈 길을 하나
	// 더 둔다 — 닫기 버튼을 못 찾으면 갇힌 것처럼 보인다.
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		Close();
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
		Bridge->OnPartyStateChanged.RemoveDynamic(this, &ThisClass::HandlePartyStateChanged);
	}

	if (APlayerController* Controller = GetOwningPlayer())
	{
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->SetShowMouseCursor(false);
	}

	Super::NativeDestruct();
}

UUEFieldServerBridgeComponent* UUEFieldPartyWidget::FindBridge() const
{
	APlayerController* Controller = GetOwningPlayer();
	if (!Controller)
	{
		return nullptr;
	}

	// 브릿지는 UUEFieldClientSubsystem 이 **컨트롤러**에 붙인다. 폰에서만 찾으면
	// 언제나 못 찾고, 목록이 조용히 빈 채로 뜬다.
	if (UUEFieldServerBridgeComponent* Bridge =
			Controller->FindComponentByClass<UUEFieldServerBridgeComponent>())
	{
		return Bridge;
	}

	// 붙는 자리가 바뀌어도 화면이 죽지 않게 폰도 본다.
	APawn* Pawn = Controller->GetPawn();
	return Pawn ? Pawn->FindComponentByClass<UUEFieldServerBridgeComponent>() : nullptr;
}

UUEPokemonSpeciesCatalog* UUEFieldPartyWidget::ResolveCatalog() const
{
	if (SpeciesCatalog)
	{
		return SpeciesCatalog;
	}

	// WBP 없이 뜰 때는 지정할 곳이 없다. 로비가 쓰는 것과 같은 표를 빌린다.
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

void UUEFieldPartyWidget::Close()
{
	RemoveFromParent();
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

void UUEFieldPartyWidget::RebuildList()
{
	const UUEFieldServerBridgeComponent* Bridge = FindBridge();
	if (!Bridge || !PokemonList)
	{
		return;
	}

	UUEPokemonSpeciesCatalog* Catalog = ResolveCatalog();
	if (!Catalog)
	{
		SetStatus(NSLOCTEXT("HHV", "PartyNoCatalog",
			"종족 카탈로그가 지정되지 않았습니다 (DefaultGame.ini 의 SpeciesCatalog)"));
		return;
	}

	// 도감번호 순으로 늘어놓는다. 카탈로그 배열은 등록한 차례라 뒤죽박죽이고,
	// 그 순서를 바꾸면 배열 위치를 종족 id 로 쓰는 옛 경로가 밀린다.
	TArray<UUEPokemonSpeciesData*> Ordered;
	Ordered.Reserve(Catalog->Species.Num());
	for (UUEPokemonSpeciesData* Entry : Catalog->Species)
	{
		if (Entry && Entry->DexNumber > 0)
		{
			Ordered.Add(Entry);
		}
	}
	Ordered.Sort([](const UUEPokemonSpeciesData& Left, const UUEPokemonSpeciesData& Right)
	{
		return Left.DexNumber < Right.DexNumber;
	});

	// 서버가 모르는 종족이 카탈로그에 들어 있으면 영영 잠긴 칸으로 보인다.
	// 잘못 눌러도 서버가 거절하므로 조용히 그대로 둔다.
	const TArray<int32>& Unlocked = Bridge->GetPartyState().Unlocked;

	PokemonList->ClearChildren();
	for (UUEPokemonSpeciesData* Species : Ordered)
	{
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
			CreateWidget<UUEFieldPartyEntryWidget>(this, UUEFieldPartyEntryWidget::StaticClass());
		if (!EntryWidget)
		{
			continue;
		}
		EntryWidget->Setup(Entry);

		if (UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(PokemonList->AddChild(EntryWidget)))
		{
			WrapSlot->SetPadding(FMargin(4.0f));
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
