#include "UEInstancePortal.h"

#include "UEFieldClientSubsystem.h"
#include "../Movement/UECoreMovementComponent.h"
#include "UEFieldServerBridgeComponent.h"
#include "../Character/UEPlayerCharacter.h"
#include "../Player/UEPlayerController.h"
#include "../System/UEGameInstance.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AUEInstancePortal::AUEInstancePortal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetSphereRadius(TriggerRadius);
	// Trigger 프로파일이 곧 "질의만 하고 막지는 않는다" 다. 직접 채널을 만지면
	// 프로젝트 설정이 바뀔 때 조용히 어긋난다.
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetGenerateOverlapEvents(true);
	SetRootComponent(Trigger);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 엔진 기본 도형이라 프로젝트 에셋이 없어도 보인다. 미술 에셋이 나오면
	// 레벨에서 이 메시만 갈아 끼우면 된다.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
		// 기본 원기둥은 100uu 정육면체에 맞춰져 있다. 사람 키만 한 기둥으로 세운다.
		Mesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 2.0f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, BasicMaterial.Object);
	}
}

void AUEInstancePortal::BeginPlay()
{
	Super::BeginPlay();

	// 반지름은 에디터에서 고칠 수 있으므로 생성자 값이 아니라 지금 값을 쓴다.
	Trigger->SetSphereRadius(TriggerRadius);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
}

void AUEInstancePortal::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    const AUEPlayerCharacter* Player = Controller ? Cast<AUEPlayerCharacter>(Controller->GetPawn()) : nullptr;
    if (!Player || !Player->GetCoreMovement()->IsNetworkSimulationActive())
    {
        return;
    }

    // A restored server position may be inside this portal. Require a real exit
    // before entering again, without sending an invented teleport to the server.
    if (!Trigger->IsOverlappingActor(Player))
    {
        bArmed = true;
        SetActorTickEnabled(false);
    }
}

void AUEInstancePortal::HandleBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor, UPrimitiveComponent* /*OtherComponent*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!bArmed || bTravelStarted)
	{
		return;
	}

	// 다른 플레이어의 대역 액터도 이 트리거를 밟는다. 그것까지 받아주면 남이
	// 지나갈 때 내 화면이 인스턴스로 넘어간다.
	const AUEPlayerCharacter* PlayerCharacter = Cast<AUEPlayerCharacter>(OtherActor);
	if (!PlayerCharacter || PlayerCharacter->IsRemoteProxy() ||
		!PlayerCharacter->IsLocallyControlled())
	{
		return;
	}

	UUEFieldClientSubsystem* FieldClientSubsystem = UUEFieldClientSubsystem::Get(this);
	if (const AUEPlayerController* Controller = Cast<AUEPlayerController>(PlayerCharacter->GetController());
		Controller && Controller->IsPhotoModeActive()) return;
	if (!FieldClientSubsystem)
	{
		return;
	}

	if (InstanceType > 0)
	{
		// 파티가 비어 있으면 들어가 봐야 싸울 포켓몬이 없다. 서버도 같은 것을
		// 검사하지만(InstanceHandler), 여기서 막아야 레벨을 옮겼다가 거절당하고
		// 돌아오는 헛걸음을 안 한다.
		// 브릿지는 UUEFieldClientSubsystem 이 **컨트롤러**에 붙인다.
		AController* OwningController = PlayerCharacter->GetController();
		const UUEFieldServerBridgeComponent* Bridge =
			OwningController
				? OwningController->FindComponentByClass<UUEFieldServerBridgeComponent>()
				: nullptr;
		if (Bridge != nullptr)
		{
			if (Bridge->GetPartyState().Party.IsEmpty())
			{
				// 겹침이 계속 들어오므로 안내는 한 번만 띄운다.
				if (!bEmptyPartyNoticeShown)
				{
					bEmptyPartyNoticeShown = true;
					if (AUEPlayerController* Controller =
							Cast<AUEPlayerController>(PlayerCharacter->GetController()))
					{
						Controller->AddSystemMessage(
							TEXT("파티에 포켓몬을 한 마리 이상 넣어야 들어갈 수 있습니다"));
					}
				}
				return;
			}
		}

		// 파티에 속해 있으면 파티장만 입장을 시작할 수 있다. 서버도 같은 것을
		// 검사하지만(InstanceHandler 의 허가권), 여기서 막아야 파티원이 혼자
		// 레벨을 옮겼다가 거절당하고 돌아오는 헛걸음을 안 한다.
		UUEGameInstance* GameInstance = Cast<UUEGameInstance>(GetGameInstance());
		const bool bInParty = GameInstance != nullptr && GameInstance->IsInPlayerParty();

		if (bInParty && !GameInstance->IsPlayerPartyLeader())
		{
			// 겹침이 계속 들어오므로 안내는 한 번만 띄운다. 자리는 그대로 둔다 —
			// 파티장이 열어 주면 여기 서 있어도 같이 넘어간다.
			if (!bLeaderNoticeShown)
			{
				bLeaderNoticeShown = true;
				if (AUEPlayerController* Controller =
						Cast<AUEPlayerController>(PlayerCharacter->GetController()))
				{
					Controller->AddSystemMessage(TEXT("파티장만 입장을 시작할 수 있습니다"));
				}
			}
			return;
		}

		bTravelStarted = true;


		if (bInParty)
		{
			// 파티장은 곧장 가지 않는다. 채팅 서버가 전원에게 입장을 열어 주고,
			// 그 신호를 받아 다 같이 넘어간다 (UEPlayerController 의 콜백).
			UE_LOG(LogTemp, Display, TEXT("InstancePortal: opening instance %d for the party"),
				InstanceType);
			if (AUEPlayerController* Controller =
					Cast<AUEPlayerController>(PlayerCharacter->GetController()))
			{
				Controller->RequestPartyEnterInstance(InstanceType);
			}
			return;
		}

		UE_LOG(LogTemp, Display, TEXT("InstancePortal: entering instance %d"), InstanceType);
		FieldClientSubsystem->EnterInstance(InstanceType);
	}
	else
	{
		// 나가는 것은 개인 자유다. 파티장 권한은 입장에만 걸린다.
		bTravelStarted = true;
		UE_LOG(LogTemp, Display, TEXT("InstancePortal: leaving instance"));
		FieldClientSubsystem->LeaveInstance();
	}
}
