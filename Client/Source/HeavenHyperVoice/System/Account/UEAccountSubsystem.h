#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UEAccountSubsystem.generated.h"

class UUEAccountSaveGame;

/** UI가 사용자에게 안내할 수 있는 로컬 계정 처리 결과다. */
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

/** 로컬 계정 생성과 인증을 UI에서 분리해 한 곳에서 관리하는 서브시스템이다. */
UCLASS(Config = Game, DefaultConfig)
class HEAVENHYPERVOICE_API UUEAccountSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 아이디 형식이 올바르고 아직 사용되지 않았을 때만 Success를 반환한다. */
	UFUNCTION(BlueprintCallable, Category = "Account")
	EUELocalAccountResult CheckUserIdAvailability(const FString& UserId);

	/** 회원가입 입력을 모두 검증한 뒤 로컬 계정을 만든다. */
	UFUNCTION(BlueprintCallable, Category = "Account")
	EUELocalAccountResult RegisterAccount(
		const FString& Nickname,
		const FString& UserId,
		const FString& Password,
		const FString& PasswordConfirmation);

	/** 저장된 계정의 아이디와 비밀번호가 모두 맞을 때만 Success를 반환한다. */
	UFUNCTION(BlueprintCallable, Category = "Account")
	EUELocalAccountResult LoginAccount(const FString& UserId, const FString& Password, FString& OutNickname);

	UFUNCTION(BlueprintPure, Category = "Account")
	bool IsUserIdRegistered(const FString& UserId) const;

	// UI와 이후 서버 구현이 같은 규칙을 공유할 수 있도록 검증 범위를 한 곳에 둔다.
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

	UPROPERTY(Config)
	FString SaveSlotName;

	UPROPERTY(Config)
	int32 SaveUserIndex = 0;
};
