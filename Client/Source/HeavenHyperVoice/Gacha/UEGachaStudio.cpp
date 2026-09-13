#include "UEGachaStudio.h"

#include "UEGachaDesk.h"
#include "UEGachaMachine.h"
#include "../Pokemon/UEPokemonSpeciesData.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"

AUEGachaStudioGameMode::AUEGachaStudioGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AUEGachaStudioController::StaticClass();
}

AUEGachaStudioController::AUEGachaStudioController()
{
	bShowMouseCursor = true;
	Desk = CreateDefaultSubobject<UUEGachaDeskComponent>(TEXT("GachaDesk"));
}

void AUEGachaStudioController::BeginPlay()
{
	Super::BeginPlay();
	// 뽑기방은 들어온 순간부터 열려 있고 닫지 않는다.
	if (Desk)
	{
		Desk->WidgetClass = StudioWidgetClass;
		Desk->Open();
	}
}

void AUEGachaStudioController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!Desk) return;
	// BindKey 는 UserClass* 를 템플릿 추론한다. TObjectPtr 로는 못 맞춘다.
	UUEGachaDeskComponent* Component = Desk.Get();
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, Component, &UUEGachaDeskComponent::GrabHandle);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, Component, &UUEGachaDeskComponent::ReleaseHandle);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, Component, &UUEGachaDeskComponent::ReleaseHandle);
}

void UUEGachaStudioWidget::NativeConstruct()
{
	Super::NativeConstruct();
	FireButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Fire);
	WaterButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Water);
	GrassButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Grass);
	NormalButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Normal);
	ElectricButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Electric);
	AgainButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Again);
}

FReply UUEGachaStudioWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return FReply::Handled();
}

// 컨트롤러 타입이 아니라 컴포넌트를 찾는다. 같은 화면을 뽑기방 컨트롤러와
// 필드 컨트롤러가 함께 쓰기 때문이다.
UUEGachaDeskComponent* UUEGachaStudioWidget::Desk() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? PC->FindComponentByClass<UUEGachaDeskComponent>() : nullptr;
}

void UUEGachaStudioWidget::Select(int32 Index)
{
	if (UUEGachaDeskComponent* Component = Desk()) Component->SelectMachine(Index);
}
void UUEGachaStudioWidget::Fire() { Select(0); }
void UUEGachaStudioWidget::Water() { Select(1); }
void UUEGachaStudioWidget::Grass() { Select(2); }
void UUEGachaStudioWidget::Normal() { Select(3); }
void UUEGachaStudioWidget::Electric() { Select(4); }
void UUEGachaStudioWidget::Again()
{
	if (UUEGachaDeskComponent* Component = Desk())
	{
		if (AUEGachaMachine* Machine = Component->GetMachine()) Machine->ResetDraw();
	}
}

void UUEGachaStudioWidget::NativeTick(const FGeometry& Geometry, float Seconds)
{
	Super::NativeTick(Geometry, Seconds);
	const UUEGachaDeskComponent* Component = Desk();
	AUEGachaMachine* Machine = Component ? Component->GetMachine() : nullptr;
	if (!Machine || !Machine->Pool) return;
	if (Machine != LastMachine)
	{
		LastMachine = Machine;
		MachineTitle->SetText(Machine->Pool->MachineName);
		FString List;
		for (int32 I = 0; I < Machine->Pool->Entries.Num(); ++I)
		{
			const auto& Entry = Machine->Pool->Entries[I];
			List += FString::Printf(TEXT("%s  ·  %.1f%%\n%s\n\n"), *Entry.DisplayName.ToString(), Machine->Pool->GetEntryProbability(I) * 100,
				*Machine->Pool->GetRarityLabel(Entry.Rarity).ToString());
		}
		PoolText->SetText(FText::FromString(List));
	}
	const bool bBusy = Machine->State == EUEGachaState::Turning
		|| Machine->State == EUEGachaState::Waiting
		|| Machine->State == EUEGachaState::Dispensing;
	for (UButton* Button : {FireButton.Get(), WaterButton.Get(), GrassButton.Get(), NormalButton.Get(), ElectricButton.Get()}) Button->SetIsEnabled(!bBusy);
	StatusText->SetText(Machine->StatusMessage);
	TurnsText->SetText(FText::FromString(FString::Printf(TEXT("%d / 3"), Machine->CompletedTurns)));
	TurnProgress->SetPercent(Machine->TurnDegrees / 1080.f);
	TurnProgress->SetFillColorAndOpacity(Machine->RevealColor);
	const FLinearColor Off(.06f, .08f, .12f);
	StageOne->SetBrushColor(Machine->CompletedTurns >= 1 ? FLinearColor(.025f, .3f, 1.f) : Off);
	StageTwo->SetBrushColor(Machine->CompletedTurns >= 2 ? (Machine->GetResult().Rarity == EUEGachaRarity::Normal ? FLinearColor(.025f, .3f, 1.f) : FLinearColor(.55f, .04f, 1.f)) : Off);
	StageThree->SetBrushColor(Machine->CompletedTurns >= 3 ? Machine->RevealColor : Off);
	const bool bResult = Machine->State == EUEGachaState::Result;
	ResultPanel->SetVisibility(bResult ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	AgainButton->SetVisibility(bResult ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bResult)
	{
		const auto& Entry = Machine->GetResult();
		ResultName->SetText(Entry.DisplayName);
		ResultRarity->SetText(Machine->Pool->GetRarityLabel(Entry.Rarity));
		ResultRarity->SetColorAndOpacity(Machine->RevealColor);
		UTexture2D* Icon = Entry.Species ? Entry.Species->ProfileIcon : nullptr;
		ResultIcon->SetBrushFromTexture(Icon);
		ResultIcon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
