#include "UERandomMeshScatterActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"


namespace RandomMeshScatter
{
	const FName GeneratedTag(TEXT("GeneratedRandomMesh"));


	FVector2D RandomPointInDisk(FRandomStream& Random, const float Radius)
	{
		const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
		const float Distance = FMath::Sqrt(Random.FRand()) * Radius;
		return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
	}
}


AUERandomMeshScatterActor::AUERandomMeshScatterActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	RadiusPreview = CreateDefaultSubobject<USphereComponent>(TEXT("RadiusPreview"));
	RadiusPreview->SetupAttachment(SceneRoot);
	RadiusPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RadiusPreview->SetGenerateOverlapEvents(false);
	RadiusPreview->SetCanEverAffectNavigation(false);
	RadiusPreview->SetHiddenInGame(true);
	RadiusPreview->SetUsingAbsoluteScale(true);
	RadiusPreview->ShapeColor = FColor(230, 160, 60);
}


void AUERandomMeshScatterActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RadiusPreview->SetSphereRadius(FMath::Max(Radius, 1.0f));
	if (bAutoRebuild && GetWorld() && !GetWorld()->IsGameWorld())
	{
		RebuildScatter();
	}
}


void AUERandomMeshScatterActor::ClearScatter()
{
	TArray<UHierarchicalInstancedStaticMeshComponent*> ExistingComponents;
	GetComponents(ExistingComponents);

	for (UHierarchicalInstancedStaticMeshComponent* Component : ExistingComponents)
	{
		if (Component && Component->ComponentHasTag(RandomMeshScatter::GeneratedTag))
		{
			Component->ClearInstances();
			Component->DestroyComponent();
		}
	}

	GeneratedComponents.Reset();
	PlacedInstanceCount = 0;
}


void AUERandomMeshScatterActor::RebuildScatter()
{
	ClearScatter();

	UWorld* World = GetWorld();
	if (!World || InstanceCount <= 0 || Radius <= 0.0f)
	{
		return;
	}

	TArray<UHierarchicalInstancedStaticMeshComponent*> ActiveComponents;
	for (UStaticMesh* Mesh : StaticMeshes)
	{
		if (!Mesh)
		{
			continue;
		}

		const FName ComponentName = MakeUniqueObjectName(
			this,
			UHierarchicalInstancedStaticMeshComponent::StaticClass(),
			*FString::Printf(TEXT("RandomMesh_%s"), *Mesh->GetName())
		);

		UHierarchicalInstancedStaticMeshComponent* Component =
			NewObject<UHierarchicalInstancedStaticMeshComponent>(this, ComponentName, RF_Transactional);
		Component->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Component->ComponentTags.Add(RandomMeshScatter::GeneratedTag);
		Component->SetupAttachment(SceneRoot);
		Component->SetMobility(EComponentMobility::Static);
		Component->SetAbsolute(false, false, true);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCullDistances(
			FMath::Max(StartCullDistance, 0),
			FMath::Max(EndCullDistance, StartCullDistance)
		);

		AddInstanceComponent(Component);
		Component->RegisterComponent();
		GeneratedComponents.Add(Component);
		ActiveComponents.Add(Component);
	}

	if (ActiveComponents.IsEmpty())
	{
		return;
	}

	FRandomStream Random(RandomSeed);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RandomMeshScatter), true, this);
	if (bLandscapeOnly)
	{
		for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
		{
			if (!Actor->IsA<ALandscapeProxy>())
			{
				QueryParams.AddIgnoredActor(*Actor);
			}
		}
	}

	const FVector Center = GetActorLocation();
	const float MinimumUpDot = FMath::Cos(FMath::DegreesToRadians(MaxGroundSlopeDegrees));
	const float SafeMinScale = FMath::Max(FMath::Min(MinScale, MaxScale), 0.001f);
	const float SafeMaxScale = FMath::Max(FMath::Max(MinScale, MaxScale), SafeMinScale);
	const int64 MaximumAttempts =
		static_cast<int64>(InstanceCount) * FMath::Max(MaxAttemptsPerInstance, 1);

	for (int64 Attempt = 0; Attempt < MaximumAttempts && PlacedInstanceCount < InstanceCount; ++Attempt)
	{
		const FVector2D Offset = RandomMeshScatter::RandomPointInDisk(Random, Radius);
		const FVector SampleCenter(Center.X + Offset.X, Center.Y + Offset.Y, Center.Z);

		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(
			Hit,
			SampleCenter + FVector::UpVector * TraceHeightAbove,
			SampleCenter - FVector::UpVector * TraceDepthBelow,
			GroundTraceChannel,
			QueryParams
		))
		{
			continue;
		}

		if (bLandscapeOnly && !Cast<ALandscapeProxy>(Hit.GetActor()))
		{
			continue;
		}

		const FVector SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
		if (FVector::DotProduct(SurfaceNormal, FVector::UpVector) < MinimumUpDot)
		{
			continue;
		}

		const float RandomYaw = FMath::DegreesToRadians(Random.FRandRange(0.0f, 360.0f));
		FQuat Rotation(FVector::UpVector, RandomYaw);
		if (bAlignToSurface)
		{
			Rotation = FQuat(SurfaceNormal, RandomYaw)
				* FQuat::FindBetweenNormals(FVector::UpVector, SurfaceNormal);
		}

		const float Scale = Random.FRandRange(SafeMinScale, SafeMaxScale);
		UHierarchicalInstancedStaticMeshComponent* Component =
			ActiveComponents[Random.RandRange(0, ActiveComponents.Num() - 1)];
		Component->AddInstance(
			FTransform(
				Rotation,
				Hit.ImpactPoint + SurfaceNormal * SurfaceOffset,
				FVector(Scale)
			),
			true
		);
		++PlacedInstanceCount;
	}

	for (UHierarchicalInstancedStaticMeshComponent* Component : GeneratedComponents)
	{
		if (Component)
		{
			Component->BuildTreeIfOutdated(true, false);
		}
	}
}


#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUERandomMeshScatterDiskTest,
	"HeavenHyperVoice.RandomMeshScatter.PointsStayInsideDisk",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter
)


bool FUERandomMeshScatterDiskTest::RunTest(const FString& Parameters)
{
	FRandomStream Random(1337);
	FRandomStream SameSeed(1337);
	constexpr float TestRadius = 1000.0f;
	TestEqual(
		TEXT("The same seed produces the same first position"),
		RandomMeshScatter::RandomPointInDisk(Random, TestRadius),
		RandomMeshScatter::RandomPointInDisk(SameSeed, TestRadius)
	);

	for (int32 Index = 0; Index < 10000; ++Index)
	{
		const FVector2D Point = RandomMeshScatter::RandomPointInDisk(Random, TestRadius);
		if (!TestTrue(TEXT("Random point is inside the configured radius"), Point.SizeSquared() <= FMath::Square(TestRadius)))
		{
			return false;
		}
	}

	return true;
}

#endif
