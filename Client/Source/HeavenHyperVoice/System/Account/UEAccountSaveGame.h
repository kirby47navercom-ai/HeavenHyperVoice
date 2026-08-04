// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UEAccountSaveGame.generated.h"

/**
 * One account stored by the client-only login prototype.
 *
 * Passwords are never written as plain text. Only a per-account salt and the
 * resulting hash are persisted. A real online game must replace this local
 * store with server-side authentication before release.
 */
USTRUCT(BlueprintType)
struct HEAVENHYPERVOICE_API FUEStoredLocalAccount
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Account")
	FString Nickname;

	UPROPERTY(SaveGame)
	FString PasswordSalt;

	UPROPERTY(SaveGame)
	FString PasswordHash;
};

/** SaveGame payload used only by UUEAccountSubsystem. */
UCLASS()
class HEAVENHYPERVOICE_API UUEAccountSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// The key is a normalized, lower-case user ID so duplicate checks are case-insensitive.
	UPROPERTY(SaveGame)
	TMap<FString, FUEStoredLocalAccount> Accounts;
};
