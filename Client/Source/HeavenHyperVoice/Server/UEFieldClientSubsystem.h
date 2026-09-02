#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UEFieldServerBridgeComponent.h"
#include "UEFieldClientSubsystem.generated.h"

class AUEPlayerCharacter;
class AUEPlayerController;

/**
 * 필드/인스턴스 접속의 주인.
 *
 * 레벨 이동을 넘어 살아남아야 해서 LocalPlayerSubsystem 이다. 브릿지 컴포넌트는
 * PlayerController 에 붙어 있어 레벨이 갈리면 함께 사라지므로, "다음에 어디로
 * 붙을 것인가" 를 기억하는 것은 여기여야 한다.
 */
UCLASS(BlueprintType, Config = Game, DefaultConfig)
class HEAVENHYPERVOICE_API UUEFieldClientSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UUEFieldClientSubsystem* Get(const UObject* WorldContextObject);

	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Field Server")
	void RegisterPlayerController(AUEPlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Field Server")
	void AttachPlayerCharacter(AUEPlayerCharacter* PlayerCharacter);

	UFUNCTION(BlueprintPure, Category = "Field Server")
	UUEFieldServerBridgeComponent* GetFieldServerBridge() const { return FieldServerBridgeComponent; }

	/**
	 * 인스턴스로 들어간다. 레벨을 인스턴스 맵으로 갈아타고, 새 레벨에서 붙는
	 * 서버도 인스턴스로 바뀐다.
	 *
	 * 레벨 이동이 브릿지 컴포넌트를 통째로 없애므로 필드 연결은 그 과정에서
	 * 저절로 정리된다. 새 레벨에서 만들어진 브릿지가 아래 PendingInstanceType
	 * 을 보고 어디로 붙을지 정한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Field Server|Instance")
	void EnterInstance(int32 InstanceType);

	/** 인스턴스에서 나와 필드 맵과 필드 서버로 돌아간다. */
	UFUNCTION(BlueprintCallable, Category = "Field Server|Instance")
	void LeaveInstance();

	/** 인스턴스에 있거나 인스턴스로 가는 중이다. */
	UFUNCTION(BlueprintPure, Category = "Field Server|Instance")
	bool IsInInstance() const { return PendingInstanceType != 0; }

	UFUNCTION(BlueprintCallable, Category = "Field Server|Pokemon")
	bool SendPokemonToggleRequest();

	UFUNCTION(BlueprintCallable, Category = "Field Server|Pokemon")
	bool SendPokemonAttackRequest(int32 AttackSlot);

	UFUNCTION(BlueprintPure, Category = "Field Server|Pokemon")
	TArray<FUEFieldPokemonPartyEntry> GetPokemonPartyEntries() const;

	const TArray<FUEFieldPokemonPartyEntry>& GetCachedPokemonPartyEntries() const;

	UPROPERTY(BlueprintAssignable, Category = "Field Server|Pokemon")
	FUEFieldPokemonPartyChangedSignature OnPokemonPartyChanged;

protected:
	// 돌아갈 필드 레벨. WBP_CharacterSelection 이 캐릭터 선택 뒤에 여는 것과
	// 같은 레벨이어야 한다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Field Server")
	TSoftObjectPtr<UWorld> FieldLevel;

	// 인스턴스 맵. 지금은 종류가 하나뿐이라 전부 이 레벨을 쓴다.
	// ponytail: 종류마다 맵이 달라지면 TMap<int32, TSoftObjectPtr<UWorld>> 로.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Field Server|Instance")
	TSoftObjectPtr<UWorld> InstanceLevel;

private:
	UFUNCTION()
	void HandleBridgePokemonPartyChanged();

	UUEFieldServerBridgeComponent* EnsureFieldServerBridge();

	// 지금 레벨의 게임 모드를 그대로 물려 이동한다. 여기서 클래스를 고르면
	// 캐릭터 선택 쪽 설정과 조용히 어긋난다.
	void TravelTo(const TSoftObjectPtr<UWorld>& Level);

	// 0 이면 필드. 레벨 이동을 넘어 살아남아야 해서 브릿지가 아니라 여기 있다.
	int32 PendingInstanceType = 0;

	TWeakObjectPtr<AUEPlayerController> CachedPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UUEFieldServerBridgeComponent> FieldServerBridgeComponent = nullptr;
};
