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
constexpr float kEntryWidth = 150.0f;
constexpr float kEntryHeight = 44.0f;

const FLinearColor kPanelColor(0.02f, 0.02f, 0.04f, 0.92f);
const FLinearColor kIdleEntryColor(0.14f, 0.14f, 0.18f, 1.0f);
const FLinearColor kInPartyColor(0.16f, 0.34f, 0.52f, 1.0f);
const FLinearColor kActiveColor(0.86f, 0.62f, 0.16f, 1.0f);

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

		UButton* Button =
			WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SelectButton"));
		SelectButton = Button;
		Sizer->AddChild(Button);

		LabelText = MakeLabel(*WidgetTree, FText::GetEmpty());
		LabelText->SetJustification(ETextJustify::Center);
		Button->AddChild(LabelText);
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
	}

	// 해금 목록에서는 "파티에 들어 있음", 파티 목록에서는 "꺼내 놓음" 이다.
	const bool bMarked =
		EntryData->SlotIndex == INDEX_NONE ? EntryData->bInParty : EntryData->bActive;

	if (SelectedMarker)
	{
		SelectedMarker->SetVisibility(
			bMarked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	else if (SelectButton)
	{
		// 표식 위젯이 없으면 버튼 색으로 대신한다. 기본 배치가 이 경로다.
		const FLinearColor Color = bMarked
			? (EntryData->SlotIndex == INDEX_NONE ? kInPartyColor : kActiveColor)
			: kIdleEntryColor;
		SelectButton->SetBackgroundColor(Color);
	}
}

void UUEFieldPartyEntryWidget::HandleClicked()
{
	if (!EntryData || !EntryData->Owner)
	{
		return;
	}

	if (EntryData->SlotIndex == INDEX_NONE)
	{
		EntryData->Owner->ToggleMember(EntryData->DexNumber);
	}
	else
	{
		EntryData->Owner->SetActiveMember(EntryData->DexNumber);
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
			PanelSlot->SetSize(FVector2D(900.0f, 640.0f));
		}

		UVerticalBox* Column =
			WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->AddChild(Column);

		Column->AddChild(MakeLabel(*WidgetTree, NSLOCTEXT("HHV", "PartyTitle", "파티")));
		Column->AddChild(MakeLabel(*WidgetTree,
			NSLOCTEXT("HHV", "PartyMembers", "데리고 다닐 포켓몬 (최대 3마리, 눌러서 꺼내기)")));

		UWrapBox* Party =
			WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("PartyList"));
		PartyList = Party;
		Column->AddChild(Party);

		Column->AddChild(MakeLabel(*WidgetTree,
			NSLOCTEXT("HHV", "PartyUnlocked", "해금한 포켓몬 (눌러서 넣고 빼기)")));

		// 해금이 늘어나면 화면 밖으로 넘친다. 목록만 스크롤한다.
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		if (UVerticalBoxSlot* ScrollSlot = Cast<UVerticalBoxSlot>(Column->AddChild(Scroll)))
		{
			ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		UWrapBox* Unlocked =
			WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("UnlockedList"));
		UnlockedList = Unlocked;
		Scroll->AddChild(Unlocked);

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

	if (PendingParty.Remove(DexNumber) > 0)
	{
		// 뺀 것을 꺼내 놓은 상태로 둘 수 없다. 서버도 같은 이유로 거절한다.
		if (PendingActive == DexNumber)
		{
			PendingActive = PendingParty.IsEmpty() ? 0 : PendingParty[0];
		}
		RebuildLists();
		return;
	}

	if (PendingParty.Num() >= MaxPartySize)
	{
		SetStatus(NSLOCTEXT("HHV", "PartyFull", "파티는 3마리까지입니다"));
		return;
	}

	PendingParty.Add(DexNumber);

	// 첫 마리는 자동으로 꺼낸다. 파티만 채우고 아무도 안 꺼낸 채 확인을 누르면
	// 파트너가 사라져서 실수처럼 보인다.
	if (PendingActive == 0)
	{
		PendingActive = DexNumber;
	}
	RebuildLists();
}

void UUEFieldPartyWidget::SetActiveMember(int32 DexNumber)
{
	if (DexNumber <= 0 || !PendingParty.Contains(DexNumber))
	{
		return;
	}
	PendingActive = PendingActive == DexNumber ? 0 : DexNumber;
	RebuildLists();
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
	RebuildLists();
}

void UUEFieldPartyWidget::RebuildLists()
{
	const UUEFieldServerBridgeComponent* Bridge = FindBridge();
	if (!Bridge)
	{
		return;
	}

	UUEPokemonSpeciesCatalog* Catalog = ResolveCatalog();

	const auto AddEntry = [this, Catalog](UPanelWidget* Panel, int32 DexNumber, int32 SlotIndex)
	{
		UUEFieldPartyEntryData* Entry = NewObject<UUEFieldPartyEntryData>(this);
		Entry->DexNumber = DexNumber;
		Entry->Owner = this;
		Entry->SlotIndex = SlotIndex;
		Entry->bInParty = PendingParty.Contains(DexNumber);
		Entry->bActive = PendingActive == DexNumber;

		if (Catalog)
		{
			Entry->Species = Catalog->FindByDex(DexNumber);
		}
		if (Entry->Species)
		{
			// 에셋 이름에서 접두사만 떼어 쓴다. 표시 이름을 코드에 박지 않는다.
			FString DisplayName = Entry->Species->GetName();
			DisplayName.RemoveFromStart(TEXT("DA_"));
			Entry->Label = FText::FromString(DisplayName);
		}
		else
		{
			// 카탈로그에 없는 도감번호. 해금은 됐는데 에셋이 아직 없는 경우다.
			Entry->Label = FText::AsNumber(DexNumber);
		}

		UUEFieldPartyEntryWidget* EntryWidget =
			CreateWidget<UUEFieldPartyEntryWidget>(this, UUEFieldPartyEntryWidget::StaticClass());
		if (!EntryWidget)
		{
			return;
		}
		EntryWidget->Setup(Entry);

		if (UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(Panel->AddChild(EntryWidget)))
		{
			WrapSlot->SetPadding(FMargin(4.0f));
		}
	};

	const TArray<int32>& Unlocked = Bridge->GetPartyState().Unlocked;
	if (UnlockedList)
	{
		UnlockedList->ClearChildren();
		for (const int32 Dex : Unlocked)
		{
			AddEntry(UnlockedList, Dex, INDEX_NONE);
		}
	}
	if (Unlocked.IsEmpty() && StatusText && StatusText->GetText().IsEmpty())
	{
		SetStatus(NSLOCTEXT("HHV", "PartyNoUnlocked", "해금한 포켓몬이 없습니다"));
	}

	if (PartyList)
	{
		PartyList->ClearChildren();
		for (int32 Index = 0; Index < PendingParty.Num(); ++Index)
		{
			AddEntry(PartyList, PendingParty[Index], Index);
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
