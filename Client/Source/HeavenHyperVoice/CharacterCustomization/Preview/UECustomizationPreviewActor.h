#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Data/UECharacterCustomizationTypes.h"
#include "UECustomizationPreviewActor.generated.h"

class UCameraComponent;
class UUECustomizationCatalog;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USceneComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USpotLightComponent;
class UTexture2D;

/** Runtime VRoid modular character used by the character-creation level. */
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

	UFUNCTION(BlueprintPure, Category = "Customization")
	int32 GetOptionCount(EUECustomizationPart Part) const;

	UFUNCTION(BlueprintPure, Category = "Customization")
	FString GetOptionLabel(EUECustomizationPart Part, int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Customization")
	UTexture2D* GetOptionTexture(EUECustomizationPart Part, int32 Index) const;

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void InitializeCatalogs();

	UFUNCTION(BlueprintCallable, Category = "Customization") void RotatePreview(float DeltaYaw);
	UFUNCTION(BlueprintCallable, Category = "Customization") void ResetAppearance();
	UFUNCTION(BlueprintCallable, Category = "Customization") void RandomizeAppearance();
	UFUNCTION(BlueprintCallable, Category = "Customization") bool SaveAppearance() const;
	UFUNCTION(BlueprintCallable, Category = "Customization") bool LoadAppearance();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USceneComponent> SceneRoot = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USceneComponent> CharacterRoot = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<UCameraComponent> PreviewCamera = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USpotLightComponent> PortraitLight = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> BodyMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> FaceSkinMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> EyeWhiteMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> EyeIrisMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> EyeHighlightMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> EyeExtraMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> BrowMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> EyelashMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> EyelineMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> MouthMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> LipOverlayMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> MouthLineOverlayMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> HairScalpMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> HairBaseMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> HairFrontMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> HairSideMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> HairBackMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> HairExtraMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> TopMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> BottomMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> OnepieceMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> ShoesMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> HeadAccessoryMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> FaceAccessoryMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> EarAccessoryMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> TailAccessoryMesh = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components") TObjectPtr<USkeletalMeshComponent> NeckAccessoryMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	FUECharacterCustomizationData Appearance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Catalog")
	TObjectPtr<UUECustomizationCatalog> CatalogAsset = nullptr;

#define UE_VROID_CATALOG(Name) \
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Catalog", meta = (AllowPrivateAccess = "true")) \
	TArray<TObjectPtr<USkeletalMesh>> Name

	UE_VROID_CATALOG(MaleBodyCatalog);
	UE_VROID_CATALOG(MaleFaceSkinCatalog);
	UE_VROID_CATALOG(MaleEyeWhiteCatalog);
	UE_VROID_CATALOG(MaleEyeIrisCatalog);
	UE_VROID_CATALOG(MaleEyeHighlightCatalog);
	UE_VROID_CATALOG(MaleEyeExtraCatalog);
	UE_VROID_CATALOG(MaleBrowCatalog);
	UE_VROID_CATALOG(MaleEyelashCatalog);
	UE_VROID_CATALOG(MaleEyelineCatalog);
	UE_VROID_CATALOG(MaleMouthCatalog);
	UE_VROID_CATALOG(MaleHairBaseCatalog);
	UE_VROID_CATALOG(MaleHairFrontCatalog);
	UE_VROID_CATALOG(MaleHairSideCatalog);
	UE_VROID_CATALOG(MaleHairBackCatalog);
	UE_VROID_CATALOG(MaleHairExtraCatalog);
	UE_VROID_CATALOG(MaleTopCatalog);
	UE_VROID_CATALOG(MaleBottomCatalog);
	UE_VROID_CATALOG(MaleOnepieceCatalog);
	UE_VROID_CATALOG(MaleShoesCatalog);
	UE_VROID_CATALOG(MaleHeadAccessoryCatalog);
	UE_VROID_CATALOG(MaleFaceAccessoryCatalog);
	UE_VROID_CATALOG(MaleEarAccessoryCatalog);
	UE_VROID_CATALOG(MaleTailAccessoryCatalog);
	UE_VROID_CATALOG(MaleNeckAccessoryCatalog);

	UE_VROID_CATALOG(FemaleBodyCatalog);
	UE_VROID_CATALOG(FemaleFaceSkinCatalog);
	UE_VROID_CATALOG(FemaleEyeWhiteCatalog);
	UE_VROID_CATALOG(FemaleEyeIrisCatalog);
	UE_VROID_CATALOG(FemaleEyeHighlightCatalog);
	UE_VROID_CATALOG(FemaleEyeExtraCatalog);
	UE_VROID_CATALOG(FemaleBrowCatalog);
	UE_VROID_CATALOG(FemaleEyelashCatalog);
	UE_VROID_CATALOG(FemaleEyelineCatalog);
	UE_VROID_CATALOG(FemaleMouthCatalog);
	UE_VROID_CATALOG(FemaleHairBaseCatalog);
	UE_VROID_CATALOG(FemaleHairFrontCatalog);
	UE_VROID_CATALOG(FemaleHairSideCatalog);
	UE_VROID_CATALOG(FemaleHairBackCatalog);
	UE_VROID_CATALOG(FemaleHairExtraCatalog);
	UE_VROID_CATALOG(FemaleTopCatalog);
	UE_VROID_CATALOG(FemaleBottomCatalog);
	UE_VROID_CATALOG(FemaleOnepieceCatalog);
	UE_VROID_CATALOG(FemaleShoesCatalog);
	UE_VROID_CATALOG(FemaleHeadAccessoryCatalog);
	UE_VROID_CATALOG(FemaleFaceAccessoryCatalog);
	UE_VROID_CATALOG(FemaleEarAccessoryCatalog);
	UE_VROID_CATALOG(FemaleTailAccessoryCatalog);
	UE_VROID_CATALOG(FemaleNeckAccessoryCatalog);

#undef UE_VROID_CATALOG

#define UE_VROID_TEXTURE_CATALOG(Name) \
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> Name

	UE_VROID_TEXTURE_CATALOG(FaceSkinTextureCatalog);
	UE_VROID_TEXTURE_CATALOG(FaceSkinNormalTextureCatalog);
	UE_VROID_TEXTURE_CATALOG(EyeWhiteTextureCatalog);
	UE_VROID_TEXTURE_CATALOG(EyeIrisTextureCatalog);
	UE_VROID_TEXTURE_CATALOG(EyeHighlightTextureCatalog);
	UE_VROID_TEXTURE_CATALOG(BrowTextureCatalog);
	UE_VROID_TEXTURE_CATALOG(EyelashTextureCatalog);
	UE_VROID_TEXTURE_CATALOG(EyelineTextureCatalog);
	UE_VROID_TEXTURE_CATALOG(MouthTextureCatalog);
	UE_VROID_TEXTURE_CATALOG(LipTextureCatalog);
	UE_VROID_TEXTURE_CATALOG(MouthLineTextureCatalog);

#undef UE_VROID_TEXTURE_CATALOG

private:
	USkeletalMeshComponent* CreatePart(const FName& PartName);
	USkeletalMesh* SelectMesh(const TArray<TObjectPtr<USkeletalMesh>>& Catalog, int32 Index) const;
	const TArray<TObjectPtr<USkeletalMesh>>& GetCatalog(EUECustomizationPart Part) const;
	const TArray<TObjectPtr<UTexture2D>>& GetTextureCatalog(EUECustomizationPart Part) const;
	void ApplyCatalogAsset();
	void ValidateCatalogSkeletons() const;
	void ClampAppearanceToCatalogs();
	bool UpdateMeshes();
	void BindFollowerPoses();
	void CreateDynamicMaterials();
	void CreateMaterialsForComponent(
		USkeletalMeshComponent* Component,
		TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutMaterials,
		UMaterialInterface* TemplateMaterial = nullptr);
	void CreateOpaqueFaceMaterials(
		USkeletalMeshComponent* Component,
		TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutMaterials,
		UMaterialInterface* TemplateMaterial = nullptr);
	void CreateMaskedOverlayMaterials(
		USkeletalMeshComponent* Component,
		TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutMaterials,
		UMaterialInterface* TemplateMaterial = nullptr);
	void ApplySelectedTextures();
	void ApplyColors();
	void ApplyNeutralMaterialLighting();
	void ApplyTransforms();
	void HideLegacyPreviewParts();
	void CaptureQAScreenshot();
	void PrepareQAHeadScreenshot();
	void CaptureQAHeadScreenshot();
	void ExitAfterQAScreenshot();
	static void SetMaterialColor(const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials, const FName& Parameter, const FLinearColor& Color);
	static void SetMaterialScalar(const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials, const FName& Parameter, float Value);

	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> BodySkinMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> FaceSkinMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> HairScalpMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> HairBaseMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> HairMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> EyeMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> EyeWhiteMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> EyeHighlightMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> EyeExtraMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> BrowMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> EyelashMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> EyelineMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> MouthMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> LipMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> MouthLineMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> TopMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> BottomMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> OnepieceMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> ShoesMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> OutfitMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> HeadAccessoryMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> FaceAccessoryMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> EarAccessoryMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> TailAccessoryMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> NeckAccessoryMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> AccessoryMaterials;
	FTimerHandle QAScreenshotTimer;
	FTimerHandle QAPrepareHeadTimer;
	FTimerHandle QAHeadScreenshotTimer;
	FTimerHandle QAExitTimer;

	static const FString SaveSlotName;
	static constexpr int32 SaveUserIndex = 0;
};
