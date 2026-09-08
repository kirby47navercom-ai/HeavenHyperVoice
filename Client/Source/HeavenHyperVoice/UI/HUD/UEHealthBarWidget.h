#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UEHealthBarWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * HP 값과 WBP 표시 요소만 연결하는 기본 위젯이다.
 * 배치, 크기, 색상과 애니메이션은 이 클래스를 상속한 WBP에서 편집한다.
 */
UCLASS(Abstract, Blueprintable)
class HEAVENHYPERVOICE_API UUEHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UUEHealthBarWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "UI|Health")
	void SetHealth(float InCurrentHealth, float InMaxHealth);

	UFUNCTION(BlueprintCallable, Category = "UI|Health")
	void SetDisplayName(const FText& InDisplayName);

	UFUNCTION(BlueprintPure, Category = "UI|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "UI|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "UI|Health")
	float GetHealthPercent() const;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PokemonNameText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Health|Text")
	FText HealthTextFormat;

	// 서버 체력은 즉시 확정하되, 감소 표시만 이 시간 동안 부드럽게 따라간다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Health", meta = (ClampMin = "0.0"))
	float HealthDecreaseDuration = 0.35f;

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Health", meta = (DisplayName = "On Health Changed"))
	void BP_OnHealthChanged(float NewCurrentHealth, float NewMaxHealth, float NewHealthPercent);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Health", meta = (DisplayName = "On Display Name Changed"))
	void BP_OnDisplayNameChanged(const FText& NewDisplayName);

private:
	void RefreshBoundWidgets();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|Health", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 100.0f;

	UPROPERTY(Transient)
	float DisplayedHealth = 100.0f;

	UPROPERTY(Transient)
	float HealthDecreaseStart = 100.0f;

	UPROPERTY(Transient)
	float HealthDecreaseElapsed = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|Health", meta = (AllowPrivateAccess = "true"))
	float MaxHealth = 100.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|Health", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	bool bHealthInitialized = false;
	bool bAnimatingHealthDecrease = false;
};
