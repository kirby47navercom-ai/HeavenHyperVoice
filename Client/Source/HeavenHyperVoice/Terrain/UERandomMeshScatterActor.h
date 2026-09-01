#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "UERandomMeshScatterActor.generated.h"


class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class USphereComponent;
class UStaticMesh;


UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "Random Mesh Scatter"))
class HEAVENHYPERVOICE_API AUERandomMeshScatterActor : public AActor
{
	GENERATED_BODY()


public:
	AUERandomMeshScatterActor();


	virtual void OnConstruction(const FTransform& Transform) override;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Area", meta = (ClampMin = "1.0"))
	float Radius = 3000.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Area", meta = (ClampMin = "0"))
	int32 InstanceCount = 100;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Area")
	int32 RandomSeed = 1337;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Meshes")
	TArray<TObjectPtr<UStaticMesh>> StaticMeshes;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Scale", meta = (ClampMin = "0.001"))
	float MinScale = 0.5f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Scale", meta = (ClampMin = "0.001"))
	float MaxScale = 2.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Ground")
	float SurfaceOffset = 0.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Ground")
	bool bLandscapeOnly = true;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Ground")
	bool bAlignToSurface = true;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Random Scatter|Ground",
		meta = (ClampMin = "0.0", ClampMax = "90.0")
	)
	float MaxGroundSlopeDegrees = 45.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Ground", meta = (ClampMin = "1.0"))
	float TraceHeightAbove = 100000.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Ground", meta = (ClampMin = "1.0"))
	float TraceDepthBelow = 100000.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Ground")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Random Scatter|Generation",
		meta = (ClampMin = "1", ClampMax = "100")
	)
	int32 MaxAttemptsPerInstance = 5;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Generation")
	bool bAutoRebuild = true;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Rendering", meta = (ClampMin = "0"))
	int32 StartCullDistance = 20000;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Scatter|Rendering", meta = (ClampMin = "0"))
	int32 EndCullDistance = 50000;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Random Scatter|Result")
	int32 PlacedInstanceCount = 0;


	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Random Scatter")
	void RebuildScatter();


	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Random Scatter")
	void ClearScatter();


private:
	UPROPERTY(VisibleAnywhere, Category = "Random Scatter")
	TObjectPtr<USceneComponent> SceneRoot;


	UPROPERTY(VisibleAnywhere, Category = "Random Scatter|Area")
	TObjectPtr<USphereComponent> RadiusPreview;


	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GeneratedComponents;
};
