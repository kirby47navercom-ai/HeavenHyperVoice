#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Data/UEPalworldCustomizationTypes.h"
#include "UEPalworldCustomizationPreviewActor.generated.h"

class UCameraComponent;
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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USceneComponent> CharacterRoot = nullptr;

	// The outfit mesh is the animation leader for the modular Palworld player parts.
	// The current extract does not include a separate naked base body skeletal mesh.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USkeletalMeshComponent> BodyEquipmentMesh = nullptr;

	// Face/head mesh selected from Palworld character creation tables.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USkeletalMeshComponent> HeadMesh = nullptr;

	// Hair mesh selected from Palworld character creation tables.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USkeletalMeshComponent> HairMesh = nullptr;

	// Hats, ears, glasses, and other head equipment. Socket names come from the Palworld table.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USkeletalMeshComponent> HeadEquipmentMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<UCameraComponent> PreviewCamera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USpotLightComponent> KeyLight = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USkyLightComponent> FillLight = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<UPointLightComponent> FrontLight = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palworld")
	TObjectPtr<UUEPalworldCustomizationCatalog> Catalog = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palworld")
	FUEPalworldAppearance Appearance;

private:
	void ApplyQACommandLineAppearance();
	void CaptureQAScreenshot();
	void PrepareQAHeadScreenshot();
	void CaptureQAHeadScreenshot();
	void ExitAfterQAScreenshot();
	void ConfigurePreviewLighting();
	void RefreshMeshes();
	void RefreshFollowerPose();
	void AttachHeadEquipment(const FUEPalworldCustomizationOption& Option);
	void ApplyMaterialColors();
	void ApplyEyeMaterial(const FUEPalworldCustomizationOption& Option);
	void ApplyColorToSlots(USkeletalMeshComponent* Component, const FLinearColor& Color, const TArray<FString>& SlotContains);
	void ApplyScale();
	void FitHeadEquipmentToHead(const FUEPalworldCustomizationOption& Option);
	void EnsureSocketFallbacks();
	const FUEPalworldCustomizationOption& GetOption(EUEPalworldCustomizationCategory Category, int32 Index) const;
	int32& MutableIndex(EUEPalworldCustomizationCategory Category);
	int32 GetIndex(EUEPalworldCustomizationCategory Category) const;
	static int32 ClampIndex(int32 Index, int32 Count);

	FTimerHandle QAScreenshotTimer;
	FTimerHandle QAPrepareHeadTimer;
	FTimerHandle QAHeadScreenshotTimer;
	FTimerHandle QAExitTimer;
	bool bHeadEquipmentAttachedToSocket = false;
};
