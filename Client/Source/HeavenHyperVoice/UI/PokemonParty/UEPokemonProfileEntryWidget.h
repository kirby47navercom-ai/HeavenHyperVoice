#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UEPokemonProfileEntryWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UWidget;

/** 보유 포켓몬 한 마리를 프로필 슬롯에 전달하기 위한 UI 전용 데이터다. */
USTRUCT(BlueprintType)
struct FUEPokemonProfileViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Profile")
	int32 PokemonInstanceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Profile")
	FName SpeciesId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Profile")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Profile")
	TObjectPtr<UTexture2D> ProfileIcon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Profile", meta = (ClampMin = "1"))
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Profile", meta = (ClampMin = "0.0"))
	float CurrentHP = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Profile", meta = (ClampMin = "1.0"))
	float MaxHP = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Profile")
	bool bSelected = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Profile")
	bool bCanSummon = true;
};

/**
 * 포켓몬 프로필 한 칸의 데이터 연결을 담당한다.
 * 색, 크기, 폰트와 배치는 자식 WBP_PokemonProfileEntry의 디자이너에서 수정한다.
 */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUEPokemonProfileEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UUEPokemonProfileEntryWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Profile")
	void SetProfileData(const FUEPokemonProfileViewData& NewProfileData);

	UFUNCTION(BlueprintPure, Category = "Pokemon|Profile")
	const FUEPokemonProfileViewData& GetProfileData() const { return ProfileData; }

protected:
	virtual void NativePreConstruct() override;

	// WBP에서 추가 연출이 필요할 때 C++ 수정 없이 이 이벤트에 연결한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Pokemon|Profile", meta = (DisplayName = "On Profile Data Changed"))
	void BP_OnProfileDataChanged(const FUEPokemonProfileViewData& NewProfileData);

	// 표시 형식도 코드에 고정하지 않고 WBP 클래스 기본값에서 바꿀 수 있게 둔다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Profile|Text")
	FText LevelFormat;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Profile|Text")
	FText HPFormat;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Profile|Designer")
	bool bShowDesignerPreview = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Profile|Designer")
	FUEPokemonProfileViewData DesignerPreviewData;

	// 아래 위젯은 모두 선택 사항이다. WBP에서 제거해도 Blueprint 이벤트 방식으로 다시 그릴 수 있다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PokemonIcon = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PokemonFallbackText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PokemonNameText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HPBar = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HPText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedIndicator = nullptr;

private:
	void ApplyProfileToBoundWidgets();

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Pokemon|Profile", meta = (AllowPrivateAccess = "true"))
	FUEPokemonProfileViewData ProfileData;
};
