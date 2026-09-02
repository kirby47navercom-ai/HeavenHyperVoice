#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "UEVegetationScatterActor.generated.h"


class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class USphereComponent;
class UStaticMesh;


UCLASS(Blueprintable, meta = (DisplayName = "Vegetation Scatter"))
class HEAVENHYPERVOICE_API AUEVegetationScatterActor : public AActor
{
	GENERATED_BODY()


public:
	AUEVegetationScatterActor();


	virtual void OnConstruction(const FTransform& Transform) override;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation|Area", meta = (ClampMin = "1.0"))
	float Radius = 100000.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation|Area", meta = (ClampMin = "0"))
	int32 InstanceCount = 1000;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation|Area")
	int32 RandomSeed = 1337;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Vegetation|Meshes",
		meta = (DisplayName = "Static Meshes")
	)
	TArray<TObjectPtr<UStaticMesh>> StaticMeshes;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Vegetation|Meshes",
		meta = (ClampMin = "0.001", DisplayName = "Min Scale")
	)
	float MinScale = 0.8f;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Vegetation|Meshes",
		meta = (ClampMin = "0.001", DisplayName = "Max Scale")
	)
	float MaxScale = 1.2f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation|Meshes")
	float SurfaceOffset = 0.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation|Ground")
	bool bLandscapeOnly = true;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation|Ground")
	bool bAlignToSurface = true;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Vegetation|Ground",
		meta = (ClampMin = "0.0", ClampMax = "90.0")
	)
	float MaxGroundSlopeDegrees = 45.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation|Ground", meta = (ClampMin = "1.0"))
	float TraceHeightAbove = 100000.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation|Ground", meta = (ClampMin = "1.0"))
	float TraceDepthBelow = 100000.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation|Ground")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Vegetation|Generation",
		meta = (ClampMin = "1", ClampMax = "100")
	)
	int32 MaxAttemptsPerInstance = 5;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation|Generation")
	bool bAutoRebuild = true;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation|Rendering", meta = (ClampMin = "0"))
	int32 StartCullDistance = 20000;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation|Rendering", meta = (ClampMin = "0"))
	int32 EndCullDistance = 50000;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Vegetation|Result")
	int32 PlacedInstanceCount = 0;


	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Vegetation")
	void RebuildVegetation();


	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Vegetation")
	void ClearVegetation();


private:
	UPROPERTY(VisibleAnywhere, Category = "Vegetation")
	TObjectPtr<USceneComponent> SceneRoot;


	UPROPERTY(VisibleAnywhere, Category = "Vegetation|Area")
	TObjectPtr<USphereComponent> RadiusPreview;


	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GeneratedComponents;
};
