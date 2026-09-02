#pragma once

// 필드에서 파티를 고치는 화면.
//
// 로비가 아니라 필드에만 있는 이유는 연결 때문이다. 로그인 연결은 캐릭터 선택
// 응답 직후 서버가 닫으므로 로비에서는 말할 상대가 없다. 필드 연결은 접속 내내
// 열려 있고, 파티를 바꾸면 그 자리에서 파트너가 바뀌는 것도 보인다.
//
// 목록은 하나다. 카탈로그의 모든 종족을 도감번호 순으로 늘어놓고, 해금하지 않은
// 것은 회색으로 눌리지 않게 둔다. 파티에 넣은 것은 노란 테두리와 모서리 번호로
// 표시한다 — 파티만 따로 떼어 놓으면 같은 포켓몬이 화면에 두 번 나온다.
//
// WBP 없이도 뜬다. 자식 위젯이 하나도 없으면 RebuildWidget 이 기본 배치를
// 만든다 — DefaultGame.ini 에 이 C++ 클래스를 그대로 지정해도 동작한다.
// 나중에 WBP 를 만들어 아래 BindWidgetOptional 이름들을 맞춰 두면 그쪽이 쓰인다.

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "UEFieldPartyWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UPanelWidget;
class UTextBlock;
class UWidget;
class UUEFieldPartyWidget;
class UUEFieldServerBridgeComponent;
class UUEPokemonSpeciesCatalog;
class UUEPokemonSpeciesData;

/** 목록 한 칸의 데이터. */
UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUEFieldPartyEntryData : public UObject
{
	GENERATED_BODY()

public:
	// 도감번호. 항목을 가리키는 유일한 열쇠다 — 배열 위치를 쓰면 카탈로그에
	// 종족을 끼워 넣는 순간 다른 포켓몬을 고르게 된다.
	UPROPERTY(BlueprintReadOnly, Category = "Field Party")
	int32 DexNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Field Party")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Field Party")
	TObjectPtr<UUEPokemonSpeciesData> Species = nullptr;

	// 해금하지 않았다. 회색으로 보이고 눌리지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Field Party")
	bool bLocked = false;

	// 파티에서의 자리. 1, 2, 3 이고 0 이면 파티에 없다. 모서리에 이 번호가 뜬다.
	UPROPERTY(BlueprintReadOnly, Category = "Field Party")
	int32 PartySlot = 0;

	// 지금 꺼내 놓은 한 마리다.
	UPROPERTY(BlueprintReadOnly, Category = "Field Party")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Field Party")
	TObjectPtr<UUEFieldPartyWidget> Owner = nullptr;
};

/**
 * 목록 한 칸.
 *
 * 누르면 파티에 넣고, 다시 누르면 뺀다. 어느 것을 꺼낼지는 1/2/3 키로 정한다.
 */
UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUEFieldPartyEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 목록을 다시 그릴 때 부모가 부른다. */
	void Setup(UUEFieldPartyEntryData* InEntryData);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SelectButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText = nullptr;

	// 종족 데이터의 ProfileIcon. 없는 종족은 이름만 뜬다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage = nullptr;

	// 파티에 들어 있으면 노랗게, 꺼내 놓았으면 더 밝게 칠한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SelectionBorder = nullptr;

	// 모서리의 1/2/3. 파티에 없으면 접힌다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SlotBadge = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SlotBadgeText = nullptr;

private:
	UFUNCTION()
	void HandleClicked();

	void ApplyEntryData();

	UPROPERTY(Transient)
	TObjectPtr<UUEFieldPartyEntryData> EntryData = nullptr;
};

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API UUEFieldPartyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 한 칸을 눌렀을 때. 파티에 없으면 넣고, 있으면 뺀다. */
	UFUNCTION(BlueprintCallable, Category = "Field Party")
	void ToggleMember(int32 DexNumber);

	/**
	 * 파티 몇 번째를 꺼낼지 고른다. SlotNumber 는 화면의 배지와 같은 1, 2, 3 이다.
	 *
	 * 이미 그 포켓몬이 나와 있으면 도로 집어넣는다 — 같은 키가 꺼내기와
	 * 집어넣기를 겸한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Field Party")
	void SetActiveSlot(int32 SlotNumber);

	/** 서버에 보낸다. 응답이 오면 화면이 서버 상태로 다시 그려진다. */
	UFUNCTION(BlueprintCallable, Category = "Field Party")
	void Confirm();

	UFUNCTION(BlueprintCallable, Category = "Field Party")
	void Close();

	// 아직 서버에 보내지 않은 편집 중인 구성이다. 도감번호.
	UFUNCTION(BlueprintPure, Category = "Field Party")
	const TArray<int32>& GetPendingParty() const { return PendingParty; }

	UFUNCTION(BlueprintPure, Category = "Field Party")
	int32 GetPendingActive() const { return PendingActive; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

	// 모든 종족이 도감번호 순으로 들어간다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> PokemonList = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ConfirmButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText = nullptr;

	// 화면에 늘어놓을 종족 표. 도감번호를 이름·초상화로 바꾸는 데도 쓴다.
	// 비어 있으면 ini 의 SpeciesCatalog 를 빌린다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Party")
	TObjectPtr<UUEPokemonSpeciesCatalog> SpeciesCatalog = nullptr;

	// 파티 상한. 서버(kMaxPartySize)와 DB(ck_party_slot)가 같은 값을 강제한다.
	// 여기서 막는 것은 화면 편의일 뿐이고 거절은 서버가 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Party", meta = (ClampMin = "1"))
	int32 MaxPartySize = 3;

private:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCloseClicked();

	// 서버가 상태를 보내면 편집 중인 내용을 버리고 그것으로 되돌린다.
	// 거절당한 변경이 화면에만 남지 않게 한다.
	UFUNCTION()
	void HandlePartyStateChanged();

	void RebuildList();
	void SetStatus(const FText& Message);
	UUEFieldServerBridgeComponent* FindBridge() const;
	UUEPokemonSpeciesCatalog* ResolveCatalog() const;

	UPROPERTY(Transient)
	TArray<int32> PendingParty;

	UPROPERTY(Transient)
	int32 PendingActive = 0;
};
