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
		UE_LOG(LogTemp, Display, TEXT("InstancePortal: entering instance %d"), InstanceType);
		FieldClientSubsystem->EnterInstance(InstanceType);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("InstancePortal: leaving instance"));
		FieldClientSubsystem->LeaveInstance();
	}
}
