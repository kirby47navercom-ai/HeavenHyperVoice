#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UECave.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;


USTRUCT(BlueprintType)
struct FUECaveSimpleCollisionData
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
class HEAVENHYPERVOICE_API AUECave : public AActor
{
	GENERATED_BODY()

public:
	AUECave();

	virtual void OnConstruction(const FTransform& Transform) override;

	// ============================================================
	// Component
	// ============================================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cave")
	TObjectPtr<UProceduralMeshComponent> CaveMesh;

	// ============================================================
	// Tunnel
	// ============================================================

	/** Length from the capped entrance to the chamber opening. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Tunnel",
		meta = (ClampMin = "500.0")
	)
	float TunnelLength = 4000.0f;

	/**
	 * Half-cylinder radius.
	 * Radius 400 cm -> width 800 cm, height 400 cm.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Tunnel",
		meta = (ClampMin = "100.0")
	)
	float TunnelRadius = 400.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Tunnel",
		meta = (ClampMin = "6", ClampMax = "64")
	)
	int32 TunnelArcSegments = 24;

	/** Fill the X=0 half-circle opening with a wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Tunnel")
	bool bCapEntrance = true;

	// ============================================================
	// Chamber
	// ============================================================

	/**
	 * Radius of the large upper-hemisphere room.
	 * 3000 cm -> roughly 60 m wide, 30 m high.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Chamber",
		meta = (ClampMin = "500.0")
	)
	float ChamberRadius = 3000.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|Chamber",
		meta = (ClampMin = "4", ClampMax = "96")
	)
	int32 ChamberLengthSegments = 32;

	// ============================================================
	// Materials
	//
	// Do NOT set materials on the component manually.
	// Set these three Actor properties instead.
	// ============================================================

	/** Section 0: tunnel wall + tunnel floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Materials")
	TObjectPtr<UMaterialInterface> TunnelMaterial = nullptr;

	/** Section 1: chamber wall + chamber floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Materials")
	TObjectPtr<UMaterialInterface> ChamberMaterial = nullptr;

	/** Section 2: entrance cap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Materials")
	TObjectPtr<UMaterialInterface> EntranceMaterial = nullptr;

	// ============================================================
	// UV
	// ============================================================

	/**
	 * Approximate world size corresponding to UV distance 1.
	 * 200 = texture repeats about every 200 cm.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Cave|UV",
		meta = (ClampMin = "10.0")
	)
	float UVWorldSize = 200.0f;

	// ============================================================
	// Rendering
	// ============================================================

	/**
	 * ProceduralMeshComponent does not force arbitrary materials
	 * to render both sides. When true, the generated cave geometry
	 * is duplicated with reversed winding, so both inside and outside
	 * are physically renderable even with one-sided materials.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rendering")
	bool bTwoSidedGeometry = true;

	// ============================================================
	// Generation
	// ============================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Generation")
	bool bAutoRebuild = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cave|Debug")
	int32 LastTunnelVertexCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cave|Debug")
	int32 LastChamberVertexCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cave|Debug")
	int32 LastEntranceVertexCount = 0;

	// ============================================================
	// Functions
	// ============================================================

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cave")
	void RebuildCave();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cave|Materials")
	void ApplyCaveMaterials();

	UFUNCTION(BlueprintPure, Category = "Cave")
	UProceduralMeshComponent* GetCaveMeshComponent() const;

	UFUNCTION(BlueprintPure, Category = "Cave|Collision")
	FVector GetChamberCenterLocal() const;

	UFUNCTION(BlueprintPure, Category = "Cave|Collision")
	FUECaveSimpleCollisionData GetSimpleCollisionData() const;

	UFUNCTION(BlueprintPure, Category = "Cave|Collision")
	bool IsPointInsideCaveLocal(FVector LocalPosition) const;

private:
	float GetChamberOffset() const;
};	