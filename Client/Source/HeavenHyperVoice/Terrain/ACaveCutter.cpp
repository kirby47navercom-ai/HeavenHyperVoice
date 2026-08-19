#include "ACaveCutter.h"

#include "Components/DynamicMeshComponent.h"

#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTangents.h"
#include "DynamicMesh/MeshAttributeUtil.h"

#include "MaterialDomain.h"


using namespace UE::Geometry;


// ============================================================
// Material IDs
// ============================================================

namespace CaveMaterialID
{
	constexpr int32 Tunnel = 0;
	constexpr int32 Chamber = 1;
	constexpr int32 Entrance = 2;
}


// ============================================================
// Constructor
// ============================================================

AACaveCutter::AACaveCutter(
	const FObjectInitializer& ObjectInitializer
)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;


	if (
		UDynamicMeshComponent* MeshComponent =
			GetDynamicMeshComponent()
	)
	{
		MeshComponent->SetCollisionEnabled(
			ECollisionEnabled::NoCollision
		);


		MeshComponent->SetGenerateOverlapEvents(
			false
		);


		MeshComponent->SetTwoSided(
			true
		);


		// Empty DynamicMesh에는 아직 UV/Normal overlay가 없습니다.
		// 생성자 단계에서는 자동 Tangent 계산을 절대 켜지 않습니다.
		MeshComponent->SetTangentsType(
			EDynamicMeshComponentTangentsMode::NoTangents
		);
	}
}


// ============================================================
// On Construction
// ============================================================

void AACaveCutter::OnConstruction(
	const FTransform& Transform
)
{
	Super::OnConstruction(
		Transform
	);


	// 기존 BP에 AutoCalculated 값이 직렬화되어 있어도
	// Rebuild 전에는 반드시 끕니다.
	if (UDynamicMeshComponent* MeshComponent = GetDynamicMeshComponent())
	{
		MeshComponent->SetTangentsType(
			EDynamicMeshComponentTangentsMode::NoTangents
		);
	}


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
// Chamber Offset
// ============================================================

float AACaveCutter::GetChamberOffset() const
{
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


	return FMath::Sqrt(
		FMath::Max(
			0.0f,

			SafeChamberRadius
			*
			SafeChamberRadius

			-

			SafeTunnelRadius
			*
			SafeTunnelRadius
		)
	);
}


// ============================================================
// Chamber Center
// ============================================================

FVector AACaveCutter::GetChamberCenterLocal() const
{
	return FVector(
		TunnelLength
		+
		GetChamberOffset(),

		0.0f,

		0.0f
	);
}


// ============================================================
// Apply Materials
//
// ★ Component에서 Material을 읽지 않습니다.
// 따라서 슬롯이 0개여도 GetMaterial(0) 같은 접근 자체가 없습니다.
// ============================================================

void AACaveCutter::ApplyCaveMaterials()
{
	UDynamicMeshComponent* MeshComponent =
		GetDynamicMeshComponent();


	if (!MeshComponent)
	{
		return;
	}


	UMaterialInterface* DefaultMaterial =
		UMaterial::GetDefaultMaterial(
			MD_Surface
		);


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


	TArray<UMaterialInterface*> MaterialSet;


	MaterialSet.Reserve(
		3
	);


	MaterialSet.Add(
		FinalTunnel
	);


	MaterialSet.Add(
		FinalChamber
	);


	MaterialSet.Add(
		FinalEntrance
	);


	/*
	 * ID 0 = MaterialSet[0]
	 * ID 1 = MaterialSet[1]
	 * ID 2 = MaterialSet[2]
	 */

	MeshComponent->ConfigureMaterialSet(
		MaterialSet,
		true
	);


	MeshComponent->SetTwoSided(
		bTwoSided
	);
}


// ============================================================
// Rebuild Cave
// ============================================================

void AACaveCutter::RebuildCave()
{
	UDynamicMeshComponent* MeshComponent =
		GetDynamicMeshComponent();


	if (!MeshComponent)
	{
		return;
	}


	// ========================================================
	// Safe Values
	// ========================================================

	const double L =
		FMath::Max(
			static_cast<double>(
				TunnelLength
			),
			100.0
		);


	const double TunnelR =
		FMath::Max(
			static_cast<double>(
				TunnelRadius
			),
			10.0
		);


	const double DomeR =
		FMath::Max(
			static_cast<double>(
				ChamberRadius
			),
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
			static_cast<double>(
				UVWorldSize
			),
			10.0
		);


	// ========================================================
	// Chamber 위치
	// ========================================================

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
		L
		+
		ChamberOffset;


	const double PhiOpening =
		FMath::Acos(
			-ChamberOffset
			/
			DomeR
		);


	// ========================================================
	// Mesh
	//
	// ★ 필요한 Vertex Data를 명시적으로 활성화
	//
	// UE 문서상 Vertex Normal / UV / Triangle Group은
	// optional component이므로 직접 활성화합니다.
	// ========================================================

	FDynamicMesh3 Mesh;


	Mesh.EnableVertexNormals(
		FVector3f(
			0.0f,
			0.0f,
			1.0f
		)
	);


	Mesh.EnableVertexUVs(
		FVector2f(
			0.0f,
			0.0f
		)
	);


	Mesh.EnableTriangleGroups(
		0
	);


	// ========================================================
	// Vertex Helper
	// ========================================================

	auto AddVertex =
		[
			&Mesh
		]
		(
			const FVector3d& Position,
			const FVector2f& UV
		)
		{
			const int32 VertexID =
				Mesh.AppendVertex(
					Position
				);


			if (
				VertexID
				==
				FDynamicMesh3::InvalidID
			)
			{
				return FDynamicMesh3::InvalidID;
			}


			Mesh.SetVertexUV(
				VertexID,
				UV
			);


			return VertexID;
		};


	// ========================================================
	// Triangle Helper
	//
	// Triangle Group을 임시 Material ID로 사용
	// ========================================================

	auto AddTriangle =
		[
			&Mesh
		]
		(
			const int32 A,
			const int32 B,
			const int32 C,
			const int32 MaterialID
		)
		{
			if (
				A < 0
				||
				B < 0
				||
				C < 0
			)
			{
				return;
			}


			const int32 TriangleID =
				Mesh.AppendTriangle(
					A,
					B,
					C,
					MaterialID
				);


			if (TriangleID < 0)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT(
						"[Cave] AppendTriangle failed."
					)
				);
			}
		};


	// ========================================================
	// 1. Tunnel Rings
	// ========================================================

	TArray<int32> TunnelStartRing;

	TArray<int32> TunnelEndRing;


	TunnelStartRing.Reserve(
		ArcSegments + 1
	);


	TunnelEndRing.Reserve(
		ArcSegments + 1
	);


	for (
		int32 i = 0;
		i <= ArcSegments;
		++i
	)
	{
		const double Alpha =
			static_cast<double>(i)
			/
			static_cast<double>(
				ArcSegments
			);


		const double Theta =
			PI
			*
			Alpha;


		const double Y =
			TunnelR
			*
			FMath::Cos(
				Theta
			);


		const double Z =
			TunnelR
			*
			FMath::Sin(
				Theta
			);


		const float V =
			static_cast<float>(
				Theta
				*
				TunnelR
				/
				UVSize
			);


		const int32 StartID =
			AddVertex(
				FVector3d(
					0.0,
					Y,
					Z
				),

				FVector2f(
					0.0f,
					V
				)
			);


		const int32 EndID =
			AddVertex(
				FVector3d(
					L,
					Y,
					Z
				),

				FVector2f(
					static_cast<float>(
						L
						/
						UVSize
					),
					V
				)
			);


		TunnelStartRing.Add(
			StartID
		);


		TunnelEndRing.Add(
			EndID
		);
	}


	// ========================================================
	// ★ 여기서 Ring이 정상인지 확인
	//
	// 이후 [0] 접근 전에 무조건 검사
	// ========================================================

	if (
		TunnelStartRing.Num()
		!=
		ArcSegments + 1
		||
		TunnelEndRing.Num()
		!=
		ArcSegments + 1
	)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[Cave] Tunnel ring generation failed."
			)
		);

		return;
	}


	// ========================================================
	// 2. Tunnel Shell
	// ========================================================

	for (
		int32 i = 0;
		i < ArcSegments;
		++i
	)
	{
		AddTriangle(
			TunnelStartRing[i],
			TunnelEndRing[i + 1],
			TunnelEndRing[i],
			CaveMaterialID::Tunnel
		);


		AddTriangle(
			TunnelStartRing[i],
			TunnelStartRing[i + 1],
			TunnelEndRing[i + 1],
			CaveMaterialID::Tunnel
		);
	}


	// ========================================================
	// 3. Tunnel Floor
	//
	// Floor는 UV가 다르므로 별도 Vertex
	// ========================================================

	const int32 FloorFR =
		AddVertex(
			FVector3d(
				0.0,
				TunnelR,
				0.0
			),
			FVector2f(
				0.0f,
				static_cast<float>(
					TunnelR / UVSize
				)
			)
		);


	const int32 FloorFL =
		AddVertex(
			FVector3d(
				0.0,
				-TunnelR,
				0.0
			),
			FVector2f(
				0.0f,
				static_cast<float>(
					-TunnelR / UVSize
				)
			)
		);


	const int32 FloorBR =
		AddVertex(
			FVector3d(
				L,
				TunnelR,
				0.0
			),
			FVector2f(
				static_cast<float>(
					L / UVSize
				),
				static_cast<float>(
					TunnelR / UVSize
				)
			)
		);


	const int32 FloorBL =
		AddVertex(
			FVector3d(
				L,
				-TunnelR,
				0.0
			),
			FVector2f(
				static_cast<float>(
					L / UVSize
				),
				static_cast<float>(
					-TunnelR / UVSize
				)
			)
		);


	AddTriangle(
		FloorFR,
		FloorBR,
		FloorBL,
		CaveMaterialID::Tunnel
	);


	AddTriangle(
		FloorFR,
		FloorBL,
		FloorFL,
		CaveMaterialID::Tunnel
	);


	// ========================================================
	// 4. Entrance Cap
	// ========================================================

	if (bCapEntrance)
	{
		const int32 EntranceCenter =
			AddVertex(
				FVector3d(
					0.0,
					0.0,
					0.0
				),
				FVector2f(
					0.0f,
					0.0f
				)
			);


		TArray<int32> EntranceArc;


		EntranceArc.Reserve(
			ArcSegments + 1
		);


		for (
			int32 i = 0;
			i <= ArcSegments;
			++i
		)
		{
			const double Theta =
				PI
				*
				static_cast<double>(i)
				/
				static_cast<double>(
					ArcSegments
				);


			const double Y =
				TunnelR
				*
				FMath::Cos(
					Theta
				);


			const double Z =
				TunnelR
				*
				FMath::Sin(
					Theta
				);


			EntranceArc.Add(
				AddVertex(
					FVector3d(
						0.0,
						Y,
						Z
					),

					FVector2f(
						static_cast<float>(
							Y / UVSize
						),
						static_cast<float>(
							Z / UVSize
						)
					)
				)
			);
		}


		if (
			EntranceArc.Num()
			==
			ArcSegments + 1
		)
		{
			for (
				int32 i = 0;
				i < ArcSegments;
				++i
			)
			{
				AddTriangle(
					EntranceCenter,
					EntranceArc[i + 1],
					EntranceArc[i],
					CaveMaterialID::Entrance
				);
			}
		}
	}


	// ========================================================
	// 5. Chamber Rings
	// ========================================================

	TArray<TArray<int32>> DomeRings;


	DomeRings.Reserve(
		DomeSegments
	);


	/*
	 * 첫 Ring은 Tunnel 끝과 같은 Geometry를 사용.
	 */
	DomeRings.Add(
		TunnelEndRing
	);


	for (
		int32 RingIndex = 1;
		RingIndex < DomeSegments;
		++RingIndex
	)
	{
		const double Alpha =
			static_cast<double>(
				RingIndex
			)
			/
			static_cast<double>(
				DomeSegments
			);


		const double Phi =
			PhiOpening
			*
			(
				1.0
				-
				Alpha
			);


		const double X =
			ChamberCenterX
			+
			DomeR
			*
			FMath::Cos(
				Phi
			);


		const double CrossRadius =
			DomeR
			*
			FMath::Sin(
				Phi
			);


		const double DistanceFromOpening =
			(
				PhiOpening
				-
				Phi
			)
			*
			DomeR;


		const float U =
			static_cast<float>(
				(
					L
					+
					DistanceFromOpening
				)
				/
				UVSize
			);


		TArray<int32> Ring;


		Ring.Reserve(
			ArcSegments + 1
		);


		for (
			int32 ArcIndex = 0;
			ArcIndex <= ArcSegments;
			++ArcIndex
		)
		{
			const double Theta =
				PI
				*
				static_cast<double>(
					ArcIndex
				)
				/
				static_cast<double>(
					ArcSegments
				);


			const double Y =
				CrossRadius
				*
				FMath::Cos(
					Theta
				);


			const double Z =
				CrossRadius
				*
				FMath::Sin(
					Theta
				);


			const float V =
				static_cast<float>(
					Theta
					*
					DomeR
					/
					UVSize
				);


			Ring.Add(
				AddVertex(
					FVector3d(
						X,
						Y,
						Z
					),

					FVector2f(
						U,
						V
					)
				)
			);
		}


		if (
			Ring.Num()
			==
			ArcSegments + 1
		)
		{
			DomeRings.Add(
				MoveTemp(
					Ring
				)
			);
		}
	}


	// ========================================================
	// ★ Dome 안전 검사
	// ========================================================

	if (
		DomeRings.Num()
		<
		1
	)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[Cave] DomeRings is empty."
			)
		);

		return;
	}


	// ========================================================
	// 6. Dome Ring -> Ring
	// ========================================================

	for (
		int32 RingIndex = 0;
		RingIndex < DomeRings.Num() - 1;
		++RingIndex
	)
	{
		const TArray<int32>& Current =
			DomeRings[
				RingIndex
			];


		const TArray<int32>& Next =
			DomeRings[
				RingIndex + 1
			];


		if (
			Current.Num()
			!=
			ArcSegments + 1
			||
			Next.Num()
			!=
			ArcSegments + 1
		)
		{
			continue;
		}


		for (
			int32 i = 0;
			i < ArcSegments;
			++i
		)
		{
			AddTriangle(
				Current[i],
				Next[i + 1],
				Next[i],
				CaveMaterialID::Chamber
			);


			AddTriangle(
				Current[i],
				Current[i + 1],
				Next[i + 1],
				CaveMaterialID::Chamber
			);
		}
	}


	// ========================================================
	// 7. Back Pole
	// ========================================================

	const int32 DomePole =
		AddVertex(
			FVector3d(
				ChamberCenterX
				+
				DomeR,

				0.0,

				0.0
			),

			FVector2f(
				static_cast<float>(
					(
						L
						+
						PhiOpening
						*
						DomeR
					)
					/
					UVSize
				),

				0.0f
			)
		);


	/*
	 * Last() 호출 전에 이미 DomeRings.Num() >= 1 검사함
	 */
	const TArray<int32>& LastRing =
		DomeRings.Last();


	if (
		LastRing.Num()
		==
		ArcSegments + 1
	)
	{
		for (
			int32 i = 0;
			i < ArcSegments;
			++i
		)
		{
			AddTriangle(
				LastRing[i],
				LastRing[i + 1],
				DomePole,
				CaveMaterialID::Chamber
			);
		}
	}


	// ========================================================
	// 8. Chamber Floor
	// ========================================================

	TArray<FVector3d> FloorPositions;


	FloorPositions.Add(
		FVector3d(
			L,
			TunnelR,
			0.0
		)
	);


	for (
		int32 RingIndex = 1;
		RingIndex < DomeRings.Num();
		++RingIndex
	)
	{
		const TArray<int32>& Ring =
			DomeRings[
				RingIndex
			];


		if (
			Ring.Num()
			!=
			ArcSegments + 1
		)
		{
			continue;
		}


		const FVector3d P =
			Mesh.GetVertex(
				Ring[0]
			);


		FloorPositions.Add(
			FVector3d(
				P.X,
				P.Y,
				0.0
			)
		);
	}


	FloorPositions.Add(
		FVector3d(
			ChamberCenterX
				+
				DomeR,

			0.0,

			0.0
		)
	);


	for (
		int32 RingIndex =
			DomeRings.Num() - 1;

		RingIndex >= 1;

		--RingIndex
	)
	{
		const TArray<int32>& Ring =
			DomeRings[
				RingIndex
			];


		if (
			Ring.Num()
			!=
			ArcSegments + 1
		)
		{
			continue;
		}


		const FVector3d P =
			Mesh.GetVertex(
				Ring[
					ArcSegments
				]
			);


		FloorPositions.Add(
			FVector3d(
				P.X,
				P.Y,
				0.0
			)
		);
	}


	FloorPositions.Add(
		FVector3d(
			L,
			-TunnelR,
			0.0
		)
	);


	// ========================================================
	// Floor Array 안전 확인
	// ========================================================

	if (
		FloorPositions.Num()
		>=
		3
	)
	{
		const int32 FloorCenter =
			AddVertex(
				FVector3d(
					ChamberCenterX,
					0.0,
					0.0
				),

				FVector2f(
					0.0f,
					0.0f
				)
			);


		TArray<int32> FloorVertices;


		FloorVertices.Reserve(
			FloorPositions.Num()
		);


		for (
			const FVector3d& Position
			:
			FloorPositions
		)
		{
			FloorVertices.Add(
				AddVertex(
					Position,

					FVector2f(
						static_cast<float>(
							(
								Position.X
								-
								ChamberCenterX
							)
							/
							UVSize
						),

						static_cast<float>(
							Position.Y
							/
							UVSize
						)
					)
				)
			);
		}


		if (
			FloorVertices.Num()
			>=
			3
		)
		{
			for (
				int32 i = 0;
				i < FloorVertices.Num() - 1;
				++i
			)
			{
				AddTriangle(
					FloorCenter,
					FloorVertices[i],
					FloorVertices[i + 1],
					CaveMaterialID::Chamber
				);
			}


			/*
			 * 이제 [0]은 Num() >= 3 검사 뒤라 안전
			 */

			AddTriangle(
				FloorCenter,
				FloorVertices.Last(),
				FloorVertices[0],
				CaveMaterialID::Chamber
			);
		}
	}


	// ========================================================
	// 9. 내부 방향
	// ========================================================

	Mesh.ReverseOrientation(
		false
	);


	// ========================================================
	// 10. Vertex Normals
	// ========================================================

	FMeshNormals::QuickComputeVertexNormals(
		Mesh,
		false
	);


	// ========================================================
	// 11. Render Attributes
	// ========================================================

	Mesh.EnableAttributes();


	FDynamicMeshAttributeSet* Attributes =
		Mesh.Attributes();


	if (!Attributes)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[Cave] Attributes creation failed."
			)
		);

		return;
	}


	// UV Layer를 확실하게 1개 생성
	if (
		Attributes->NumUVLayers()
		<
		1
	)
	{
		Attributes->SetNumUVLayers(
			1
		);
	}


	// Normal Layer도 확실하게 1개
	if (
		Attributes->NumNormalLayers()
		<
		1
	)
	{
		Attributes->SetNumNormalLayers(
			1
		);
	}


	// ========================================================
	// 12. Material ID
	// ========================================================

	Attributes->EnableMaterialID();


	FDynamicMeshMaterialAttribute* MaterialIDs =
		Attributes->GetMaterialID();


	if (!MaterialIDs)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[Cave] MaterialID Attribute failed."
			)
		);

		return;
	}


	for (
		int32 TriangleID
		:
		Mesh.TriangleIndicesItr()
	)
	{
		MaterialIDs->SetValue(
			TriangleID,

			Mesh.GetTriangleGroup(
				TriangleID
			)
		);
	}


	// ========================================================
	// 13. UV Overlay
	// ========================================================

	FDynamicMeshUVOverlay* UVOverlay =
		Attributes->PrimaryUV();


	if (!UVOverlay)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[Cave] PrimaryUV is null."
			)
		);

		return;
	}


	if (
		!UE::Geometry::CopyVertexUVsToOverlay(
			Mesh,
			*UVOverlay,
			true
		)
	)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[Cave] UV copy failed."
			)
		);

		return;
	}


	// ========================================================
	// 14. Normal Overlay
	// ========================================================

	FDynamicMeshNormalOverlay* NormalOverlay =
		Attributes->PrimaryNormals();


	if (!NormalOverlay)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[Cave] PrimaryNormals is null."
			)
		);

		return;
	}


	if (
		!UE::Geometry::CopyVertexNormalsToOverlay(
			Mesh,
			*NormalOverlay,
			true
		)
	)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"[Cave] Normal copy failed."
			)
		);

		return;
	}


	// ========================================================
	// 15. Tangent Overlay
	//
	// AutoCalculated를 Component에 맡기지 않고,
	// UV/Normal Overlay가 완성된 지금 Mesh 자체에 Tangent를 만듭니다.
	// 이 방식이면 빈 Mesh의 UV Layer[0]를 읽는 시점을 피할 수 있습니다.
	// ========================================================

	// UE 5.8: tangent/bitangent overlays are NOT created by default.
	// EnableTangents() creates Tangent = Normal layer 1 and Bitangent = Normal layer 2.
	// ComputeDefaultOverlayTangents() writes into those overlays, so they must exist first.
	Attributes->EnableTangents();

	const bool bTangentsBuilt =
		Attributes->HasTangentSpace()
		&&
		FMeshTangentsf::ComputeDefaultOverlayTangents(
			Mesh
		);


	if (!bTangentsBuilt)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"[Cave] Tangent build failed. Rendering will continue without tangent-space normals."
			)
		);
	}


	// ========================================================
	// Debug
	// ========================================================

	LastVertexCount =
		Mesh.VertexCount();


	LastTriangleCount =
		Mesh.TriangleCount();


	// ========================================================
	// 15. Component 적용
	// ========================================================

	MeshComponent->SetMesh(
		MoveTemp(
			Mesh
		)
	);


	// Tangent overlay가 정상 생성된 경우에만
	// Component가 Mesh 내부 Tangent를 사용하게 합니다.
	MeshComponent->SetTangentsType(
		bTangentsBuilt
			? EDynamicMeshComponentTangentsMode::ExternallyProvided
			: EDynamicMeshComponentTangentsMode::NoTangents
	);


	MeshComponent->SetTwoSided(
		bTwoSided
	);


	// ========================================================
	// 16. ★ Material
	//
	// Component에서 Material을 읽지 않습니다.
	// UPROPERTY 세 개만 가지고 적용합니다.
	// ========================================================

	ApplyCaveMaterials();


	// ========================================================
	// Collision OFF
	// ========================================================

	MeshComponent->SetCollisionEnabled(
		ECollisionEnabled::NoCollision
	);


	MeshComponent->SetGenerateOverlapEvents(
		false
	);


	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"[Cave] Build OK | "
			"Vertices=%d | "
			"Triangles=%d"
		),

		LastVertexCount,
		LastTriangleCount
	);
}


// ============================================================
// Component
// ============================================================

UDynamicMeshComponent*
AACaveCutter::GetCaveMeshComponent()
{
	return GetDynamicMeshComponent();
}


// ============================================================
// Collision Data
// ============================================================

FCaveSimpleCollisionData
AACaveCutter::GetSimpleCollisionData() const
{
	FCaveSimpleCollisionData Data;


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
// Server Collision
// ============================================================

bool AACaveCutter::IsPointInsideCaveLocal(
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


	bool bInsideTunnel =
		false;


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


	const FVector Center =
		GetChamberCenterLocal();


	const FVector Delta =
		P - Center;


	bool bInsideChamber =
		false;


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