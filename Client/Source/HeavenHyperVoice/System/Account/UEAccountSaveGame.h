#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UEAccountSaveGame.generated.h"

/**
 * 서버 연동 전 로컬 로그인에서 사용하는 계정 데이터다.
 * 비밀번호 원문은 저장하지 않고 계정별 솔트와 해시만 저장한다.
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

/** UUEAccountSubsystem 전용 로컬 저장 데이터다. */
UCLASS()
class HEAVENHYPERVOICE_API UUEAccountSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// 소문자로 정규화한 아이디를 키로 사용해 대소문자와 무관하게 중복을 검사한다.
	UPROPERTY(SaveGame)
	TMap<FString, FUEStoredLocalAccount> Accounts;
};
