#include "UEVegetationScatterActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"


namespace VegetationScatter
{
	const FName GeneratedTag(TEXT("GeneratedVegetation"));
	constexpr float HexRowHeight = 0.8660254038f;


	void BuildGridWithSpacing(
		const float Radius,
		const float Spacing,
		TArray<FVector2D>& OutPoints
	)
	{
		OutPoints.Reset();

		const float SafeRadius = FMath::Max(Radius, 0.0f);
		const float SafeSpacing = FMath::Max(Spacing, 1.0f);
		const float RowSpacing = SafeSpacing * HexRowHeight;
		const int32 RowCount = FMath::CeilToInt(SafeRadius / RowSpacing);
		const int32 ColumnCount = FMath::CeilToInt(SafeRadius / SafeSpacing) + 1;
		const float RadiusSquared = FMath::Square(SafeRadius);

		for (int32 Row = -RowCount; Row <= RowCount; ++Row)
		{
			const float Y = Row * RowSpacing;
			const float XOffset = (FMath::Abs(Row) % 2) * SafeSpacing * 0.5f;

			for (int32 Column = -ColumnCount; Column <= ColumnCount; ++Column)
			{
				const FVector2D Point(Column * SafeSpacing + XOffset, Y);
				if (Point.SizeSquared() <= RadiusSquared)
				{
					OutPoints.Add(Point);
				}
			}
		}
	}


	void BuildGridPoints(
		const float Radius,
		const int32 PointCount,
		TArray<FVector2D>& OutPoints
	)
	{
		OutPoints.Reset();
		if (Radius <= 0.0f || PointCount <= 0)
		{
			return;
		}

		float Spacing = Radius * FMath::Sqrt(
			(2.0f * PI) / (FMath::Sqrt(3.0f) * PointCount)
		);

		TArray<FVector2D> FullGrid;
		for (int32 Attempt = 0; Attempt < 4; ++Attempt)
		{
			BuildGridWithSpacing(Radius, Spacing, FullGrid);
			if (FullGrid.Num() >= PointCount)
			{
				break;
			}

			Spacing *= FullGrid.IsEmpty()
				? 0.5f
				: FMath::Sqrt(static_cast<float>(FullGrid.Num()) / PointCount) * 0.99f;
		}

		if (FullGrid.Num() <= PointCount)
		{
			OutPoints = MoveTemp(FullGrid);
			return;
		}

		OutPoints.Reserve(PointCount);
		for (int32 Index = 0; Index < PointCount; ++Index)
		{
			const int32 GridIndex = static_cast<int32>(
				(static_cast<int64>(Index) * FullGrid.Num()) / PointCount
			);
			OutPoints.Add(FullGrid[GridIndex]);
		}
	}
}


AUEVegetationScatterActor::AUEVegetationScatterActor()
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
	RadiusPreview->ShapeColor = FColor(80, 220, 120);
}


void AUEVegetationScatterActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RadiusPreview->SetSphereRadius(FMath::Max(Radius, 1.0f));

	if (bAutoRebuild && GetWorld() && !GetWorld()->IsGameWorld())
	{
		RebuildVegetation();
	}
}


void AUEVegetationScatterActor::ClearVegetation()
{
	TArray<UHierarchicalInstancedStaticMeshComponent*> ExistingComponents;
	GetComponents(ExistingComponents);

	for (UHierarchicalInstancedStaticMeshComponent* Component : ExistingComponents)
	{
		if (Component && Component->ComponentHasTag(VegetationScatter::GeneratedTag))
		{
			Component->ClearInstances();
			Component->DestroyComponent();
		}
	}

	GeneratedComponents.Reset();
	PlacedInstanceCount = 0;
}


void AUEVegetationScatterActor::RebuildVegetation()
{
	ClearVegetation();

	UWorld* World = GetWorld();
	if (!World || InstanceCount <= 0 || Radius <= 0.0f)
	{
		return;
	}

	struct FActivePlant
	{
		UHierarchicalInstancedStaticMeshComponent* Component = nullptr;
	};

	TArray<FActivePlant> ActivePlants;

	for (UStaticMesh* Mesh : StaticMeshes)
	{
		if (!Mesh)
		{
			continue;
		}

		const FName ComponentName = MakeUniqueObjectName(
			this,
			UHierarchicalInstancedStaticMeshComponent::StaticClass(),
			*FString::Printf(TEXT("Vegetation_%s"), *Mesh->GetName())
		);

		UHierarchicalInstancedStaticMeshComponent* Component =
			NewObject<UHierarchicalInstancedStaticMeshComponent>(this, ComponentName, RF_Transactional);

		Component->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Component->ComponentTags.Add(VegetationScatter::GeneratedTag);
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
		ActivePlants.Add({Component});
	}

	if (ActivePlants.IsEmpty())
	{
		return;
	}

	FRandomStream Random(RandomSeed);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VegetationScatter), true, this);
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
	TArray<FVector2D> GridPoints;
	VegetationScatter::BuildGridPoints(Radius, InstanceCount, GridPoints);

	for (const FVector2D& Offset : GridPoints)
	{
		const FVector SampleCenter(Center.X + Offset.X, Center.Y + Offset.Y, Center.Z);
		const FVector TraceStart = SampleCenter + FVector::UpVector * TraceHeightAbove;
		const FVector TraceEnd = SampleCenter - FVector::UpVector * TraceDepthBelow;

		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, GroundTraceChannel, QueryParams))
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

		const FActivePlant& SelectedPlant =
			ActivePlants[Random.RandRange(0, ActivePlants.Num() - 1)];

		const float SafeMinScale = FMath::Max(
			FMath::Min(MinScale, MaxScale),
			0.001f
		);
		const float SafeMaxScale = FMath::Max(MinScale, MaxScale);
		const float UniformScale =
			Random.FRandRange(SafeMinScale, FMath::Max(SafeMaxScale, SafeMinScale));
		const float RandomYawRadians = FMath::DegreesToRadians(Random.FRandRange(0.0f, 360.0f));

		FQuat Rotation = FQuat(FVector::UpVector, RandomYawRadians);
		if (bAlignToSurface)
		{
			Rotation = FQuat(SurfaceNormal, RandomYawRadians)
				* FQuat::FindBetweenNormals(FVector::UpVector, SurfaceNormal);
		}

		const FVector Location =
			Hit.ImpactPoint + SurfaceNormal * SurfaceOffset;

		SelectedPlant.Component->AddInstance(
			FTransform(Rotation, Location, FVector(UniformScale)),
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
	FUEVegetationScatterGridTest,
	"HeavenHyperVoice.Vegetation.GridStaysInsideDisk",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter
)


bool FUEVegetationScatterGridTest::RunTest(const FString& Parameters)
{
	constexpr float TestRadius = 1000.0f;
	constexpr int32 TestPointCount = 1000;
	TArray<FVector2D> Points;
	VegetationScatter::BuildGridPoints(TestRadius, TestPointCount, Points);

	TestEqual(TEXT("Grid contains the requested number of positions"), Points.Num(), TestPointCount);

	for (const FVector2D& Point : Points)
	{
		if (!TestTrue(TEXT("Grid point is inside the configured radius"), Point.SizeSquared() <= FMath::Square(TestRadius)))
		{
			return false;
		}
	}

	return true;
}

#endif
