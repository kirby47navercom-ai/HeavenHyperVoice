#include "UECave.h"

#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"

#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

#include "MaterialDomain.h"

namespace CaveSection
{
	constexpr int32 Tunnel = 0;
	constexpr int32 Chamber = 1;
	constexpr int32 Entrance = 2;
}


namespace
{
	struct FCaveSectionBuffers
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector2D> UV0;
		TArray<FVector> Normals;
		TArray<FProcMeshTangent> Tangents;

		bool bValid = true;

		int32 AddVertex(const FVector& Position, const FVector2D& UV)
		{
			const int32 Index = Vertices.Add(Position);
			UV0.Add(UV);
			return Index;
		}

		void AddTriangle(const int32 A, const int32 B, const int32 C)
		{
			if (
				A < 0 || A >= Vertices.Num() ||
				B < 0 || B >= Vertices.Num() ||
				C < 0 || C >= Vertices.Num()
			)
			{
				bValid = false;
				return;
			}

			Triangles.Add(A);
			Triangles.Add(B);
			Triangles.Add(C);
		}

		/**
		 * Make the section physically two-sided.
		 *
		 * We intentionally duplicate the vertices for the back faces.
		 * If reversed triangles shared the same vertices, generated
		 * normals could cancel each other. Separate vertices allow
		 * CalculateTangentsForMesh() to create opposite normals/tangents
		 * for the reverse side.
		 */
		void MakeTwoSided()
		{
			if (!bValid || Vertices.Num() == 0 || Triangles.Num() == 0)
			{
				return;
			}

			if (UV0.Num() != Vertices.Num() || (Triangles.Num() % 3) != 0)
			{
				bValid = false;
				return;
			}

			/*
			 * IMPORTANT:
			 *
			 * Do NOT do this:
			 *
			 *     Vertices.Add(Vertices[Index]);
			 *
			 * TArray::Add() is modifying Vertices while its input reference
			 * points to an element inside the same Vertices array.
			 * UE 5.8 asserts on that self-aliasing access.
			 *
			 * Therefore we first copy the original data into independent
			 * arrays and only append/read from those copies.
			 */
			const TArray<FVector> OriginalVertices = Vertices;
			const TArray<FVector2D> OriginalUV0 = UV0;
			const TArray<int32> OriginalTriangles = Triangles;

			const int32 OriginalVertexCount = OriginalVertices.Num();
			const int32 OriginalTriangleIndexCount = OriginalTriangles.Num();
			const int32 BackFaceVertexOffset = OriginalVertexCount;

			Vertices.Reserve(OriginalVertexCount * 2);
			UV0.Reserve(OriginalVertexCount * 2);
			Triangles.Reserve(OriginalTriangleIndexCount * 2);

			// Safe: source arrays are independent copies.
			Vertices.Append(OriginalVertices);
			UV0.Append(OriginalUV0);

			// Add reversed copies of the ORIGINAL triangles only.
			for (
				int32 TriangleIndex = 0;
				TriangleIndex < OriginalTriangleIndexCount;
				TriangleIndex += 3
			)
			{
				const int32 A = OriginalTriangles[TriangleIndex + 0];
				const int32 B = OriginalTriangles[TriangleIndex + 1];
				const int32 C = OriginalTriangles[TriangleIndex + 2];

				Triangles.Add(A + BackFaceVertexOffset);
				Triangles.Add(C + BackFaceVertexOffset);
				Triangles.Add(B + BackFaceVertexOffset);
			}
		}

		bool Validate() const
		{
			if (!bValid)
			{
				return false;
			}

			if (Vertices.Num() == 0 || Triangles.Num() == 0)
			{
				return false;
			}

			if (UV0.Num() != Vertices.Num())
			{
				return false;
			}

			if ((Triangles.Num() % 3) != 0)
			{
				return false;
			}

			for (const int32 Index : Triangles)
			{
				if (Index < 0 || Index >= Vertices.Num())
				{
					return false;
				}
			}

			return true;
		}

		bool CalculateNormalsAndTangents()
		{
			if (!Validate())
			{
				return false;
			}

			Normals.Reset();
			Tangents.Reset();

			UKismetProceduralMeshLibrary::CalculateTangentsForMesh(
				Vertices,
				Triangles,
				UV0,
				Normals,
				Tangents
			);

			return
				Normals.Num() == Vertices.Num()
				&&
				Tangents.Num() == Vertices.Num();
		}
	};
}


// ============================================================
// Constructor
// ============================================================

AUECave::AUECave()
{
	PrimaryActorTick.bCanEverTick = false;

	CaveMesh = CreateDefaultSubobject<UProceduralMeshComponent>(
		TEXT("CaveMesh")
	);

	SetRootComponent(CaveMesh);

	// Unreal collision is disabled.
	// The user's external server owns cave collision.
	// 콜리전
	
	CaveMesh->SetCollisionEnabled(
		ECollisionEnabled::NoCollision
	);
	// CaveMesh->SetCollisionEnabled(
	// 	ECollisionEnabled::QueryAndPhysics
	// );

	CaveMesh->SetGenerateOverlapEvents(false);
}


// ============================================================
// Construction
// ============================================================

void AUECave::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bAutoRebuild)
	{
		RebuildCave();
	}
	else
	{
		ApplyCaveMaterials();
	}
}


// ============================================================
// Chamber placement
// ============================================================

float AUECave::GetChamberOffset() const
{
	const float SafeTunnelRadius =
		FMath::Max(TunnelRadius, 1.0f);

	const float SafeChamberRadius =
		FMath::Max(
			ChamberRadius,
			SafeTunnelRadius + 1.0f
		);

	return FMath::Sqrt(
		FMath::Max(
			0.0f,
			SafeChamberRadius * SafeChamberRadius
			-
			SafeTunnelRadius * SafeTunnelRadius
		)
	);
}


FVector AUECave::GetChamberCenterLocal() const
{
	return FVector(
		TunnelLength + GetChamberOffset(),
		0.0f,
		0.0f
	);
}


// ============================================================
// Materials
// ============================================================

void AUECave::ApplyCaveMaterials()
{
	if (!CaveMesh)
	{
		return;
	}

	UMaterialInterface* DefaultMaterial =
		UMaterial::GetDefaultMaterial(MD_Surface);

	UMaterialInterface* FinalTunnel =
		TunnelMaterial
			? TunnelMaterial.Get()
			: DefaultMaterial;

	UMaterialInterface* FinalChamber =
		ChamberMaterial
			? ChamberMaterial.Get()
			: FinalTunnel;

	UMaterialInterface* FinalEntrance =
		EntranceMaterial
			? EntranceMaterial.Get()
			: FinalTunnel;

	CaveMesh->SetMaterial(
		CaveSection::Tunnel,
		FinalTunnel
	);

	CaveMesh->SetMaterial(
		CaveSection::Chamber,
		FinalChamber
	);

	CaveMesh->SetMaterial(
		CaveSection::Entrance,
		FinalEntrance
	);
}


// ============================================================
// Rebuild
// ============================================================

void AUECave::RebuildCave()
{
	if (!CaveMesh)
	{
		return;
	}

	const double L =
		FMath::Max(
			static_cast<double>(TunnelLength),
			100.0
		);

	const double TunnelR =
		FMath::Max(
			static_cast<double>(TunnelRadius),
			10.0
		);

	const double DomeR =
		FMath::Max(
			static_cast<double>(ChamberRadius),
			TunnelR + 10.0
		);

	const int32 ArcSegments =
		FMath::Clamp(
			TunnelArcSegments,
			6,
			64
		);

	const int32 DomeSegments =
		FMath::Clamp(
			ChamberLengthSegments,
			4,
			96
		);

	const double UVSize =
		FMath::Max(
			static_cast<double>(UVWorldSize),
			10.0
		);

	const double ChamberOffset =
		FMath::Sqrt(
			FMath::Max(
				0.0,
				DomeR * DomeR
				-
				TunnelR * TunnelR
			)
		);

	const double ChamberCenterX =
		L + ChamberOffset;

	const double PhiOpening =
		FMath::Acos(
			-ChamberOffset / DomeR
		);

	FCaveSectionBuffers Tunnel;
	FCaveSectionBuffers Chamber;
	FCaveSectionBuffers Entrance;

	// ========================================================
	// 1. Tunnel arch shell
	//
	// The winding faces INTO the cave.
	// ========================================================

	TArray<int32> TunnelStart;
	TArray<int32> TunnelEnd;

	TunnelStart.Reserve(ArcSegments + 1);
	TunnelEnd.Reserve(ArcSegments + 1);

	for (int32 i = 0; i <= ArcSegments; ++i)
	{
		const double Alpha =
			static_cast<double>(i)
			/
			static_cast<double>(ArcSegments);

		const double Theta =
			PI * Alpha;

		const double Y =
			TunnelR * FMath::Cos(Theta);

		const double Z =
			TunnelR * FMath::Sin(Theta);

		const double ArcDistance =
			Theta * TunnelR;

		TunnelStart.Add(
			Tunnel.AddVertex(
				FVector(0.0, Y, Z),
				FVector2D(
					0.0,
					ArcDistance / UVSize
				)
			)
		);

		TunnelEnd.Add(
			Tunnel.AddVertex(
				FVector(L, Y, Z),
				FVector2D(
					L / UVSize,
					ArcDistance / UVSize
				)
			)
		);
	}

	for (int32 i = 0; i < ArcSegments; ++i)
	{
		const int32 A = TunnelStart[i];
		const int32 B = TunnelEnd[i];
		const int32 C = TunnelEnd[i + 1];
		const int32 D = TunnelStart[i + 1];

		Tunnel.AddTriangle(A, B, C);
		Tunnel.AddTriangle(A, C, D);
	}

	// ========================================================
	// 2. Tunnel floor
	//
	// Separate vertices are intentional:
	// the floor needs planar UVs.
	// Normals face +Z, which is the cave interior.
	// ========================================================

	const int32 FloorFR =
		Tunnel.AddVertex(
			FVector(0.0, TunnelR, 0.0),
			FVector2D(
				0.0,
				TunnelR / UVSize
			)
		);

	const int32 FloorFL =
		Tunnel.AddVertex(
			FVector(0.0, -TunnelR, 0.0),
			FVector2D(
				0.0,
				-TunnelR / UVSize
			)
		);

	const int32 FloorBR =
		Tunnel.AddVertex(
			FVector(L, TunnelR, 0.0),
			FVector2D(
				L / UVSize,
				TunnelR / UVSize
			)
		);

	const int32 FloorBL =
		Tunnel.AddVertex(
			FVector(L, -TunnelR, 0.0),
			FVector2D(
				L / UVSize,
				-TunnelR / UVSize
			)
		);

	Tunnel.AddTriangle(
		FloorFR,
		FloorBL,
		FloorBR
	);

	Tunnel.AddTriangle(
		FloorFR,
		FloorFL,
		FloorBL
	);

	// ========================================================
	// 3. Entrance cap
	//
	// The front wall faces +X, into the tunnel.
	// ========================================================

	if (bCapEntrance)
	{
		const int32 Center =
			Entrance.AddVertex(
				FVector::ZeroVector,
				FVector2D::ZeroVector
			);

		TArray<int32> Arc;
		Arc.Reserve(ArcSegments + 1);

		for (int32 i = 0; i <= ArcSegments; ++i)
		{
			const double Theta =
				PI
				*
				static_cast<double>(i)
				/
				static_cast<double>(ArcSegments);

			const double Y =
				TunnelR * FMath::Cos(Theta);

			const double Z =
				TunnelR * FMath::Sin(Theta);

			Arc.Add(
				Entrance.AddVertex(
					FVector(0.0, Y, Z),
					FVector2D(
						Y / UVSize,
						Z / UVSize
					)
				)
			);
		}

		for (int32 i = 0; i < ArcSegments; ++i)
		{
			Entrance.AddTriangle(
				Center,
				Arc[i],
				Arc[i + 1]
			);
		}
	}

	// ========================================================
	// 4. Chamber shell
	//
	// The first ring is exactly at X=TunnelLength and has
	// radius TunnelRadius. The room then expands into the
	// large hemisphere and closes at the back pole.
	// ========================================================

	TArray<TArray<int32>> ChamberRings;
	ChamberRings.Reserve(DomeSegments);

	for (int32 RingIndex = 0; RingIndex < DomeSegments; ++RingIndex)
	{
		const double Alpha =
			static_cast<double>(RingIndex)
			/
			static_cast<double>(DomeSegments);

		// Opening -> back pole.
		const double Phi =
			PhiOpening * (1.0 - Alpha);

		const double X =
			ChamberCenterX
			+
			DomeR * FMath::Cos(Phi);

		const double CrossRadius =
			DomeR * FMath::Sin(Phi);

		const double UDistance =
			(PhiOpening - Phi) * DomeR;

		TArray<int32> Ring;
		Ring.Reserve(ArcSegments + 1);

		for (int32 i = 0; i <= ArcSegments; ++i)
		{
			const double Theta =
				PI
				*
				static_cast<double>(i)
				/
				static_cast<double>(ArcSegments);

			const double Y =
				CrossRadius * FMath::Cos(Theta);

			const double Z =
				CrossRadius * FMath::Sin(Theta);

			Ring.Add(
				Chamber.AddVertex(
					FVector(X, Y, Z),
					FVector2D(
						UDistance / UVSize,
						Theta * DomeR / UVSize
					)
				)
			);
		}

		ChamberRings.Add(MoveTemp(Ring));
	}

	for (
		int32 RingIndex = 0;
		RingIndex < ChamberRings.Num() - 1;
		++RingIndex
	)
	{
		const TArray<int32>& Current =
			ChamberRings[RingIndex];

		const TArray<int32>& Next =
			ChamberRings[RingIndex + 1];

		for (int32 i = 0; i < ArcSegments; ++i)
		{
			const int32 A = Current[i];
			const int32 B = Next[i];
			const int32 C = Next[i + 1];
			const int32 D = Current[i + 1];


			Chamber.AddTriangle(A, B, C);
			Chamber.AddTriangle(A, C, D);
		}
	}

	// ========================================================
	// 5. Chamber back pole
	// ========================================================

	const int32 ChamberPole =
		Chamber.AddVertex(
			FVector(
				ChamberCenterX + DomeR,
				0.0,
				0.0
			),
			FVector2D(
				PhiOpening * DomeR / UVSize,
				0.0
			)
		);

	if (ChamberRings.Num() == 0)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[UECave] ChamberRings is unexpectedly empty.")
		);
		return;
	}

	const TArray<int32>& LastRing =
		ChamberRings.Last();

	for (int32 i = 0; i < ArcSegments; ++i)
	{
		Chamber.AddTriangle(
			LastRing[i],
			ChamberPole,
			LastRing[i + 1]
		);
	}

	// ========================================================
	// 6. Chamber floor
	//
	// Boundary:
	// opening-right -> room back -> opening-left.
	// Fan winding faces +Z.
	// ========================================================

	TArray<FVector> FloorBoundaryPositions;
	FloorBoundaryPositions.Reserve(
		ChamberRings.Num() * 2 + 1
	);

	// Right edge: opening -> back.
	for (
		int32 RingIndex = 0;
		RingIndex < ChamberRings.Num();
		++RingIndex
	)
	{
		const int32 VertexIndex =
			ChamberRings[RingIndex][0];

		const FVector P =
			Chamber.Vertices[VertexIndex];

		FloorBoundaryPositions.Add(
			FVector(P.X, P.Y, 0.0)
		);
	}

	// Exact back point.
	FloorBoundaryPositions.Add(
		FVector(
			ChamberCenterX + DomeR,
			0.0,
			0.0
		)
	);

	// Left edge: back -> opening.
	for (
		int32 RingIndex = ChamberRings.Num() - 1;
		RingIndex >= 0;
		--RingIndex
	)
	{
		const int32 VertexIndex =
			ChamberRings[RingIndex][ArcSegments];

		const FVector P =
			Chamber.Vertices[VertexIndex];

		FloorBoundaryPositions.Add(
			FVector(P.X, P.Y, 0.0)
		);
	}

	const int32 ChamberFloorCenter =
		Chamber.AddVertex(
			FVector(
				ChamberCenterX,
				0.0,
				0.0
			),
			FVector2D::ZeroVector
		);

	TArray<int32> FloorBoundary;
	FloorBoundary.Reserve(
		FloorBoundaryPositions.Num()
	);

	for (const FVector& Position : FloorBoundaryPositions)
	{
		FloorBoundary.Add(
			Chamber.AddVertex(
				Position,
				FVector2D(
					(Position.X - ChamberCenterX) / UVSize,
					Position.Y / UVSize
				)
			)
		);
	}

	if (FloorBoundary.Num() < 3)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[UECave] Chamber floor boundary is invalid.")
		);
		return;
	}

	for (int32 i = 0; i < FloorBoundary.Num() - 1; ++i)
	{
		Chamber.AddTriangle(
			ChamberFloorCenter,
			FloorBoundary[i + 1],
			FloorBoundary[i]
		);
	}

	Chamber.AddTriangle(
		ChamberFloorCenter,
		FloorBoundary[0],
		FloorBoundary.Last()
	);

	// ========================================================
	// 7. Two-sided geometry
	//
	// This does NOT depend on the Material's "Two Sided" flag.
	// We create a second copy of every surface with reversed
	// winding and separate vertices.
	// ========================================================

	if (bTwoSidedGeometry)
	{
		Tunnel.MakeTwoSided();
		Chamber.MakeTwoSided();

		if (bCapEntrance)
		{
			Entrance.MakeTwoSided();
		}
	}

	// ========================================================
	// 8. Validate EVERYTHING before calling engine mesh APIs
	// ========================================================

	if (!Tunnel.Validate())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[UECave] Tunnel data validation failed.")
		);
		return;
	}

	if (!Chamber.Validate())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[UECave] Chamber data validation failed.")
		);
		return;
	}

	if (bCapEntrance && !Entrance.Validate())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[UECave] Entrance data validation failed.")
		);
		return;
	}

	// ========================================================
	// 9. Generate render normals + tangents from UV0
	// ========================================================

	if (!Tunnel.CalculateNormalsAndTangents())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[UECave] Tunnel tangent generation failed.")
		);
		return;
	}

	if (!Chamber.CalculateNormalsAndTangents())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[UECave] Chamber tangent generation failed.")
		);
		return;
	}

	if (
		bCapEntrance
		&&
		!Entrance.CalculateNormalsAndTangents()
	)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[UECave] Entrance tangent generation failed.")
		);
		return;
	}

	// ========================================================
	// 10. Replace the three mesh sections
	//
	// Section 0 = Tunnel
	// Section 1 = Chamber
	// Section 2 = Entrance
	//
	// No DynamicMesh attributes or overlays are used here.
	// ========================================================

	CaveMesh->ClearAllMeshSections();

	const TArray<FLinearColor> EmptyVertexColors;
	// 콜리전
	bool test = false;
	
	CaveMesh->CreateMeshSection_LinearColor(
		CaveSection::Tunnel,
		Tunnel.Vertices,
		Tunnel.Triangles,
		Tunnel.Normals,
		Tunnel.UV0,
		EmptyVertexColors,
		Tunnel.Tangents,
		test, // no Unreal collision
		false
	);

	CaveMesh->CreateMeshSection_LinearColor(
		CaveSection::Chamber,
		Chamber.Vertices,
		Chamber.Triangles,
		Chamber.Normals,
		Chamber.UV0,
		EmptyVertexColors,
		Chamber.Tangents,
		test,
		false
	);

	if (bCapEntrance)
	{
		CaveMesh->CreateMeshSection_LinearColor(
			CaveSection::Entrance,
			Entrance.Vertices,
			Entrance.Triangles,
			Entrance.Normals,
			Entrance.UV0,
			EmptyVertexColors,
			Entrance.Tangents,
			test,
			false
		);
	}

	ApplyCaveMaterials();
	
	//// 콜리전
	
	CaveMesh->SetCollisionEnabled(
		ECollisionEnabled::NoCollision
	);
	
	// CaveMesh->SetCollisionEnabled(
	// 	ECollisionEnabled::QueryAndPhysics
	// );

	CaveMesh->SetGenerateOverlapEvents(false);

	LastTunnelVertexCount =
		Tunnel.Vertices.Num();

	LastChamberVertexCount =
		Chamber.Vertices.Num();

	LastEntranceVertexCount =
		bCapEntrance
			? Entrance.Vertices.Num()
			: 0;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[UECave] Build OK V2 | Tunnel=%d | Chamber=%d | Entrance=%d | TwoSidedGeometry=%s"
		),
		LastTunnelVertexCount,
		LastChamberVertexCount,
		LastEntranceVertexCount,
		bTwoSidedGeometry ? TEXT("TRUE") : TEXT("FALSE")
	);
}


// ============================================================
// Getters
// ============================================================

UProceduralMeshComponent*
AUECave::GetCaveMeshComponent() const
{
	return CaveMesh;
}


FUECaveSimpleCollisionData
AUECave::GetSimpleCollisionData() const
{
	FUECaveSimpleCollisionData Data;

	Data.CaveTransform =
		GetActorTransform();

	Data.TunnelLength =
		TunnelLength;

	Data.TunnelRadius =
		TunnelRadius;

	Data.ChamberCenterLocal =
		GetChamberCenterLocal();

	Data.ChamberRadius =
		ChamberRadius;

	return Data;
}


// ============================================================
// External-server-compatible containment test
// ============================================================

bool AUECave::IsPointInsideCaveLocal(
	FVector P
) const
{
	const float SafeTunnelLength =
		FMath::Max(
			TunnelLength,
			1.0f
		);

	const float SafeTunnelRadius =
		FMath::Max(
			TunnelRadius,
			1.0f
		);

	const float SafeChamberRadius =
		FMath::Max(
			ChamberRadius,
			SafeTunnelRadius + 1.0f
		);

	bool bInsideTunnel = false;

	if (
		P.X >= 0.0f
		&&
		P.X <= SafeTunnelLength
		&&
		P.Z >= 0.0f
	)
	{
		bInsideTunnel =
			(
				P.Y * P.Y
				+
				P.Z * P.Z
			)
			<=
			(
				SafeTunnelRadius
				*
				SafeTunnelRadius
			);
	}

	const FVector ChamberCenter =
		GetChamberCenterLocal();

	const FVector Delta =
		P - ChamberCenter;

	bool bInsideChamber = false;

	if (
		P.X >= SafeTunnelLength
		&&
		P.Z >= 0.0f
	)
	{
		bInsideChamber =
			Delta.SizeSquared()
			<=
			(
				SafeChamberRadius
				*
				SafeChamberRadius
			);
	}

	return
		bInsideTunnel
		||
		bInsideChamber;
}