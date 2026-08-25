#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../../CharacterCustomization/HHV/Data/UEHHVCustomizationTypes.h"
#include "UECharacterLobbySlotWidget.generated.h"

class AUEPlayerCharacter;
class AUEPokemonCharacter;
class UButton;
class UTextBlock;
class UUEPokemonSpeciesData;
class UViewport;
class UWidget;

/** 로비 카드가 보여 줄 세 가지 상태다. 실제 배치는 UMG 블루프린트가 소유한다. */
UENUM(BlueprintType)
enum class EUECharacterLobbySlotState : uint8
{
	Occupied,
	Available,
	Locked
};

/** 서버 없이도 로비 화면을 구성할 수 있는 슬롯 표시 데이터다. */
USTRUCT(BlueprintType)
struct HEAVENHYPERVOICE_API FUECharacterLobbySlotViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby Slot")
	EUECharacterLobbySlotState State = EUECharacterLobbySlotState::Locked;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby Slot")
	FText CharacterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby Slot")
	FText PartnerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby Slot|Preview")
	FUEHHVAppearance Appearance;

	// 파트너 종족은 WBP 기본값에서 자유롭게 바꿀 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby Slot|Preview")
	TObjectPtr<UUEPokemonSpeciesData> PartnerSpecies = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUEOnLobbySlotActionRequested, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUEOnLobbySlotDeleteRequested, int32, SlotIndex);

/** 한 개의 로비 슬롯 카드에 데이터와 프리뷰를 연결하는 네이티브 기반 위젯이다. */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUECharacterLobbySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Lobby Slot")
	void InitializeSlot(int32 InSlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Lobby Slot")
	void ApplyViewData(const FUECharacterLobbySlotViewData& InViewData);

	UPROPERTY(BlueprintAssignable, Category = "Lobby Slot")
	FUEOnLobbySlotActionRequested OnActionRequested;

	UPROPERTY(BlueprintAssignable, Category = "Lobby Slot")
	FUEOnLobbySlotDeleteRequested OnDeleteRequested;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 상태별 패널의 모양과 배치는 WBP_CharacterLobbySlot에서 편집한다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> OccupiedPanel = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> AvailablePanel = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> LockedPanel = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> OccupiedActionButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> AvailableActionButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> DeleteButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> DeleteConfirmationPanel = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmDeleteButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CancelDeleteButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SlotNumberValueText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CharacterNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PartnerNameText = nullptr;

	// 3D 프리뷰 영역 자체는 블루프린트 디자이너에서 배치한다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UViewport> PreviewViewport = nullptr;

	// 프리뷰에 생성할 클래스와 모든 좌표는 WBP 클래스 기본값에서 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Slot|Preview")
	TSubclassOf<AUEPlayerCharacter> CharacterPreviewActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Slot|Preview")
	TSubclassOf<AUEPokemonCharacter> PartnerPreviewActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Slot|Preview")
	FTransform CharacterPreviewTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Slot|Preview")
	FTransform PartnerPreviewTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Slot|Preview")
	FVector PreviewCameraLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Slot|Preview")
	FRotator PreviewCameraRotation = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Slot|Preview")
	FLinearColor PreviewBackgroundColor = FLinearColor::Transparent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Slot|Preview", meta = (ClampMin = "0.0"))
	float PreviewLightIntensity = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Slot|Preview", meta = (ClampMin = "0.0"))
	float PreviewSkyIntensity = 0.60f;

	// 디자이너에서 카드 한 장만 열어도 완성 상태를 확인할 수 있게 하는 미리보기 값이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby Slot|Designer")
	FUECharacterLobbySlotViewData DesignerPreviewData;

private:
	UFUNCTION()
	void HandleActionClicked();

	UFUNCTION()
	void HandleDeleteClicked();

	UFUNCTION()
	void HandleConfirmDeleteClicked();

	UFUNCTION()
	void HandleCancelDeleteClicked();

	void RefreshPreview();
	void SetDeleteConfirmationVisible(bool bVisible);
	void DestroyPreviewActors();
	void SetIntegerText(UTextBlock* TextBlock, int32 Value) const;

	int32 SlotIndex = INDEX_NONE;
	FUECharacterLobbySlotViewData ViewData;
	TWeakObjectPtr<AActor> SpawnedCharacterPreview;
	TWeakObjectPtr<AActor> SpawnedPartnerPreview;
};
