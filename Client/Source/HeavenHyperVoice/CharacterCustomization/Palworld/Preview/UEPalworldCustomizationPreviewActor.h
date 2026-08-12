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

	// 팔월드 추출본은 별도 알몸 베이스 바디가 없어서 의상 메쉬가 모듈러 파츠의 애니메이션 기준이 된다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USkeletalMeshComponent> BodyEquipmentMesh = nullptr;

	// 팔월드 캐릭터 생성 테이블에서 선택한 얼굴/머리 메쉬.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USkeletalMeshComponent> HeadMesh = nullptr;

	// 팔월드 캐릭터 생성 테이블에서 선택한 헤어 메쉬.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Components")
	TObjectPtr<USkeletalMeshComponent> HairMesh = nullptr;

	// 머리 장비용 예비 컴포넌트. 현재 캐릭터 생성 화면에서는 얼굴을 가리지 않도록 항상 비활성화한다.
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
	void CaptureQAWidgetScreenshot();
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
	FTimerHandle QAWidgetScreenshotTimer;
	FTimerHandle QAPrepareHeadTimer;
	FTimerHandle QAHeadScreenshotTimer;
	FTimerHandle QAExitTimer;
	bool bHeadEquipmentAttachedToSocket = false;
};
