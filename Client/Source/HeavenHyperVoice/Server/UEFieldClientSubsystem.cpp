#include "UEFieldClientSubsystem.h"

#include "../Character/UEPlayerCharacter.h"
#include "../Player/UEPlayerController.h"
#include "../System/UEGameInstance.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	UUEFieldClientSubsystem* GetFieldClientSubsystemFromController(const APlayerController* PlayerController)
	{
		if (!PlayerController)
		{
			return nullptr;
		}

		ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
		return LocalPlayer ? LocalPlayer->GetSubsystem<UUEFieldClientSubsystem>() : nullptr;
	}
}

UUEFieldClientSubsystem* UUEFieldClientSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	if (const APlayerController* PlayerController = Cast<APlayerController>(WorldContextObject))
	{
		return GetFieldClientSubsystemFromController(PlayerController);
	}

	if (const APawn* Pawn = Cast<APawn>(WorldContextObject))
	{
		return GetFieldClientSubsystemFromController(Cast<APlayerController>(Pawn->GetController()));
	}

	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		return GetFieldClientSubsystemFromController(World->GetFirstPlayerController());
	}

	return nullptr;
}

void UUEFieldClientSubsystem::Deinitialize()
{
	if (FieldServerBridgeComponent)
	{
		FieldServerBridgeComponent->OnPokemonPartyChanged.RemoveDynamic(
			this,
			&ThisClass::HandleBridgePokemonPartyChanged);
		FieldServerBridgeComponent->DetachFromPlayer();
		FieldServerBridgeComponent->DestroyComponent();
		FieldServerBridgeComponent = nullptr;
	}

	CachedPlayerController.Reset();
	Super::Deinitialize();
}

void UUEFieldClientSubsystem::RegisterPlayerController(AUEPlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	CachedPlayerController = PlayerController;
	EnsureFieldServerBridge();

	if (AUEPlayerCharacter* PlayerCharacter = Cast<AUEPlayerCharacter>(PlayerController->GetPawn()))
	{
		AttachPlayerCharacter(PlayerCharacter);
	}
}

void UUEFieldClientSubsystem::AttachPlayerCharacter(AUEPlayerCharacter* PlayerCharacter)
{
	if (!PlayerCharacter || PlayerCharacter->IsRemoteProxy())
	{
		return;
	}

	if (UUEFieldServerBridgeComponent* Bridge = EnsureFieldServerBridge())
	{
		// 붙이기 **전에** 목적지를 알려준다. AttachToPlayer 가 그 자리에서
		// 접속을 시작하므로, 뒤에 주면 필드로 한 번 붙었다가 갈아타게 된다.
		Bridge->SetConnectionTarget(PendingInstanceType);
		Bridge->AttachToPlayer(PlayerCharacter);
	}
}

void UUEFieldClientSubsystem::EnterInstance(int32 InstanceType)
{
	if (InstanceType <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FieldClient: instance type must be positive."));
		return;
	}
	if (PendingInstanceType != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FieldClient: already in instance %d."), PendingInstanceType);
		return;
	}
	if (InstanceLevel.IsNull())
	{
		UE_LOG(LogTemp, Error,
			TEXT("FieldClient: InstanceLevel is not set (DefaultGame.ini 의 UEFieldClientSubsystem)."));
		return;
	}

	// 필드 서버는 접속이 끊길 때 "마지막으로 받은 좌표" 를 저장한다. 포탈 위에
	// 선 채로 떠나면 그 자리가 저장되고, 다음 접속에 거기서 살아나면서 포탈
	// 겹침이 다시 터져 인스턴스로 끌려 들어간다.
	//
	// 포탈이 이미 캐릭터를 밖으로 밀어 놓았으므로, 그 좌표를 한 번 더 보내
	// 서버가 그것을 마지막으로 알게 한다.
	if (FieldServerBridgeComponent)
	{
		FieldServerBridgeComponent->ReportFieldPositionNow();
	}

	PendingInstanceType = InstanceType;
	TravelTo(InstanceLevel);
}

void UUEFieldClientSubsystem::LeaveInstance()
{
	if (PendingInstanceType == 0)
	{
		return;
	}
	if (FieldLevel.IsNull())
	{
		UE_LOG(LogTemp, Error,
			TEXT("FieldClient: FieldLevel is not set; 인스턴스에서 나갈 곳이 없다."));
		return;
	}

	PendingInstanceType = 0;
	TravelTo(FieldLevel);
}

void UUEFieldClientSubsystem::TravelTo(const TSoftObjectPtr<UWorld>& Level)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 게임 모드를 ?game= 으로 넘기지 않는다. 레벨이 World Settings 에 적어 둔
	// 오버라이드를 코드가 덮어버리기 때문이다 — 맵 담당이 거기서 게임모드를
	// 바꿔도 아무 일이 안 일어나고, 원인이 코드에 있어서 찾기도 나쁘다.
	//
	// 대신 각 레벨이 자기 게임모드를 선언해야 한다. 안 하면 프로젝트 기본값
	// (BP_FrontendGameMode) 으로 떨어져 폰이 안 뜬다.
	UE_LOG(LogTemp, Display, TEXT("FieldClient: travelling to %s (instance type %d)"),
		*Level.ToString(), PendingInstanceType);
	// 로딩 화면을 거쳐 연다. 옵션은 비운다 — 위 주석 참고.
	if (UUEGameInstance* GameInstance = Cast<UUEGameInstance>(World->GetGameInstance()))
	{
		GameInstance->OpenLevelWithLoadingScreen(Level, /*bAbsolute=*/true, FString());
	}
}

bool UUEFieldClientSubsystem::SendPokemonToggleRequest()
{
	// 이름은 "꺼내기 요청" 이지만 실제로 하는 일은 파티 화면 열기다. 어느 것을
	// 꺼낼지 고르는 곳이 그 화면이라 키 하나로 합쳤다.
	UUEFieldServerBridgeComponent* Bridge = EnsureFieldServerBridge();
	if (!Bridge)
	{
		return false;
	}
	Bridge->TogglePartyWidget();
	return true;
}

bool UUEFieldClientSubsystem::SendPokemonAttackRequest(int32 AttackSlot)
{
	UUEFieldServerBridgeComponent* Bridge = EnsureFieldServerBridge();
	return Bridge ? Bridge->SendPokemonAttackRequest(AttackSlot) : false;
}

TArray<FUEFieldPokemonPartyEntry> UUEFieldClientSubsystem::GetPokemonPartyEntries() const
{
	return FieldServerBridgeComponent
		? FieldServerBridgeComponent->GetPokemonPartyEntries()
		: TArray<FUEFieldPokemonPartyEntry>();
}

const TArray<FUEFieldPokemonPartyEntry>& UUEFieldClientSubsystem::GetCachedPokemonPartyEntries() const
{
	static const TArray<FUEFieldPokemonPartyEntry> EmptyEntries;
	return FieldServerBridgeComponent
		? FieldServerBridgeComponent->GetCachedPokemonPartyEntries()
		: EmptyEntries;
}

void UUEFieldClientSubsystem::HandleBridgePokemonPartyChanged()
{
	OnPokemonPartyChanged.Broadcast();
}

UUEFieldServerBridgeComponent* UUEFieldClientSubsystem::EnsureFieldServerBridge()
{
	if (FieldServerBridgeComponent)
	{
		return FieldServerBridgeComponent;
	}

	AUEPlayerController* PlayerController = CachedPlayerController.Get();
	if (!PlayerController)
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			PlayerController = Cast<AUEPlayerController>(LocalPlayer->GetPlayerController(GetWorld()));
			CachedPlayerController = PlayerController;
		}
	}

	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return nullptr;
	}

	FieldServerBridgeComponent = NewObject<UUEFieldServerBridgeComponent>(
		PlayerController,
		UUEFieldServerBridgeComponent::StaticClass(),
		TEXT("FieldServerBridgeComponent"));
	if (!FieldServerBridgeComponent)
	{
		return nullptr;
	}

	PlayerController->AddInstanceComponent(FieldServerBridgeComponent);
	FieldServerBridgeComponent->RegisterComponent();
	FieldServerBridgeComponent->OnPokemonPartyChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleBridgePokemonPartyChanged);

	return FieldServerBridgeComponent;
}
