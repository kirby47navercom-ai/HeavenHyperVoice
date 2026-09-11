#include "UEGoldenrodCity.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AUEGoldenrodCity::AUEGoldenrodCity()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("CityRoot")));
	RootComponent->SetMobility(EComponentMobility::Static);
	CityMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CityMesh"));
	CityMesh->SetupAttachment(RootComponent);
	CityMesh->SetMobility(EComponentMobility::Static);
	CityMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CityMesh->SetGenerateOverlapEvents(false);
	CityMesh->SetCanEverAffectNavigation(false);
	BoundaryWalls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BoundaryWalls"));
	BoundaryWalls->SetupAttachment(RootComponent);
	BoundaryWalls->SetMobility(EComponentMobility::Static);
	BoundaryWalls->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundaryWalls->SetGenerateOverlapEvents(false);
	BoundaryWalls->SetCanEverAffectNavigation(false);
	BoundaryWalls->SetCastShadow(false);
	// R03 원본의 지면 높이와 건물 경계. UE 단위(cm), FBX의 Y축 변환 적용.
#include "GoldenrodCollision.inl"
#include "GoldenrodPropCollision.inl"
}

void AUEGoldenrodCity::AddCollisionBox(const TCHAR* Name, FVector Min, FVector Max, bool bGround)
{
	UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>(Name);
	Box->SetupAttachment(RootComponent);
	Box->SetMobility(EComponentMobility::Static);
	Box->SetRelativeLocation((Min + Max) * 0.5);
	Box->SetBoxExtent((Max - Min) * 0.5);
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->SetGenerateOverlapEvents(false);
	Box->SetCanEverAffectNavigation(true);
	Box->SetHiddenInGame(true);
	if (FCString::Strncmp(Name, TEXT("Prop_"), 5) == 0)
	{
		Box->CanCharacterStepUpOn = ECB_No;
	}
	Box->ComponentTags.Add(bGround ? TEXT("ServerGround") : TEXT("ServerWall"));
	if (FCString::Strncmp(Name, TEXT("Shore_"), 6) == 0)
	{
		ShoreBoxes.Add(Box);
	}
}

void AUEGoldenrodCity::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildBoundaryWalls();
}

void AUEGoldenrodCity::RebuildBoundaryWalls()
{
	BoundaryWalls->ClearInstances();
	const UStaticMesh* Mesh = CityMesh->GetStaticMesh();
	if (!Mesh) return;
	// Enclose the entire imported map, including the sea, with a 200 cm margin.
	const FBox Bounds = Mesh->GetBoundingBox().TransformBy(CityMesh->GetRelativeTransform());
	const FVector Min = Bounds.Min - FVector(200, 200, 200);
	const FVector Max = Bounds.Max + FVector(200, 200, 0);
	const double Top = FMath::Max(18000.0, Max.Z + 5000.0);
	const FVector Center((Min.X + Max.X) * .5, (Min.Y + Max.Y) * .5, (Min.Z + Top) * .5);
	const double Height = Top - Min.Z;
	const FVector Locations[] = {
		FVector(Min.X, Center.Y, Center.Z), FVector(Max.X, Center.Y, Center.Z),
		FVector(Center.X, Min.Y, Center.Z), FVector(Center.X, Max.Y, Center.Z)};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FVector Size = Index < 2 ? FVector(60, Max.Y - Min.Y + 60, Height)
			: FVector(Max.X - Min.X + 60, 60, Height);
		BoundaryWalls->AddInstance(FTransform(FQuat::Identity, Locations[Index], Size / 100.0));
	}
}

float AUEGoldenrodCity::GetBoundaryCameraWeight(const FVector& WorldPosition) const
{
	float Distance = TNumericLimits<float>::Max();
	for (const UBoxComponent* Box : ShoreBoxes)
	{
		if (!Box) continue;
		const FVector Local = Box->GetComponentTransform().InverseTransformPosition(WorldPosition);
		const FVector Extent = Box->GetUnscaledBoxExtent();
		const FVector2D Gap(FMath::Max(0., FMath::Abs(Local.X) - Extent.X),
			FMath::Max(0., FMath::Abs(Local.Y) - Extent.Y));
		Distance = FMath::Min(Distance, static_cast<float>(Gap.Size()));
	}
	const float FullDistance = FMath::Min(CameraFullEffectDistance, CameraTransitionDistance - 1.f);
	const float Alpha = 1.f - FMath::Clamp((Distance - FullDistance) /
		FMath::Max(1.f, CameraTransitionDistance - FullDistance), 0.f, 1.f);
	return Alpha * Alpha * (3.f - 2.f * Alpha);
}
