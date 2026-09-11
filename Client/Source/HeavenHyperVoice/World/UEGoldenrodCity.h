#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UEGoldenrodCity.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UInstancedStaticMeshComponent;

/** Goldenrod R03: visual mesh plus native floor, building and shoreline collision.
 * ServerGround/ServerWall components are exported by HHVMapExport.
 */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEGoldenrodCity : public AActor
{
	GENERATED_BODY()
public:
	AUEGoldenrodCity();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Goldenrod")
	TObjectPtr<UStaticMeshComponent> CityMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Goldenrod|Boundary")
	TObjectPtr<UInstancedStaticMeshComponent> BoundaryWalls;
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Goldenrod|Boundary")
	void RebuildBoundaryWalls();
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Goldenrod|Boundary Camera", meta=(ClampMin="1"))
	float CameraTransitionDistance = 2600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Goldenrod|Boundary Camera", meta=(ClampMin="0"))
	float CameraFullEffectDistance = 350.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Goldenrod|Boundary Camera", meta=(ClampMin="-85", ClampMax="-30"))
	float BoundaryCameraPitch = -72.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Goldenrod|Boundary Camera", meta=(ClampMin="0"))
	float BoundaryCameraExtraDistance = 220.f;
	float GetBoundaryCameraWeight(const FVector& WorldPosition) const;
private:
	void AddCollisionBox(const TCHAR* Name, FVector Min, FVector Max, bool bGround);
	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> ShoreBoxes;
};
