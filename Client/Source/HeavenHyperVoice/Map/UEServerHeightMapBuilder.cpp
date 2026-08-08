// Fill out your copyright notice in the Description page of Project Settings.

#include "UEServerHeightMapBuilder.h"

namespace
{
	struct FHeightSurface
	{
		FVector Center = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		FVector AxisU = FVector::ForwardVector;
		FVector AxisV = FVector::RightVector;
		double HalfU = 0.0;
		double HalfV = 0.0;
		FBox Bounds = FBox(ForceInit);
	};

	FBox CalculateSurfaceBounds(const FHeightSurface& Surface)
	{
		FBox Result(ForceInit);
		const double Signs[] = { -1.0, 1.0 };
		for (const double SignU : Signs)
		{
			for (const double SignV : Signs)
			{
				Result += Surface.Center + Surface.AxisU * Surface.HalfU * SignU + Surface.AxisV * Surface.HalfV * SignV;
			}
		}

		return Result.ExpandBy(1.0);
	}

	void AddSurface(
		TArray<FHeightSurface>& Surfaces,
		const FUEServerMapObb& Obb,
		const FVector& Normal,
		const FVector& AxisU,
		const FVector& AxisV,
		double HalfU,
		double HalfV,
		double NormalOffset
	)
	{
		FHeightSurface Surface;
		Surface.Normal = Normal.GetSafeNormal();
		Surface.AxisU = AxisU.GetSafeNormal();
		Surface.AxisV = AxisV.GetSafeNormal();
		Surface.HalfU = HalfU;
		Surface.HalfV = HalfV;
		Surface.Center = Obb.Center + Surface.Normal * NormalOffset;
		Surface.Bounds = CalculateSurfaceBounds(Surface);

		if (Surface.Normal.Z > UE_SMALL_NUMBER)
		{
			Surfaces.Add(Surface);
		}
	}

	TArray<FHeightSurface> BuildGroundSurfaces(const TArray<FUEServerMapObb>& GroundObbs)
	{
		TArray<FHeightSurface> Surfaces;
		Surfaces.Reserve(GroundObbs.Num());

		for (const FUEServerMapObb& Obb : GroundObbs)
		{
			AddSurface(Surfaces, Obb, Obb.AxisX, Obb.AxisY, Obb.AxisZ, Obb.HalfExtent.Y, Obb.HalfExtent.Z, Obb.HalfExtent.X);
			AddSurface(Surfaces, Obb, -Obb.AxisX, Obb.AxisY, Obb.AxisZ, Obb.HalfExtent.Y, Obb.HalfExtent.Z, Obb.HalfExtent.X);
			AddSurface(Surfaces, Obb, Obb.AxisY, Obb.AxisX, Obb.AxisZ, Obb.HalfExtent.X, Obb.HalfExtent.Z, Obb.HalfExtent.Y);
			AddSurface(Surfaces, Obb, -Obb.AxisY, Obb.AxisX, Obb.AxisZ, Obb.HalfExtent.X, Obb.HalfExtent.Z, Obb.HalfExtent.Y);
			AddSurface(Surfaces, Obb, Obb.AxisZ, Obb.AxisX, Obb.AxisY, Obb.HalfExtent.X, Obb.HalfExtent.Y, Obb.HalfExtent.Z);
			AddSurface(Surfaces, Obb, -Obb.AxisZ, Obb.AxisX, Obb.AxisY, Obb.HalfExtent.X, Obb.HalfExtent.Y, Obb.HalfExtent.Z);
		}

		return Surfaces;
	}

	bool TryGetSurfaceHeightAt(
		const FHeightSurface& Surface,
		double WorldX,
		double WorldY,
		double Expand,
		double& OutHeight
	)
	{
		if (Surface.Normal.Z <= UE_SMALL_NUMBER)
		{
			return false;
		}

		OutHeight =
			Surface.Center.Z -
			(Surface.Normal.X * (WorldX - Surface.Center.X) +
				Surface.Normal.Y * (WorldY - Surface.Center.Y)) /
				Surface.Normal.Z;

		const FVector SurfacePoint(WorldX, WorldY, OutHeight);
		const FVector Delta = SurfacePoint - Surface.Center;
		const double LocalU = FVector::DotProduct(Delta, Surface.AxisU);
		const double LocalV = FVector::DotProduct(Delta, Surface.AxisV);

		return
			FMath::Abs(LocalU) <= Surface.HalfU + Expand &&
			FMath::Abs(LocalV) <= Surface.HalfV + Expand;
	}
}

void FUEServerHeightMapBuilder::BuildFromGroundObbs(
	const TArray<FUEServerMapObb>& GroundObbs,
	float CellSize,
	FUEServerHeightMapData& OutHeightMap
)
{
	OutHeightMap = FUEServerHeightMapData{};
	OutHeightMap.CellSize = FMath::Max(static_cast<double>(CellSize), 1.0);

	const TArray<FHeightSurface> GroundSurfaces = BuildGroundSurfaces(GroundObbs);
	FBox HeightBounds(ForceInit);
	for (const FHeightSurface& Surface : GroundSurfaces)
	{
		HeightBounds += Surface.Bounds;
	}

	if (!HeightBounds.IsValid || GroundSurfaces.IsEmpty())
	{
		return;
	}

	const double OriginX = FMath::FloorToDouble(HeightBounds.Min.X / OutHeightMap.CellSize) * OutHeightMap.CellSize;
	const double OriginY = FMath::FloorToDouble(HeightBounds.Min.Y / OutHeightMap.CellSize) * OutHeightMap.CellSize;
	const double MaxX = FMath::CeilToDouble(HeightBounds.Max.X / OutHeightMap.CellSize) * OutHeightMap.CellSize;
	const double MaxY = FMath::CeilToDouble(HeightBounds.Max.Y / OutHeightMap.CellSize) * OutHeightMap.CellSize;

	OutHeightMap.Origin = FVector2D(OriginX, OriginY);
	OutHeightMap.Width = FMath::Max(1, FMath::CeilToInt((MaxX - OriginX) / OutHeightMap.CellSize));
	OutHeightMap.Height = FMath::Max(1, FMath::CeilToInt((MaxY - OriginY) / OutHeightMap.CellSize));

	const double SurfaceExpand = OutHeightMap.CellSize * 0.5;
	for (int32 CellY = 0; CellY < OutHeightMap.Height; ++CellY)
	{
		for (int32 CellX = 0; CellX < OutHeightMap.Width; ++CellX)
		{
			const double WorldX = OriginX + (static_cast<double>(CellX) + 0.5) * OutHeightMap.CellSize;
			const double WorldY = OriginY + (static_cast<double>(CellY) + 0.5) * OutHeightMap.CellSize;

			double BestHeight = -TNumericLimits<double>::Max();
			FVector BestNormal = FVector::UpVector;
			bool bHasGround = false;

			for (const FHeightSurface& Surface : GroundSurfaces)
			{
				double SurfaceHeight = 0.0;
				if (!TryGetSurfaceHeightAt(Surface, WorldX, WorldY, SurfaceExpand, SurfaceHeight))
				{
					continue;
				}

				if (!bHasGround || SurfaceHeight > BestHeight)
				{
					BestHeight = SurfaceHeight;
					BestNormal = Surface.Normal;
					bHasGround = true;
				}
			}

			if (bHasGround)
			{
				FUEServerHeightCell Cell;
				Cell.CellX = CellX;
				Cell.CellY = CellY;
				Cell.Height = BestHeight;
				Cell.Normal = BestNormal.GetSafeNormal();
				OutHeightMap.Cells.Add(Cell);
			}
		}
	}
}
