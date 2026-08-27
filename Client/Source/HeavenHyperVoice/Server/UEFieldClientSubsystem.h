#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UEFieldServerBridgeComponent.h"
#include "UEFieldClientSubsystem.generated.h"

class AUEPlayerCharacter;
class AUEPlayerController;

UCLASS(BlueprintType)
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

	UFUNCTION(BlueprintCallable, Category = "Field Server|Pokemon")
	bool SendPokemonToggleRequest();

	UFUNCTION(BlueprintCallable, Category = "Field Server|Pokemon")
	bool SendPokemonAttackRequest(int32 AttackSlot);

	UFUNCTION(BlueprintPure, Category = "Field Server|Pokemon")
	TArray<FUEFieldPokemonPartyEntry> GetPokemonPartyEntries() const;

	const TArray<FUEFieldPokemonPartyEntry>& GetCachedPokemonPartyEntries() const;

	UPROPERTY(BlueprintAssignable, Category = "Field Server|Pokemon")
	FUEFieldPokemonPartyChangedSignature OnPokemonPartyChanged;

private:
	UFUNCTION()
	void HandleBridgePokemonPartyChanged();

	UUEFieldServerBridgeComponent* EnsureFieldServerBridge();

	TWeakObjectPtr<AUEPlayerController> CachedPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UUEFieldServerBridgeComponent> FieldServerBridgeComponent = nullptr;
};
