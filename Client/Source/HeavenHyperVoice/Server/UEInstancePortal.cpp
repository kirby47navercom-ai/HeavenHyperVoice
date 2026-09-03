#include "UEInstancePortal.h"

#include "UEFieldClientSubsystem.h"
#include "../Character/UEPlayerCharacter.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AUEInstancePortal::AUEInstancePortal()
{
	PrimaryActorTick.bCanEverTick = false;

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

void AUEInstancePortal::PushOutOfTrigger(AActor* PlayerActor) const
{
	if (!PlayerActor)
	{
		return;
	}

	const FVector PortalLocation = GetActorLocation();
	const FVector PlayerLocation = PlayerActor->GetActorLocation();

	// 들어온 쪽 반대가 아니라 "포탈에서 멀어지는 쪽" 이다. 겹침이 시작된
	// 순간이라 캐릭터는 이미 트리거 가장자리에 있고, 그 방향이 곧 왔던 길이다.
	FVector Outward = PlayerLocation - PortalLocation;
	Outward.Z = 0.0f;
	if (!Outward.Normalize())
	{
		// 정확히 중심에 겹쳐 선 경우. 그때는 포탈이 바라보는 쪽으로 내보낸다.
		Outward = GetActorForwardVector().GetSafeNormal2D();
		if (Outward.IsNearlyZero())
		{
			return;
		}
	}

	// 높이는 건드리지 않는다. 지금 서 있는 지면 높이가 맞다.
	FVector Exit = PortalLocation + Outward * (TriggerRadius + ExitMargin);
	Exit.Z = PlayerLocation.Z;

	// 스윕 없이 옮긴다. 이 액터는 곧 레벨과 함께 사라지고, 여기서 벽에 걸려
	// 트리거 안에 남으면 고치려던 문제가 그대로 남는다.
	PlayerActor->SetActorLocation(Exit, /*bSweep=*/false);
}

void AUEInstancePortal::HandleBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor, UPrimitiveComponent* /*OtherComponent*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (bTravelStarted)
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
	if (!FieldClientSubsystem)
	{
		return;
	}

	bTravelStarted = true;
	if (InstanceType > 0)
	{
		// 포탈 밖으로 밀어낸 뒤에 들어간다. 필드 서버가 저장하는 "마지막 좌표"
		// 가 포탈 위면, 다음 접속에 그 자리에서 살아나면서 겹침이 다시 터져
		// 곧장 인스턴스로 끌려 들어간다.
		PushOutOfTrigger(OtherActor);

		UE_LOG(LogTemp, Display, TEXT("InstancePortal: entering instance %d"), InstanceType);
		FieldClientSubsystem->EnterInstance(InstanceType);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("InstancePortal: leaving instance"));
		FieldClientSubsystem->LeaveInstance();
	}
}
