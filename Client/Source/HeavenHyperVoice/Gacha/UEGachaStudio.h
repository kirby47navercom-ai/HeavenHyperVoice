#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "UEGachaStudio.generated.h"

class AUEGachaMachine;
class UUEGachaDeskComponent;
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
	// 소유 컨트롤러의 뽑기 컴포넌트. 뽑기방과 필드가 같은 화면을 쓴다.
	UUEGachaDeskComponent* Desk() const;

	void Select(int32 Index);
	UFUNCTION() void Fire();
	UFUNCTION() void Water();
	UFUNCTION() void Grass();
	UFUNCTION() void Normal();
	UFUNCTION() void Electric();
	UFUNCTION() void Again();
	TWeakObjectPtr<AUEGachaMachine> LastMachine;
};

// 뽑기방 전용 컨트롤러. 조작은 전부 UUEGachaDeskComponent 에 있고, 여기서는
// 들어가는 순간 열어 주고 마우스 버튼만 넘겨준다 — 필드에서는 같은 컴포넌트를
// AUEPlayerController 가 O 키로 켰다 껐다 한다.
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEGachaStudioController : public APlayerController
{
	GENERATED_BODY()
public:
	AUEGachaStudioController();

	// BP_GachaStudioController 가 이미 WBP_GachaStudio 를 넣어 둔 자리다.
	// 컴포넌트로 그대로 넘긴다.
	UPROPERTY(EditDefaultsOnly, Category="Gacha") TSubclassOf<UUEGachaStudioWidget> StudioWidgetClass;
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
private:
	UPROPERTY(Transient) TObjectPtr<UUEGachaDeskComponent> Desk;
};

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEGachaStudioGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AUEGachaStudioGameMode();
};
