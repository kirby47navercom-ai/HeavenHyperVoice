#pragma once

// 필드에서 파티를 고치는 화면.
//
// 로비가 아니라 필드에만 있는 이유는 연결 때문이다. 로그인 연결은 캐릭터 선택
// 응답 직후 서버가 닫으므로 로비에서는 말할 상대가 없다. 필드 연결은 접속 내내
// 열려 있고, 파티를 바꾸면 그 자리에서 파트너가 바뀌는 것도 보인다.
//
// WBP 없이도 뜬다. 자식 위젯이 하나도 없으면 RebuildWidget 이 기본 배치를
// 만든다 — DefaultGame.ini 에 이 C++ 클래스를 그대로 지정해도 동작한다.
// 나중에 WBP 를 만들어 아래 BindWidgetOptional 이름들을 맞춰 두면 그쪽이 쓰인다.
//
// 목록에 TileView 를 쓰지 않는다. 후보가 구현된 종족 수(스무 남짓)뿐이라
// 가상화가 이득이 없고, 패널에 직접 붙이면 항목 클래스 배선이 사라진다.

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "UEFieldPartyWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UWidget;
class UUEFieldPartyWidget;
class UUEFieldServerBridgeComponent;
class UUEPokemonSpeciesCatalog;
class UUEPokemonSpeciesData;

/** 목록 한 칸의 데이터. 해금 목록과 파티 목록이 같은 타입을 쓴다. */
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

	// 해금 목록에서만 의미가 있다. 이미 파티에 들어간 종족이다.
	UPROPERTY(BlueprintReadOnly, Category = "Field Party")
	bool bInParty = false;

	// 파티 목록에서만 의미가 있다. 지금 꺼내 놓은 한 마리다.
	UPROPERTY(BlueprintReadOnly, Category = "Field Party")
	bool bActive = false;

	// 파티 목록에서의 칸 번호. 해금 목록이면 INDEX_NONE.
	UPROPERTY(BlueprintReadOnly, Category = "Field Party")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Field Party")
	TObjectPtr<UUEFieldPartyWidget> Owner = nullptr;
};

/**
 * 목록 한 칸.
 *
 * 누르면 무엇을 할지는 여기서 정한다 — 해금 목록이면 파티에 넣고 빼고,
 * 파티 목록이면 그 한 마리를 꺼낸다.
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

	// 파티에 들어 있거나(해금 목록) 꺼내 놓았을 때(파티 목록) 보인다.
	// 기본 배치에는 없다 — 버튼 색으로 대신한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedMarker = nullptr;

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
	/** 해금 목록에서 눌렀을 때. 들어 있으면 빼고, 없으면 넣는다. */
	UFUNCTION(BlueprintCallable, Category = "Field Party")
	void ToggleMember(int32 DexNumber);

	/** 파티 목록에서 눌렀을 때. 그 한 마리를 꺼낸다. 이미 꺼낸 것이면 도로 넣는다. */
	UFUNCTION(BlueprintCallable, Category = "Field Party")
	void SetActiveMember(int32 DexNumber);

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

	// 해금한 종족 후보가 들어간다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> UnlockedList = nullptr;

	// 편집 중인 파티 세 칸.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> PartyList = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ConfirmButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText = nullptr;

	// 도감번호를 이름으로 바꾼다. AUEPokemonCharacter 가 쓰는 것과 같은 에셋이다.
	// 비어 있으면 ini 의 SpeciesCatalog 를 쓰고, 그것도 없으면 번호만 표시한다.
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

	void RebuildLists();
	void SetStatus(const FText& Message);
	UUEFieldServerBridgeComponent* FindBridge() const;
	UUEPokemonSpeciesCatalog* ResolveCatalog() const;

	UPROPERTY(Transient)
	TArray<int32> PendingParty;

	UPROPERTY(Transient)
	int32 PendingActive = 0;
};
