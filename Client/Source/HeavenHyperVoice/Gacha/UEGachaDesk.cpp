#include "UEGachaDesk.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

#include "UEGachaMachine.h"
#include "UEGachaPool.h"
#include "UEGachaStudio.h"

namespace
{
	// 필드 컨트롤러 블루프린트를 손대지 않아도 동작하게 하는 폴백.
	const TCHAR* kDefaultWidgetPath = TEXT("/Game/Gacha/UI/WBP_GachaStudio.WBP_GachaStudio_C");

	// 기계 본체. 부품과 캡슐 서른 개가 블루프린트의 Components 에 있으므로
	// C++ 클래스가 아니라 이쪽을 띄워야 한다.
	const TCHAR* kMachineBlueprintPath =
		TEXT("/Game/Gacha/Blueprints/BP_GachaMachine.BP_GachaMachine_C");

	// 타입별 후보표. 순서가 곧 DisplayOrder 이고 서버의 GachaType 과 같다.
	const TCHAR* kPoolPaths[] = {
		TEXT("/Game/Gacha/Data/DA_Gacha_Fire.DA_Gacha_Fire"),
		TEXT("/Game/Gacha/Data/DA_Gacha_Water.DA_Gacha_Water"),
		TEXT("/Game/Gacha/Data/DA_Gacha_Grass.DA_Gacha_Grass"),
		TEXT("/Game/Gacha/Data/DA_Gacha_Normal.DA_Gacha_Normal"),
		TEXT("/Game/Gacha/Data/DA_Gacha_Electric.DA_Gacha_Electric"),
	};

	// 띄운 기계를 둘 자리. 플레이어 머리 위로 멀찍이 올린다 — 지형과 겹치지
	// 않고, 카메라가 기계에 붙으므로 어디에 있든 보이는 것은 같다.
	constexpr double kSpawnHeight = 20000.0;
	constexpr double kSpawnSpacing = 600.0;
}

UUEGachaDeskComponent::UUEGachaDeskComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

APlayerController* UUEGachaDeskComponent::Controller() const
{
	return Cast<APlayerController>(GetOwner());
}

bool UUEGachaDeskComponent::Open()
{
	if (bOpen)
	{
		return true;
	}

	APlayerController* PC = Controller();
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return false;
	}

	Machines.Reset();
	for (TActorIterator<AUEGachaMachine> It(World); It; ++It)
	{
		if (It->Pool)
		{
			Machines.Add(*It);
		}
	}
	// 레벨에 놓인 기계가 없으면(필드가 그렇다) 우리가 띄운다.
	if (Machines.Num() == 0 && !SpawnMachines())
	{
		return false;
	}

	Machines.Sort([](const AUEGachaMachine& A, const AUEGachaMachine& B)
	{
		return A.Pool->DisplayOrder < B.Pool->DisplayOrder;
	});

	if (!WidgetClass)
	{
		WidgetClass = LoadClass<UUEGachaStudioWidget>(nullptr, kDefaultWidgetPath);
		if (!WidgetClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("gacha: cannot load %s"), kDefaultWidgetPath);
			return false;
		}
	}

	PreviousViewTarget = PC->GetViewTarget();
	bRestoreCursor = PC->bShowMouseCursor;

	bOpen = true;
	CurrentMachine = nullptr;
	// 블렌드 없이 잡는다. 띄운 기계는 멀리 있어서 블렌드를 주면 카메라가
	// 거기까지 날아가는 것이 그대로 보인다.
	SelectMachine(0, 0.f);

	if (!Widget)
	{
		Widget = CreateWidget<UUEGachaStudioWidget>(PC, WidgetClass);
	}
	if (Widget && !Widget->IsInViewport())
	{
		Widget->AddToViewport();
	}

	PC->bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(Mode);

	// 카메라가 기계에 가 있는 동안 캐릭터가 보이지 않는 곳을 돌아다니지 않게 막는다.
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);

	SetComponentTickEnabled(true);
	return true;
}

bool UUEGachaDeskComponent::SpawnMachines()
{
	UWorld* World = GetWorld();
	APlayerController* PC = Controller();
	if (!World || !PC)
	{
		return false;
	}

	// 이미 띄워 둔 것이 있으면 그대로 쓴다. 열 때마다 만들면 계속 쌓인다.
	if (SpawnedMachines.Num() > 0)
	{
		for (AUEGachaMachine* Machine : SpawnedMachines)
		{
			if (IsValid(Machine))
			{
				Machines.Add(Machine);
			}
		}
		if (Machines.Num() > 0)
		{
			return true;
		}
		SpawnedMachines.Reset();
	}

	UClass* MachineClass = LoadClass<AUEGachaMachine>(nullptr, kMachineBlueprintPath);
	if (!MachineClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("gacha: cannot load %s"), kMachineBlueprintPath);
		return false;
	}

	const APawn* Pawn = PC->GetPawn();
	const FVector Base = (Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector) +
		FVector(0, 0, kSpawnHeight);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(kPoolPaths); ++Index)
	{
		UUEGachaPool* Pool = LoadObject<UUEGachaPool>(nullptr, kPoolPaths[Index]);
		if (!Pool)
		{
			UE_LOG(LogTemp, Warning, TEXT("gacha: cannot load %s"), kPoolPaths[Index]);
			continue;
		}

		const FVector Location = Base + FVector(0, Index * kSpawnSpacing, 0);
		AUEGachaMachine* Machine =
			World->SpawnActor<AUEGachaMachine>(MachineClass, Location, FRotator::ZeroRotator,
				Params);
		if (!Machine)
		{
			continue;
		}

		// 블루프린트가 이미 Pool 을 물고 있을 수 있다. 우리가 띄운 것은 타입을
		// 여기서 정한다 — 다섯 대가 전부 같은 표를 쓰면 안 된다.
		Machine->Pool = Pool;
		Machines.Add(Machine);
		SpawnedMachines.Add(Machine);
	}

	if (Machines.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("gacha: no machine could be spawned"));
		return false;
	}
	return true;
}

void UUEGachaDeskComponent::Close()
{
	if (!bOpen)
	{
		return;
	}

	ReleaseHandle();
	bOpen = false;
	SetComponentTickEnabled(false);

	if (Widget)
	{
		Widget->RemoveFromParent();
	}

	APlayerController* PC = Controller();
	if (!PC)
	{
		return;
	}

	// 카메라를 원래 대상으로 돌린다. 폰이 사라졌으면 컨트롤러 자신이 대상이 된다.
	AActor* Restore = PreviousViewTarget ? PreviousViewTarget.Get()
		: static_cast<AActor*>(PC->GetPawn());
	if (Restore)
	{
		PC->SetViewTargetWithBlend(Restore, .4f);
	}
	PreviousViewTarget = nullptr;

	PC->bShowMouseCursor = bRestoreCursor;
	PC->SetInputMode(FInputModeGameOnly());
	PC->SetIgnoreMoveInput(false);
	PC->SetIgnoreLookInput(false);
}

void UUEGachaDeskComponent::Toggle()
{
	if (bOpen)
	{
		Close();
	}
	else
	{
		Open();
	}
}

void UUEGachaDeskComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Close();

	// 우리가 띄운 것만 지운다. 레벨에 놓인 기계는 우리 것이 아니다.
	for (AUEGachaMachine* Machine : SpawnedMachines)
	{
		if (IsValid(Machine))
		{
			Machine->Destroy();
		}
	}
	SpawnedMachines.Reset();

	Super::EndPlay(EndPlayReason);
}

void UUEGachaDeskComponent::SelectMachine(int32 Index, float BlendTime)
{
	// 뽑는 중에는 기계를 바꾸지 않는다. 바꾸면 진행 중인 추첨의 결과가
	// 엉뚱한 기계에서 배출된다.
	if (!Machines.IsValidIndex(Index) ||
		(CurrentMachine && CurrentMachine->State != EUEGachaState::Ready
			&& CurrentMachine->State != EUEGachaState::Result))
	{
		return;
	}

	ReleaseHandle();
	CurrentMachine = Machines[Index];

	if (APlayerController* PC = Controller())
	{
		PC->SetViewTargetWithBlend(CurrentMachine, BlendTime);
	}
	// 블렌드가 끝나야 손잡이를 잡을 수 있다. 블렌드가 없으면 기다릴 것도 없다.
	if (const UWorld* World = GetWorld())
	{
		CameraReadyTime = World->GetTimeSeconds() + BlendTime;
	}
}

bool UUEGachaDeskComponent::GetCrankAngle(float& Angle) const
{
	const APlayerController* PC = Controller();
	if (!PC || !CurrentMachine)
	{
		return false;
	}

	FVector2D Center;
	FVector2D Mouse;
	if (!PC->ProjectWorldLocationToScreen(CurrentMachine->HandlePivot->GetComponentLocation(),
			Center) ||
		!PC->GetMousePosition(Mouse.X, Mouse.Y))
	{
		return false;
	}

	const FVector2D Delta = Mouse - Center;
	// 방향을 판단할 수 없는 중심점과 기계에서 너무 멀어진 실수 드래그는 무시한다.
	int32 Width = 0;
	int32 Height = 0;
	PC->GetViewportSize(Width, Height);
	const float Scale = FMath::Max(.25f, Height / 1080.f);
	if (Delta.SizeSquared() < FMath::Square(12 * Scale) ||
		Delta.SizeSquared() > FMath::Square(240 * Scale))
	{
		return false;
	}

	Angle = FMath::Atan2(Delta.Y, Delta.X);
	return true;
}

void UUEGachaDeskComponent::GrabHandle()
{
	APlayerController* PC = Controller();
	const UWorld* World = GetWorld();
	if (!bOpen || !PC || !World || !CurrentMachine || !CurrentMachine->CanTurn() ||
		World->GetTimeSeconds() < CameraReadyTime)
	{
		return;
	}

	FHitResult Hit;
	if (!PC->GetHitResultUnderCursor(ECC_Visibility, true, Hit) ||
		Hit.GetActor() != CurrentMachine || !Hit.GetComponent() ||
		!Hit.GetComponent()->ComponentHasTag(TEXT("GachaHandle")))
	{
		return;
	}

	bDragging = GetCrankAngle(PreviousAngle);
}

void UUEGachaDeskComponent::ReleaseHandle()
{
	bDragging = false;
}

void UUEGachaDeskComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDragging)
	{
		return;
	}

	const APlayerController* PC = Controller();
	if (!PC || !CurrentMachine || !CurrentMachine->CanTurn() ||
		!PC->IsInputKeyDown(EKeys::LeftMouseButton))
	{
		ReleaseHandle();
		return;
	}

	float Angle = 0;
	if (!GetCrankAngle(Angle))
	{
		ReleaseHandle();
		return;
	}

	// 화면 Y는 아래로 증가하므로 각도 증가가 시계 방향이다. ±PI 경계도 연속 처리한다.
	const float Difference = Angle - PreviousAngle;
	const float Delta =
		FMath::RadiansToDegrees(FMath::Atan2(FMath::Sin(Difference), FMath::Cos(Difference)));
	PreviousAngle = Angle;
	CurrentMachine->TurnHandle(Delta);
}
