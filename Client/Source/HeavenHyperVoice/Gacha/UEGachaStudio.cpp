#include "UEGachaStudio.h"
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
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
}

void AUEGachaStudioController::BeginPlay()
{
	Super::BeginPlay();
	for (TActorIterator<AUEGachaMachine> It(GetWorld()); It; ++It) if (It->Pool) Machines.Add(*It);
	Machines.Sort([](const AUEGachaMachine& A, const AUEGachaMachine& B) { return A.Pool->DisplayOrder < B.Pool->DisplayOrder; });
	SelectMachine(0);
	if (StudioWidgetClass)
	{
		StudioWidget = CreateWidget<UUEGachaStudioWidget>(this, StudioWidgetClass);
		if (StudioWidget) StudioWidget->AddToViewport();
	}
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
}

void AUEGachaStudioController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ThisClass::GrabHandle);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ThisClass::ReleaseHandle);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::ReleaseHandle);
}

void AUEGachaStudioController::SelectMachine(int32 Index)
{
	if (!Machines.IsValidIndex(Index) || (CurrentMachine && CurrentMachine->State != EUEGachaState::Ready && CurrentMachine->State != EUEGachaState::Result)) return;
	ReleaseHandle();
	CurrentMachine = Machines[Index];
	SetViewTargetWithBlend(CurrentMachine, .6f);
	CameraReadyTime = GetWorld()->GetTimeSeconds() + .65f;
}

bool AUEGachaStudioController::GetCrankAngle(float& Angle) const
{
	if (!CurrentMachine) return false;
	FVector2D Center, Mouse;
	if (!ProjectWorldLocationToScreen(CurrentMachine->HandlePivot->GetComponentLocation(), Center) || !GetMousePosition(Mouse.X, Mouse.Y)) return false;
	const FVector2D Delta = Mouse - Center;
	// 방향을 판단할 수 없는 중심점과 기계에서 너무 멀어진 실수 드래그는 무시한다.
	int32 Width, Height;
	GetViewportSize(Width, Height);
	const float Scale = FMath::Max(.25f, Height / 1080.f);
	if (Delta.SizeSquared() < FMath::Square(12 * Scale) || Delta.SizeSquared() > FMath::Square(240 * Scale)) return false;
	Angle = FMath::Atan2(Delta.Y, Delta.X);
	return true;
}

void AUEGachaStudioController::GrabHandle()
{
	if (!CurrentMachine || !CurrentMachine->CanTurn() || GetWorld()->GetTimeSeconds() < CameraReadyTime) return;
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, true, Hit) || Hit.GetActor() != CurrentMachine
		|| !Hit.GetComponent() || !Hit.GetComponent()->ComponentHasTag(TEXT("GachaHandle"))) return;
	bDragging = GetCrankAngle(PreviousAngle);
}

void AUEGachaStudioController::ReleaseHandle() { bDragging = false; }

void AUEGachaStudioController::Tick(float Seconds)
{
	Super::Tick(Seconds);
	if (!bDragging) return;
	if (!CurrentMachine || !CurrentMachine->CanTurn() || !IsInputKeyDown(EKeys::LeftMouseButton)) { ReleaseHandle(); return; }
	float Angle;
	if (!GetCrankAngle(Angle)) { ReleaseHandle(); return; }
	// 화면 Y는 아래로 증가하므로 각도 증가가 시계 방향이다. ±PI 경계도 연속 처리한다.
	const float Difference = Angle - PreviousAngle;
	const float Delta = FMath::RadiansToDegrees(FMath::Atan2(FMath::Sin(Difference), FMath::Cos(Difference)));
	PreviousAngle = Angle;
	CurrentMachine->TurnHandle(Delta);
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

void UUEGachaStudioWidget::Select(int32 Index)
{
	if (auto* PC = GetOwningPlayer<AUEGachaStudioController>()) PC->SelectMachine(Index);
}
void UUEGachaStudioWidget::Fire() { Select(0); }
void UUEGachaStudioWidget::Water() { Select(1); }
void UUEGachaStudioWidget::Grass() { Select(2); }
void UUEGachaStudioWidget::Normal() { Select(3); }
void UUEGachaStudioWidget::Electric() { Select(4); }
void UUEGachaStudioWidget::Again()
{
	if (auto* PC = GetOwningPlayer<AUEGachaStudioController>()) if (PC->GetMachine()) PC->GetMachine()->ResetDraw();
}

void UUEGachaStudioWidget::NativeTick(const FGeometry& Geometry, float Seconds)
{
	Super::NativeTick(Geometry, Seconds);
	const auto* PC = GetOwningPlayer<AUEGachaStudioController>();
	AUEGachaMachine* Machine = PC ? PC->GetMachine() : nullptr;
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
	const bool bBusy = Machine->State == EUEGachaState::Turning || Machine->State == EUEGachaState::Dispensing;
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
