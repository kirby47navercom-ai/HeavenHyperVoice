#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Data/UECharacterCustomizationTypes.h"
#include "UECustomizationPreviewActor.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Independent preview actor used only by the customization scene.
 *
 * It uses engine primitive meshes so the feature remains visible before final
 * modular character art arrives. Production skeletal parts can later consume
 * the same FUECharacterCustomizationData structure.
 */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUECustomizationPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AUECustomizationPreviewActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void ApplyAppearance(const FUECharacterCustomizationData& NewAppearance);

	UFUNCTION(BlueprintPure, Category = "Customization")
	const FUECharacterCustomizationData& GetAppearance() const { return Appearance; }

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void RotatePreview(float DeltaYaw);

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void ResetAppearance();

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void RandomizeAppearance();

	UFUNCTION(BlueprintCallable, Category = "Customization")
	bool SaveAppearance() const;

	UFUNCTION(BlueprintCallable, Category = "Customization")
	bool LoadAppearance();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<USceneComponent> CharacterRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UCameraComponent> PreviewCamera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UStaticMeshComponent> Head = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UStaticMeshComponent> Hair = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UStaticMeshComponent> Torso = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UStaticMeshComponent> Pelvis = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UStaticMeshComponent> LeftArm = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UStaticMeshComponent> RightArm = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UStaticMeshComponent> LeftLeg = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UStaticMeshComponent> RightLeg = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UStaticMeshComponent> LeftBoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UStaticMeshComponent> RightBoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UStaticMeshComponent> Accessory = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	FUECharacterCustomizationData Appearance;

private:
	UStaticMeshComponent* CreatePart(const FName& PartName);
	void CreateDynamicMaterials();
	void ApplyColors();
	void ApplyTransforms();
	void SetMaterialColor(UMaterialInstanceDynamic* Material, const FLinearColor& Color) const;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkinMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HairMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OutfitMaterial = nullptr;

	static const FString SaveSlotName;
	static constexpr int32 SaveUserIndex = 0;
};
