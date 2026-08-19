#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshActor.h"
#include "ACaveCutter.generated.h"


class UDynamicMeshComponent;
class UMaterialInterface;


/**
 * 외부 서버용 단순 충돌 데이터
 */
USTRUCT(BlueprintType)
struct FCaveSimpleCollisionData
{
	GENERATED_BODY()


	UPROPERTY(BlueprintReadOnly, Category = "Cave Collision")
	FTransform CaveTransform;


	UPROPERTY(BlueprintReadOnly, Category = "Cave Collision")
	float TunnelLength = 0.0f;


	UPROPERTY(BlueprintReadOnly, Category = "Cave Collision")
	float TunnelRadius = 0.0f;


	UPROPERTY(BlueprintReadOnly, Category = "Cave Collision")
	FVector ChamberCenterLocal = FVector::ZeroVector;


	UPROPERTY(BlueprintReadOnly, Category = "Cave Collision")
	float ChamberRadius = 0.0f;
};



UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AACaveCutter
	: public ADynamicMeshActor
{
	GENERATED_BODY()


public:

	AACaveCutter(
		const FObjectInitializer& ObjectInitializer
	);


	virtual void OnConstruction(
		const FTransform& Transform
	) override;


	// ============================================================
	// Tunnel
	// ============================================================

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Tunnel",
		meta = (
			ClampMin = "500.0"
		)
	)
	float TunnelLength = 4000.0f;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Tunnel",
		meta = (
			ClampMin = "100.0"
		)
	)
	float TunnelRadius = 400.0f;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Tunnel",
		meta = (
			ClampMin = "6",
			ClampMax = "64"
		)
	)
	int32 TunnelArcSegments = 24;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Tunnel"
	)
	bool bCapEntrance = true;


	// ============================================================
	// Chamber
	// ============================================================

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Chamber",
		meta = (
			ClampMin = "500.0"
		)
	)
	float ChamberRadius = 3000.0f;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Chamber",
		meta = (
			ClampMin = "4",
			ClampMax = "96"
		)
	)
	int32 ChamberLengthSegments = 32;


	// ============================================================
	// Materials
	//
	// ★ 앞으로 Material은
	// DynamicMeshComponent의 Element에 직접 넣지 말고
	// 이 세 곳에 넣으시면 됩니다.
	// ============================================================

	/**
	 * 통로 + 통로 바닥
	 *
	 * Material ID 0
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Materials"
	)
	TObjectPtr<UMaterialInterface> TunnelMaterial = nullptr;


	/**
	 * 큰 반구 방 + 방 바닥
	 *
	 * Material ID 1
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Materials"
	)
	TObjectPtr<UMaterialInterface> ChamberMaterial = nullptr;


	/**
	 * 입구를 막는 반원 벽
	 *
	 * Material ID 2
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Materials"
	)
	TObjectPtr<UMaterialInterface> EntranceMaterial = nullptr;


	// ============================================================
	// UV
	// ============================================================

	/**
	 * Texture 반복 크기
	 *
	 * 200 = 약 200cm당 1회 반복
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|UV",
		meta = (
			ClampMin = "10.0"
		)
	)
	float UVWorldSize = 200.0f;


	// ============================================================
	// Rendering
	// ============================================================

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Rendering"
	)
	bool bTwoSided = true;


	// ============================================================
	// Generation
	// ============================================================

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Generation"
	)
	bool bAutoRebuild = true;


	// ============================================================
	// Debug
	// ============================================================

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Transient,
		Category = "Cave|Debug"
	)
	int32 LastVertexCount = 0;


	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Transient,
		Category = "Cave|Debug"
	)
	int32 LastTriangleCount = 0;


	// ============================================================
	// Functions
	// ============================================================

	UFUNCTION(
		BlueprintCallable,
		CallInEditor,
		Category = "Cave"
	)
	void RebuildCave();


	UFUNCTION(
		BlueprintCallable,
		CallInEditor,
		Category = "Cave|Materials"
	)
	void ApplyCaveMaterials();


	UFUNCTION(
		BlueprintPure,
		Category = "Cave"
	)
	UDynamicMeshComponent*
	GetCaveMeshComponent();


	UFUNCTION(
		BlueprintPure,
		Category = "Cave|Collision"
	)
	FVector GetChamberCenterLocal() const;


	UFUNCTION(
		BlueprintPure,
		Category = "Cave|Collision"
	)
	FCaveSimpleCollisionData
	GetSimpleCollisionData() const;


	UFUNCTION(
		BlueprintPure,
		Category = "Cave|Collision"
	)
	bool IsPointInsideCaveLocal(
		FVector LocalPosition
	) const;


private:

	float GetChamberOffset() const;
};