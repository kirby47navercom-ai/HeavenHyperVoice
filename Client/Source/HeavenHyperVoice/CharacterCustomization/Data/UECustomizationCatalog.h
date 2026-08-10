#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UECustomizationCatalog.generated.h"

class USkeletalMesh;
class UTexture2D;

UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUECustomizationCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleBodyCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleFaceSkinCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleEyeWhiteCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleEyeIrisCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleEyeHighlightCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleEyeExtraCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleBrowCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleEyelashCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleEyelineCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleMouthCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleHairBaseCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleHairFrontCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleHairSideCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleHairBackCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleHairExtraCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleTopCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleBottomCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleOnepieceCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male")
	TArray<TObjectPtr<USkeletalMesh>> MaleShoesCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male|Accessories")
	TArray<TObjectPtr<USkeletalMesh>> MaleHeadAccessoryCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male|Accessories")
	TArray<TObjectPtr<USkeletalMesh>> MaleFaceAccessoryCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male|Accessories")
	TArray<TObjectPtr<USkeletalMesh>> MaleEarAccessoryCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male|Accessories")
	TArray<TObjectPtr<USkeletalMesh>> MaleTailAccessoryCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Male|Accessories")
	TArray<TObjectPtr<USkeletalMesh>> MaleNeckAccessoryCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleBodyCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleFaceSkinCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleEyeWhiteCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleEyeIrisCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleEyeHighlightCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleEyeExtraCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleBrowCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleEyelashCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleEyelineCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleMouthCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleHairBaseCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleHairFrontCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleHairSideCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleHairBackCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleHairExtraCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleTopCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleBottomCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleOnepieceCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female")
	TArray<TObjectPtr<USkeletalMesh>> FemaleShoesCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female|Accessories")
	TArray<TObjectPtr<USkeletalMesh>> FemaleHeadAccessoryCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female|Accessories")
	TArray<TObjectPtr<USkeletalMesh>> FemaleFaceAccessoryCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female|Accessories")
	TArray<TObjectPtr<USkeletalMesh>> FemaleEarAccessoryCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female|Accessories")
	TArray<TObjectPtr<USkeletalMesh>> FemaleTailAccessoryCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes|Female|Accessories")
	TArray<TObjectPtr<USkeletalMesh>> FemaleNeckAccessoryCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Textures")
	TArray<TObjectPtr<UTexture2D>> FaceSkinTextureCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Textures")
	TArray<TObjectPtr<UTexture2D>> FaceSkinNormalTextureCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Textures")
	TArray<TObjectPtr<UTexture2D>> EyeWhiteTextureCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Textures")
	TArray<TObjectPtr<UTexture2D>> EyeIrisTextureCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Textures")
	TArray<TObjectPtr<UTexture2D>> EyeHighlightTextureCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Textures")
	TArray<TObjectPtr<UTexture2D>> BrowTextureCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Textures")
	TArray<TObjectPtr<UTexture2D>> EyelashTextureCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Textures")
	TArray<TObjectPtr<UTexture2D>> EyelineTextureCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Textures")
	TArray<TObjectPtr<UTexture2D>> MouthTextureCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Textures")
	TArray<TObjectPtr<UTexture2D>> LipTextureCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Textures")
	TArray<TObjectPtr<UTexture2D>> MouthLineTextureCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
	TArray<int32> MaleHairStyleIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
	TArray<int32> FemaleHairStyleIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fit")
	float MaleHeadPivotY = 5.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fit")
	float MaleHeadPivotZ = 162.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fit")
	float MaleHeadVerticalOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fit")
	float FemaleHeadPivotY = 4.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fit")
	float FemaleHeadPivotZ = 143.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fit")
	float FemaleHeadVerticalOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fit", meta = (ClampMin = "1.0", ClampMax = "1.2"))
	float HairRadialScale = 1.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fit", meta = (ClampMin = "0.9", ClampMax = "1.0"))
	float HairScalpInsetScale = 0.985f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fit")
	float ScleraDepthOffsetY = 0.005f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fit")
	float HeadAccessoryVerticalOffset = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fit")
	float FaceAccessoryForwardOffset = 6.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fit")
	float FaceAccessoryVerticalOffset = 17.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	FLinearColor ReferenceFaceSkinColor = FLinearColor(0.863157f, 0.485150f, 0.309469f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	FLinearColor BodySkinColorGain = FLinearColor(1.15f, 1.50f, 1.50f, 1.0f);
};
