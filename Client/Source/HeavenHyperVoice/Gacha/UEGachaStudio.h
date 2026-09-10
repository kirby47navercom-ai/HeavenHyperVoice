#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "UEGachaStudio.generated.h"

class AUEGachaMachine;
class UButton;
class UTextBlock;
class UImage;
class UProgressBar;
class UBorder;

UCLASS(Abstract, Blueprintable)
class HEAVENHYPERVOICE_API UUEGachaStudioWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float Seconds) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> FireButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> WaterButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> GrassButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> NormalButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ElectricButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> AgainButton;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> MachineTitle;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> PoolText;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TurnsText;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ResultName;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ResultRarity;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> ResultIcon;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UProgressBar> TurnProgress;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> ResultPanel;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> StageOne;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> StageTwo;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> StageThree;
private:
	void Select(int32 Index);
	UFUNCTION() void Fire();
	UFUNCTION() void Water();
	UFUNCTION() void Grass();
	UFUNCTION() void Normal();
	UFUNCTION() void Electric();
	UFUNCTION() void Again();
	TWeakObjectPtr<AUEGachaMachine> LastMachine;
};

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEGachaStudioController : public APlayerController
{
	GENERATED_BODY()
public:
	AUEGachaStudioController();
	virtual void Tick(float Seconds) override;
	void SelectMachine(int32 Index);
	AUEGachaMachine* GetMachine() const { return CurrentMachine; }
	UPROPERTY(EditDefaultsOnly, Category="Gacha") TSubclassOf<UUEGachaStudioWidget> StudioWidgetClass;
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
private:
	void GrabHandle();
	void ReleaseHandle();
	bool GetCrankAngle(float& Angle) const;
	UPROPERTY(Transient) TObjectPtr<UUEGachaStudioWidget> StudioWidget;
	UPROPERTY(Transient) TArray<TObjectPtr<AUEGachaMachine>> Machines;
	UPROPERTY(Transient) TObjectPtr<AUEGachaMachine> CurrentMachine;
	bool bDragging = false;
	float PreviousAngle = 0;
	float CameraReadyTime = 0;
};

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEGachaStudioGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AUEGachaStudioGameMode();
};
