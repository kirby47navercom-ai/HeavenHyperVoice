#include "UEDirtPathDecalActor.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#if WITH_EDITOR
#include "Containers/Ticker.h"
#endif
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Math/RotationMatrix.h"
#include "Materials/MaterialInterface.h"
#include "UERandomMeshScatterActor.h"
#include "UEVegetationScatterActor.h"


namespace DirtPathDecal
{
	const FName GeneratedTag(TEXT("GeneratedDirtPathDecal"));
}


AUEDirtPathDecalActor::AUEDirtPathDecalActor()
{
	PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITOR
	bRunConstructionScriptOnDrag = false;
#endif

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
}


void AUEDirtPathDecalActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bAutoRebuild && GetWorld() && !GetWorld()->IsGameWorld())
	{
#if WITH_EDITOR
		QueueDeferredEditorRebuild();
#else
		RebuildPath();
#endif
	}
}


#if WITH_EDITOR
void AUEDirtPathDecalActor::QueueDeferredEditorRebuild()
{
	if (PendingRebuildHandle.IsValid())
	{
		FTSTicker::RemoveTicker(PendingRebuildHandle);
		PendingRebuildHandle.Reset();
	}

	PendingRebuildHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float)
		{
			PendingRebuildHandle.Reset();
			if (bAutoRebuild && GetWorld() && !GetWorld()->IsGameWorld())
			{
				RebuildPath();
			}
			return false;
		}),
		0.35f
	);
}
#endif


void AUEDirtPathDecalActor::ClearPath()
{
	TArray<UDecalComponent*> ExistingComponents;
	GetComponents(ExistingComponents);

	for (UDecalComponent* Component : ExistingComponents)
	{
		if (Component && Component->ComponentHasTag(DirtPathDecal::GeneratedTag))
		{
			Component->DestroyComponent();
		}
	}

	GeneratedDecals.Reset();
	GeneratedDecalCount = 0;
}


void AUEDirtPathDecalActor::RebuildPath()
{
#if WITH_EDITOR
	if (PendingRebuildHandle.IsValid())
	{
		FTSTicker::RemoveTicker(PendingRebuildHandle);
		PendingRebuildHandle.Reset();
	}
#endif

	ClearPath();

	UWorld* World = GetWorld();
	if (!World || !DecalMaterial)
	{
		return;
	}

	const FTransform ActorTransform = GetActorTransform();
	const float AreaLength = ActorTransform.TransformVector(
		FVector(PathEnd.X, 0.0f, 0.0f)
	).Size2D();
	const float AreaWidth = ActorTransform.TransformVector(
		FVector(0.0f, PathEnd.Y, 0.0f)
	).Size2D();
	if (AreaLength <= UE_SMALL_NUMBER && AreaWidth <= UE_SMALL_NUMBER)
	{
		return;
	}

	const FVector PathDirection = FVector::VectorPlaneProject(
		ActorTransform.TransformVectorNoScale(FVector::ForwardVector),
		FVector::UpVector
	).GetSafeNormal();
	if (PathDirection.IsNearlyZero())
	{
		return;
	}

	const float SafeDecalScale = FMath::Max(DecalPieceScale, 0.01f);
	const float SafeColumnSpacing = FMath::Max(PieceSpacing, 1.0f);
	const float SafeRowSpacing = FMath::Max(RowSpacing, 1.0f);
	const float SafeWidth = FMath::Max(PathWidth * SafeDecalScale, 1.0f);
	const float SafeLength = FMath::Max(PieceLength * SafeDecalScale, 1.0f);
	int32 ColumnCount = AreaLength > UE_SMALL_NUMBER
		? FMath::CeilToInt(AreaLength / SafeColumnSpacing) + 1
		: 1;
	int32 RowCount = AreaWidth > UE_SMALL_NUMBER
		? FMath::CeilToInt(AreaWidth / SafeRowSpacing) + 1
		: 1;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DirtPathDecal), true, this);
	for (AActor* ExcludedArea : ExcludedGrassAreas)
	{
		if (ExcludedArea)
		{
			QueryParams.AddIgnoredActor(ExcludedArea);
		}
	}

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

	const int32 SafeSubdivisionDepth = FMath::Clamp(ExclusionSubdivisionDepth, 0, 6);
	TFunction<void(const FVector&, float, float, int32)> AddDecalPiece;
	AddDecalPiece = [&, this](
		const FVector& Center,
		const float Width,
		const float Length,
		const int32 SubdivisionDepth
	)
	{
		const EExcludedAreaRelation AreaRelation = ClassifyExcludedAreaOverlap(
			Center,
			PathDirection,
			Width,
			Length
		);
		if (AreaRelation == EExcludedAreaRelation::Inside)
		{
			return;
		}

		if (AreaRelation == EExcludedAreaRelation::Partial)
		{
			if (SubdivisionDepth >= SafeSubdivisionDepth)
			{
				return;
			}

			const FVector SideDirection = FVector::CrossProduct(
				FVector::UpVector,
				PathDirection
			).GetSafeNormal();
			const float ChildWidth = Width * 0.5f;
			const float ChildLength = Length * 0.5f;
			const FVector ForwardOffset = PathDirection * ChildLength * 0.5f;
			const FVector SideOffset = SideDirection * ChildWidth * 0.5f;

			AddDecalPiece(
				Center + ForwardOffset + SideOffset,
				ChildWidth,
				ChildLength,
				SubdivisionDepth + 1
			);
			AddDecalPiece(
				Center + ForwardOffset - SideOffset,
				ChildWidth,
				ChildLength,
				SubdivisionDepth + 1
			);
			AddDecalPiece(
				Center - ForwardOffset + SideOffset,
				ChildWidth,
				ChildLength,
				SubdivisionDepth + 1
			);
			AddDecalPiece(
				Center - ForwardOffset - SideOffset,
				ChildWidth,
				ChildLength,
				SubdivisionDepth + 1
			);
			return;
		}

		FHitResult Hit;
		const FVector TraceStart = Center + FVector::UpVector * TraceHeightAbove;
		const FVector TraceEnd = Center - FVector::UpVector * TraceDepthBelow;
		if (!World->LineTraceSingleByChannel(
			Hit,
			TraceStart,
			TraceEnd,
			GroundTraceChannel,
			QueryParams
		))
		{
			return;
		}

		if (bLandscapeOnly && !Cast<ALandscapeProxy>(Hit.GetActor()))
		{
			return;
		}

		const FVector SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
		const FVector SurfacePathDirection = FVector::VectorPlaneProject(
			PathDirection,
			SurfaceNormal
		).GetSafeNormal();
		if (SurfaceNormal.IsNearlyZero() || SurfacePathDirection.IsNearlyZero())
		{
			return;
		}

		const FQuat Rotation = FRotationMatrix::MakeFromXZ(
			-SurfaceNormal,
			SurfacePathDirection
		).ToQuat();
		const FName ComponentName = MakeUniqueObjectName(
			this,
			UDecalComponent::StaticClass(),
			TEXT("DirtPathPiece")
		);
		UDecalComponent* Component = NewObject<UDecalComponent>(
			this,
			ComponentName,
			RF_Transactional
		);
		Component->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Component->ComponentTags.Add(DirtPathDecal::GeneratedTag);
		Component->SetupAttachment(SceneRoot);
		Component->SetMobility(EComponentMobility::Static);
		Component->SetAbsolute(false, false, true);
		Component->DecalSize = FVector(
			FMath::Max(ProjectionDepth * SafeDecalScale, 1.0f),
			Width * 0.5f,
			Length * 0.5f
		);
		Component->SetDecalMaterial(DecalMaterial);
		Component->SetFadeScreenSize(FadeScreenSize);
		Component->SetSortOrder(SortOrder);

		AddInstanceComponent(Component);
		Component->RegisterComponent();
		Component->SetWorldLocationAndRotation(
			Hit.ImpactPoint + SurfaceNormal * SurfaceOffset,
			Rotation
		);

		GeneratedDecals.Add(Component);
		++GeneratedDecalCount;
	};

	for (int32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
	{
		const float RowAlpha = RowCount > 1
			? static_cast<float>(RowIndex) / (RowCount - 1)
			: 0.0f;

		for (int32 ColumnIndex = 0; ColumnIndex < ColumnCount; ++ColumnIndex)
		{
			const float ColumnAlpha = ColumnCount > 1
				? static_cast<float>(ColumnIndex) / (ColumnCount - 1)
				: 0.0f;
			const FVector LocalSample(
				PathEnd.X * ColumnAlpha,
				PathEnd.Y * RowAlpha,
				PathEnd.Z * FMath::Max(ColumnAlpha, RowAlpha)
			);
			const FVector SampleCenter = ActorTransform.TransformPosition(LocalSample);
			AddDecalPiece(SampleCenter, SafeWidth, SafeLength, 0);
		}
	}
}


AUEDirtPathDecalActor::EExcludedAreaRelation
AUEDirtPathDecalActor::ClassifyExcludedAreaOverlap(
	const FVector& Center,
	const FVector& PathDirection,
	const float Width,
	const float Length
) const
{
	const FVector SideDirection = FVector::CrossProduct(
		FVector::UpVector,
		PathDirection
	).GetSafeNormal();
	const FVector2D Center2D(Center.X, Center.Y);
	const FVector2D Forward2D(PathDirection.X, PathDirection.Y);
	const FVector2D Side2D(SideDirection.X, SideDirection.Y);
	const float HalfLength = Length * 0.5f;
	const float HalfWidth = Width * 0.5f;
	const FVector2D FootprintCorners[] =
	{
		Center2D + Forward2D * HalfLength + Side2D * HalfWidth,
		Center2D + Forward2D * HalfLength - Side2D * HalfWidth,
		Center2D - Forward2D * HalfLength + Side2D * HalfWidth,
		Center2D - Forward2D * HalfLength - Side2D * HalfWidth
	};
	bool bHasPartialOverlap = false;

	for (const AActor* ExcludedArea : ExcludedGrassAreas)
	{
		if (!ExcludedArea)
		{
			continue;
		}

		if (const AUEVegetationScatterActor* VegetationArea =
			Cast<AUEVegetationScatterActor>(ExcludedArea))
		{
			const FVector2D AreaCenter(
				VegetationArea->GetActorLocation().X,
				VegetationArea->GetActorLocation().Y
			);
			const float Radius = FMath::Max(VegetationArea->Radius, 0.0f);
			const float RadiusSquared = FMath::Square(Radius);
			bool bAllCornersInside = true;
			for (const FVector2D& Corner : FootprintCorners)
			{
				if (FVector2D::DistSquared(Corner, AreaCenter) > RadiusSquared)
				{
					bAllCornersInside = false;
					break;
				}
			}
			if (bAllCornersInside)
			{
				return EExcludedAreaRelation::Inside;
			}

			const FVector2D AreaDelta = AreaCenter - Center2D;
			const float ClosestForward = FMath::Clamp(
				FVector2D::DotProduct(AreaDelta, Forward2D),
				-HalfLength,
				HalfLength
			);
			const float ClosestSide = FMath::Clamp(
				FVector2D::DotProduct(AreaDelta, Side2D),
				-HalfWidth,
				HalfWidth
			);
			const FVector2D ClosestPoint =
				Center2D + Forward2D * ClosestForward + Side2D * ClosestSide;
			bHasPartialOverlap |=
				FVector2D::DistSquared(ClosestPoint, AreaCenter) <= RadiusSquared;
			continue;
		}

		if (const AUERandomMeshScatterActor* RandomMeshArea =
			Cast<AUERandomMeshScatterActor>(ExcludedArea))
		{
			const FVector2D AreaCenter(
				RandomMeshArea->GetActorLocation().X,
				RandomMeshArea->GetActorLocation().Y
			);
			const float Radius = FMath::Max(RandomMeshArea->Radius, 0.0f);
			const float RadiusSquared = FMath::Square(Radius);
			bool bAllCornersInside = true;
			for (const FVector2D& Corner : FootprintCorners)
			{
				if (FVector2D::DistSquared(Corner, AreaCenter) > RadiusSquared)
				{
					bAllCornersInside = false;
					break;
				}
			}
			if (bAllCornersInside)
			{
				return EExcludedAreaRelation::Inside;
			}

			const FVector2D AreaDelta = AreaCenter - Center2D;
			const float ClosestForward = FMath::Clamp(
				FVector2D::DotProduct(AreaDelta, Forward2D),
				-HalfLength,
				HalfLength
			);
			const float ClosestSide = FMath::Clamp(
				FVector2D::DotProduct(AreaDelta, Side2D),
				-HalfWidth,
				HalfWidth
			);
			const FVector2D ClosestPoint =
				Center2D + Forward2D * ClosestForward + Side2D * ClosestSide;
			bHasPartialOverlap |=
				FVector2D::DistSquared(ClosestPoint, AreaCenter) <= RadiusSquared;
			continue;
		}

		FVector BoundsOrigin;
		FVector BoundsExtent;
		ExcludedArea->GetActorBounds(false, BoundsOrigin, BoundsExtent, true);
		bool bAllCornersInside = true;
		for (const FVector2D& Corner : FootprintCorners)
		{
			if (Corner.X < BoundsOrigin.X - BoundsExtent.X
				|| Corner.X > BoundsOrigin.X + BoundsExtent.X
				|| Corner.Y < BoundsOrigin.Y - BoundsExtent.Y
				|| Corner.Y > BoundsOrigin.Y + BoundsExtent.Y)
			{
				bAllCornersInside = false;
				break;
			}
		}
		if (bAllCornersInside)
		{
			return EExcludedAreaRelation::Inside;
		}

		const FVector2D CenterDelta(
			BoundsOrigin.X - Center.X,
			BoundsOrigin.Y - Center.Y
		);
		const FVector2D SeparationAxes[] =
		{
			Forward2D,
			Side2D,
			FVector2D::UnitX(),
			FVector2D::UnitY()
		};
		bool bSeparated = false;
		for (const FVector2D& Axis : SeparationAxes)
		{
			const float FootprintProjection =
				HalfLength * FMath::Abs(FVector2D::DotProduct(Forward2D, Axis))
				+ HalfWidth * FMath::Abs(FVector2D::DotProduct(Side2D, Axis));
			const float AreaProjection =
				BoundsExtent.X * FMath::Abs(Axis.X)
				+ BoundsExtent.Y * FMath::Abs(Axis.Y);
			if (FMath::Abs(FVector2D::DotProduct(CenterDelta, Axis))
				> FootprintProjection + AreaProjection)
			{
				bSeparated = true;
				break;
			}
		}
		bHasPartialOverlap |= !bSeparated;
	}

	return bHasPartialOverlap
		? EExcludedAreaRelation::Partial
		: EExcludedAreaRelation::Outside;
}
