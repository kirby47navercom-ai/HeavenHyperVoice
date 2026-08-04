// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UEAccountSubsystem.generated.h"

class UUEAccountSaveGame;

/** Every result the UI may need to explain to the player. */
UENUM(BlueprintType)
enum class EUELocalAccountResult : uint8
{
	Success,
	EmptyNickname,
	InvalidNicknameLength,
	EmptyUserId,
	InvalidUserIdLength,
	InvalidUserIdCharacters,
	InvalidPasswordLength,
	PasswordConfirmationMismatch,
	AccountAlreadyExists,
	AccountNotFound,
	IncorrectPassword,
	SaveFailed
};

/**
 * Owns client-local account creation and credential verification.
 *
 * Keeping this logic outside the widget makes the validation reusable from
 * Blueprint and gives the UI only one source of truth for account data.
 */
UCLASS()
class HEAVENHYPERVOICE_API UUEAccountSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Returns Success only when the ID is valid and currently unused. */
	UFUNCTION(BlueprintCallable, Category = "Account")
	EUELocalAccountResult CheckUserIdAvailability(const FString& UserId);

	/** Creates a new local account after validating every registration field. */
	UFUNCTION(BlueprintCallable, Category = "Account")
	EUELocalAccountResult RegisterAccount(
		const FString& Nickname,
		const FString& UserId,
		const FString& Password,
		const FString& PasswordConfirmation);

	/** Returns Success only when both the ID and password match one stored account. */
	UFUNCTION(BlueprintCallable, Category = "Account")
	EUELocalAccountResult LoginAccount(const FString& UserId, const FString& Password, FString& OutNickname);

	UFUNCTION(BlueprintPure, Category = "Account")
	bool IsUserIdRegistered(const FString& UserId) const;

	// These limits are public constants so UI and future server code can mirror the same rules.
	static constexpr int32 MinNicknameLength = 2;
	static constexpr int32 MaxNicknameLength = 16;
	static constexpr int32 MinUserIdLength = 4;
	static constexpr int32 MaxUserIdLength = 20;
	static constexpr int32 MinPasswordLength = 6;

private:
	void LoadAccounts();
	bool SaveAccounts() const;

	static FString NormalizeUserId(const FString& UserId);
	static bool HasValidUserIdCharacters(const FString& UserId);
	static FString HashPassword(const FString& Password, const FString& Salt);

private:
	UPROPERTY(Transient)
	TObjectPtr<UUEAccountSaveGame> AccountSaveGame = nullptr;

	static const FString SaveSlotName;
	static constexpr int32 SaveUserIndex = 0;
};
