#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/Actor.h"
#include "../Data/UEPalworldCustomizationTypes.h"
#include "UEPalworldCustomizationPreviewActor.generated.h"

class UCameraComponent;
class UDirectionalLightComponent;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class USkeletalMeshComponent;
class USkyLightComponent;
class USpotLightComponent;
class UUEPalworldCustomizationCatalog;

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEPalworldCustomizationPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AUEPalworldCustomizationPreviewActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

	UFUNCTION(BlueprintCallable, Category = "Palworld")
	void ApplyAppearance(const FUEPalworldAppearance& NewAppearance);

	UFUNCTION(BlueprintPure, Category = "Palworld")
	const FUEPalworldAppearance& GetAppearance() const { return Appearance; }

	UFUNCTION(BlueprintCallable, Category = "Palworld")
	void SelectOption(EUEPalworldCustomizationCategory Category, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Palworld")
	int32 GetOptionCount(EUEPalworldCustomizationCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "Palworld")
	FString GetOptionLabel(EUEPalworldCustomizationCategory Category, int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Palworld")
	UUEPalworldCustomizationCatalog* GetCatalog() const { return Catalog; }

	UFUNCTION(BlueprintCallable, Category = "Palworld")
	void SelectGender(EUEPalworldGender NewGender);

	UFUNCTION(BlueprintCallable, Category = "Palworld")
	void SetColor(EUEPalworldColorChannel Channel, const FLinearColor& Color);

	UFUNCTION(BlueprintCallable, Category = "Palworld")
	void SetScaleValue(EUEPalworldScaleChannel Channel, float Value);

	UFUNCTION(BlueprintCallable, Category = "Palworld|Preview")
	void AddPreviewYaw(float DeltaYaw);

	UFUNCTION(BlueprintCallable, Category = "Palworld|Preview")
	void AddPreviewZoom(float DeltaZoom);

	UFUNCTION(BlueprintCallable, Category = "Palworld|Preview")
	void AddPreviewPan(FVector2D DeltaPixels);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USceneComponent> CharacterRoot = nullptr;

	// Palworld 원본 의상 메시와 원본 머티리얼을 그대로 보여준다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USkeletalMeshComponent> BaseBodyMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USkeletalMeshComponent> BodyEquipmentMesh = nullptr;

	// 얼굴 프리셋 메시다. 몸과 같은 루트 아래에서 같이 움직인다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USkeletalMeshComponent> HeadMesh = nullptr;

	// 머리카락 프리셋 메시다. 머리만 따로 뜨지 않게 공통 루트에 둔다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USkeletalMeshComponent> HairMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<UCameraComponent> PreviewCamera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USpotLightComponent> KeyLight = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<UDirectionalLightComponent> PreviewDirectionalLight = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USkyLightComponent> FillLight = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<UPointLightComponent> FrontLight = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<UPointLightComponent> BodyFillLight = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld")
	TObjectPtr<UUEPalworldCustomizationCatalog> Catalog = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld")
	FUEPalworldAppearance Appearance;

	UPROPERTY(EditAnywhere, Category = "Palworld|Preview")
	float PreviewYawDegrees = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Palworld|Preview")
	float PreviewZoom = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Palworld|Preview")
	FVector2D PreviewPanPixels = FVector2D::ZeroVector;

private:
	void ApplyQACommandLineAppearance();
	void CaptureQAWidgetScreenshot();
	void CaptureQAScreenshot();
	void PrepareQAHeadScreenshot();
	void CaptureQAHeadScreenshot();
	void ExitAfterQAScreenshot();
	void ConfigurePreviewLighting();
	void FramePreviewCamera();
	void PreparePreviewStage();
	void NormalizeLegacyDefaultColors();
	void RefreshMeshes();
	void RefreshFollowerPose();
	void ApplyMaterialColors();
	void ApplyEyeMaterial(const FUEPalworldCustomizationOption& Option);
	bool IsEyeIrisMaterialSlot(USkeletalMeshComponent* Component, int32 MaterialIndex) const;
	void ApplyColorToSlots(USkeletalMeshComponent* Component, const FLinearColor& Color, const TArray<FString>& SlotContains);
	void ResetComponentMaterials(USkeletalMeshComponent* Component);
	void ApplyMorphSafeMaterials(USkeletalMeshComponent* Component);
	void HideFaceCoverSections(USkeletalMeshComponent* Component);
	void HideBaseBodyOutfitSections(USkeletalMeshComponent* Component);
	void HideUnsupportedAttachmentComponents();
	void ApplyScale();
	const FUEPalworldCustomizationOption& GetOption(EUEPalworldCustomizationCategory Category, int32 Index) const;
	int32& MutableIndex(EUEPalworldCustomizationCategory Category);
	int32 GetIndex(EUEPalworldCustomizationCategory Category) const;
	static int32 ClampIndex(int32 Index, int32 Count);

	FTimerHandle QAScreenshotTimer;
	FTimerHandle QAWidgetScreenshotTimer;
	FTimerHandle QAPrepareHeadTimer;
	FTimerHandle QAHeadScreenshotTimer;
	FTimerHandle QAExitTimer;
};
