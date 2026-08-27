#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UEPokemonProfileEntryWidget.h"
#include "UEPokemonPartyWidget.generated.h"

class AUEPlayerCharacter;
class UImage;
class UPanelWidget;
class UProgressBar;
class UTextBlock;
class UUEFieldClientSubsystem;
class UWidget;

/**
 * 로컬 플레이어의 보유 포켓몬을 프로필 위젯으로 변환한다.
 * 화면 모양은 자식 WBP_PokemonParty가 소유하고 이 클래스는 데이터만 공급한다.
 */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUEPokemonPartyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UUEPokemonPartyWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Party")
	void InitializeForPlayer(AUEPlayerCharacter* PlayerCharacter);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Party")
	void RefreshProfiles();

	UFUNCTION(BlueprintPure, Category = "Pokemon|Party")
	const TArray<FUEPokemonProfileViewData>& GetProfiles() const { return Profiles; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Pokemon|Party", meta = (DisplayName = "On Party Profiles Rebuilt"))
	void BP_OnPartyProfilesRebuilt(const TArray<FUEPokemonProfileViewData>& NewProfiles);

	// 슬롯 WBP도 변수로 받아 런타임 에셋 경로를 코드에 쓰지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Party")
	TSubclassOf<UUEPokemonProfileEntryWidget> ProfileEntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Party|Text")
	FText UnknownPokemonText;

	// 대표 프로필의 레벨 표기도 WBP 클래스 기본값에서 한글 문구를 바꿀 수 있다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Party|Text")
	FText ActiveLevelFormat;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Party|Text")
	FText ActiveHPFormat;

	// Z-A처럼 큰 대표 프로필 하나와 작은 파티 슬롯을 분리한다. 모든 위젯은 WBP에서 교체 가능하다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ActiveProfilePanel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ActivePokemonIcon = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActivePokemonFallbackText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActivePokemonNameText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActivePokemonLevelText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ActivePokemonHPBar = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActivePokemonHPText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ProfileList = nullptr;

private:
	UFUNCTION()
	void HandlePokemonPartyChanged();

	void BindRosterDelegate();
	void ApplyActiveProfile(const FUEPokemonProfileViewData* ActiveProfile);
	UUEFieldClientSubsystem* GetFieldClientSubsystem() const;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Party", meta = (AllowPrivateAccess = "true"))
	TArray<FUEPokemonProfileViewData> Profiles;

	TWeakObjectPtr<AUEPlayerCharacter> SourcePlayer;
};
