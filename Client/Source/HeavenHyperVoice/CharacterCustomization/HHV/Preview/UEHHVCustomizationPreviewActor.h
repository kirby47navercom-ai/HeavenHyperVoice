#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/Actor.h"
#include "../Data/UEHHVCustomizationTypes.h"
#include "UEHHVCustomizationPreviewActor.generated.h"

class UCameraComponent;
class UDirectionalLightComponent;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class USkeletalMeshComponent;
class USkyLightComponent;
class USpotLightComponent;
class UUEHHVCustomizationCatalog;

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEHHVCustomizationPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AUEHHVCustomizationPreviewActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void ApplyAppearance(const FUEHHVAppearance& NewAppearance);

	UFUNCTION(BlueprintPure, Category = "Customization")
	const FUEHHVAppearance& GetAppearance() const { return Appearance; }

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SelectOption(EUEHHVCustomizationCategory Category, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Customization")
	int32 GetOptionCount(EUEHHVCustomizationCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "Customization")
	FString GetOptionLabel(EUEHHVCustomizationCategory Category, int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Customization")
	UUEHHVCustomizationCatalog* GetCatalog() const { return Catalog; }

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SelectGender(EUEHHVGender NewGender);

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SetColor(EUEHHVColorChannel Channel, const FLinearColor& Color);

	UFUNCTION(BlueprintCallable, Category = "Customization|Preview")
	void AddPreviewYaw(float DeltaYaw);

	UFUNCTION(BlueprintCallable, Category = "Customization|Preview")
	void AddPreviewZoom(float DeltaZoom);

	UFUNCTION(BlueprintCallable, Category = "Customization|Preview")
	void AddPreviewPan(FVector2D DeltaPixels);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<USceneComponent> CharacterRoot = nullptr;

	// 원본 의상 메시와 원본 머티리얼을 그대로 보여준다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<USkeletalMeshComponent> BaseBodyMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<USkeletalMeshComponent> BodyEquipmentMesh = nullptr;

	// 얼굴 프리셋 메시다. 몸과 같은 루트 아래에서 같이 움직인다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<USkeletalMeshComponent> HeadMesh = nullptr;

	// 머리카락 프리셋 메시다. 머리만 따로 뜨지 않게 공통 루트에 둔다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<USkeletalMeshComponent> HairMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UCameraComponent> PreviewCamera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<USpotLightComponent> KeyLight = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UDirectionalLightComponent> PreviewDirectionalLight = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<USkyLightComponent> FillLight = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UPointLightComponent> FrontLight = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization|Components")
	TObjectPtr<UPointLightComponent> BodyFillLight = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<UUEHHVCustomizationCatalog> Catalog = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	FUEHHVAppearance Appearance;

	UPROPERTY(EditAnywhere, Category = "Customization|Preview")
	float PreviewYawDegrees = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Customization|Preview")
	float PreviewZoom = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Customization|Preview")
	FVector2D PreviewPanPixels = FVector2D::ZeroVector;

private:
	void ApplyQACommandLineAppearance();
	void CaptureQAWidgetScreenshot();
	void CaptureQAScreenshot();
	void PrepareQAHeadScreenshot();
	void CaptureQAHeadScreenshot();
	void ExitAfterQAScreenshot();
	void BeginQABatch();
	void PrepareNextQABatchCase();
	void CaptureCurrentQABatchCase();
	void ConfigurePreviewLighting();
	void FramePreviewCamera();
	void PreparePreviewStage();
	void NormalizeLegacyDefaultColors();
	void RefreshMeshes();
	void RefreshFollowerPose();
	void ApplyMaterialColors();
	void ApplyEyeMaterial(const FUEHHVCustomizationOption& Option);
	bool IsEyeIrisMaterialSlot(USkeletalMeshComponent* Component, int32 MaterialIndex) const;
	void ApplyColorToSlots(USkeletalMeshComponent* Component, const FLinearColor& Color, const TArray<FString>& SlotContains);
	void ResetComponentMaterials(USkeletalMeshComponent* Component);
	void ApplyMeshLocalMaterials(USkeletalMeshComponent* Component);
	void ApplyMorphSafeMaterials(USkeletalMeshComponent* Component);
	void HideFaceCoverSections(USkeletalMeshComponent* Component);
	void HideBaseBodyOutfitSections(USkeletalMeshComponent* Component);
	void HideEquipmentSkinSections(USkeletalMeshComponent* Component);
	void HideUnsupportedAttachmentComponents();
	void ApplyScale();
	const FUEHHVCustomizationOption& GetOption(EUEHHVCustomizationCategory Category, int32 Index) const;
	int32& MutableIndex(EUEHHVCustomizationCategory Category);
	int32 GetIndex(EUEHHVCustomizationCategory Category) const;
	static int32 ClampIndex(int32 Index, int32 Count);

	FTimerHandle QAScreenshotTimer;
	FTimerHandle QAWidgetScreenshotTimer;
	FTimerHandle QAPrepareHeadTimer;
	FTimerHandle QAHeadScreenshotTimer;
	FTimerHandle QAExitTimer;
	FTimerHandle QABatchTimer;
	int32 QABatchGenderIndex = 0;
	int32 QABatchPhase = 0;
	int32 QABatchCaseIndex = 0;
};
