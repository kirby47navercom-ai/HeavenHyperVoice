#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#if WITH_EDITOR
#include "Containers/Ticker.h"
#endif

#include "UEDirtPathDecalActor.generated.h"


class UDecalComponent;
class UMaterialInterface;
class USceneComponent;


UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "Dirt Path Decal"))
class HEAVENHYPERVOICE_API AUEDirtPathDecalActor : public AActor
{
	GENERATED_BODY()


public:
	AUEDirtPathDecalActor();


	virtual void OnConstruction(const FTransform& Transform) override;


	/** Move this viewport widget on X and Y to set the area filled with decal pieces. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Dirt Path|Area",
		meta = (MakeEditWidget = "true", DisplayName = "Area End")
	)
	FVector PathEnd = FVector(2000.0f, 0.0f, 0.0f);


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dirt Path|Decal")
	TObjectPtr<UMaterialInterface> DecalMaterial;


	/** Uniformly changes the real size of every generated decal piece. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Dirt Path|Decal",
		meta = (ClampMin = "0.01", DisplayName = "Decal Piece Scale")
	)
	float DecalPieceScale = 1.0f;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Dirt Path|Decal",
		meta = (ClampMin = "1.0", Units = "cm")
	)
	float PathWidth = 400.0f;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Dirt Path|Decal",
		meta = (ClampMin = "1.0", Units = "cm")
	)
	float PieceLength = 400.0f;


	/** Keep this below Piece Length when the material needs overlap between columns. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Dirt Path|Decal",
		meta = (ClampMin = "1.0", Units = "cm")
	)
	float PieceSpacing = 320.0f;


	/** Keep this below Path Width when the material needs overlap between rows. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Dirt Path|Decal",
		meta = (ClampMin = "1.0", Units = "cm")
	)
	float RowSpacing = 320.0f;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Dirt Path|Decal",
		meta = (ClampMin = "1.0", Units = "cm")
	)
	float ProjectionDepth = 200.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dirt Path|Decal")
	int32 SortOrder = 0;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Dirt Path|Decal",
		meta = (ClampMin = "0.0", ClampMax = "1.0")
	)
	float FadeScreenSize = 0.01f;


	/**
	 * Decals are not generated inside any of these Blueprint actor spaces.
	 * Vegetation Scatter and Random Mesh Scatter use their exact Radius;
	 * other Blueprint actors use their component bounds.
	 */
	UPROPERTY(
		EditInstanceOnly,
		BlueprintReadWrite,
		Category = "Dirt Path|Excluded Blueprint Area",
		meta = (DisplayName = "Excluded Blueprint Areas")
	)
	TArray<TObjectPtr<AActor>> ExcludedGrassAreas;


	/** Subdivides decal pieces only where they cross an excluded area boundary. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Dirt Path|Excluded Blueprint Area",
		meta = (ClampMin = "0", ClampMax = "6")
	)
	int32 ExclusionSubdivisionDepth = 4;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dirt Path|Ground")
	bool bLandscapeOnly = true;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dirt Path|Ground")
	float SurfaceOffset = 2.0f;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Dirt Path|Ground",
		meta = (ClampMin = "1.0", Units = "cm")
	)
	float TraceHeightAbove = 100000.0f;


	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Dirt Path|Ground",
		meta = (ClampMin = "1.0", Units = "cm")
	)
	float TraceDepthBelow = 100000.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dirt Path|Ground")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dirt Path|Generation")
	bool bAutoRebuild = true;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Dirt Path|Result")
	int32 GeneratedDecalCount = 0;


	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Dirt Path")
	void RebuildPath();


	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Dirt Path")
	void ClearPath();


private:
	enum class EExcludedAreaRelation : uint8
	{
		Outside,
		Partial,
		Inside
	};


	EExcludedAreaRelation ClassifyExcludedAreaOverlap(
		const FVector& Center,
		const FVector& PathDirection,
		float Width,
		float Length
	) const;


#if WITH_EDITOR
	void QueueDeferredEditorRebuild();


	FTSTicker::FDelegateHandle PendingRebuildHandle;
#endif


	UPROPERTY(VisibleAnywhere, Category = "Dirt Path")
	TObjectPtr<USceneComponent> SceneRoot;


	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> GeneratedDecals;
};
