// Fill out your copyright notice in the Description page of Project Settings.

#include "UEAccountSubsystem.h"

#include "UEAccountSaveGame.h"

#include "Hash/Blake3.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"
#include "Subsystems/SubsystemCollection.h"

void UUEAccountSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadAccounts();
}

EUELocalAccountResult UUEAccountSubsystem::CheckUserIdAvailability(const FString& UserId)
{
	if (!AccountSaveGame)
	{
		LoadAccounts();
	}

	const FString TrimmedUserId = UserId.TrimStartAndEnd();
	if (TrimmedUserId.IsEmpty())
	{
		return EUELocalAccountResult::EmptyUserId;
	}

	if (TrimmedUserId.Len() < MinUserIdLength || TrimmedUserId.Len() > MaxUserIdLength)
	{
		return EUELocalAccountResult::InvalidUserIdLength;
	}

	if (!HasValidUserIdCharacters(TrimmedUserId))
	{
		return EUELocalAccountResult::InvalidUserIdCharacters;
	}

	return IsUserIdRegistered(TrimmedUserId)
		? EUELocalAccountResult::AccountAlreadyExists
		: EUELocalAccountResult::Success;
}

EUELocalAccountResult UUEAccountSubsystem::RegisterAccount(
	const FString& Nickname,
	const FString& UserId,
	const FString& Password,
	const FString& PasswordConfirmation)
{
	const FString TrimmedNickname = Nickname.TrimStartAndEnd();
	if (TrimmedNickname.IsEmpty())
	{
		return EUELocalAccountResult::EmptyNickname;
	}

	if (TrimmedNickname.Len() < MinNicknameLength || TrimmedNickname.Len() > MaxNicknameLength)
	{
		return EUELocalAccountResult::InvalidNicknameLength;
	}

	const EUELocalAccountResult AvailabilityResult = CheckUserIdAvailability(UserId);
	if (AvailabilityResult != EUELocalAccountResult::Success)
	{
		// UI에서 확인한 뒤 값이 달라질 수 있으므로 실제 생성 시점에 다시 검사한다.
		return AvailabilityResult;
	}

	if (Password.Len() < MinPasswordLength)
	{
		return EUELocalAccountResult::InvalidPasswordLength;
	}

	if (Password != PasswordConfirmation)
	{
		return EUELocalAccountResult::PasswordConfirmationMismatch;
	}

	if (!AccountSaveGame)
	{
		return EUELocalAccountResult::SaveFailed;
	}

	const FString NormalizedUserId = NormalizeUserId(UserId);
	FUEStoredLocalAccount NewAccount;
	NewAccount.Nickname = TrimmedNickname;
	NewAccount.PasswordSalt = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	NewAccount.PasswordHash = HashPassword(Password, NewAccount.PasswordSalt);

	AccountSaveGame->Accounts.Add(NormalizedUserId, MoveTemp(NewAccount));
	if (!SaveAccounts())
	{
		// 저장에 실패한 계정은 메모리에도 남기지 않는다.
		AccountSaveGame->Accounts.Remove(NormalizedUserId);
		return EUELocalAccountResult::SaveFailed;
	}

	return EUELocalAccountResult::Success;
}

EUELocalAccountResult UUEAccountSubsystem::LoginAccount(const FString& UserId, const FString& Password, FString& OutNickname)
{
	OutNickname.Reset();

	if (!AccountSaveGame)
	{
		LoadAccounts();
	}

	if (!AccountSaveGame)
	{
		return EUELocalAccountResult::AccountNotFound;
	}

	const FUEStoredLocalAccount* Account = AccountSaveGame->Accounts.Find(NormalizeUserId(UserId));
	if (!Account)
	{
		return EUELocalAccountResult::AccountNotFound;
	}

	if (HashPassword(Password, Account->PasswordSalt) != Account->PasswordHash)
	{
		return EUELocalAccountResult::IncorrectPassword;
	}

	OutNickname = Account->Nickname;
	return EUELocalAccountResult::Success;
}

bool UUEAccountSubsystem::IsUserIdRegistered(const FString& UserId) const
{
	return AccountSaveGame && AccountSaveGame->Accounts.Contains(NormalizeUserId(UserId));
}

void UUEAccountSubsystem::LoadAccounts()
{
	AccountSaveGame = Cast<UUEAccountSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));

	if (!AccountSaveGame)
	{
		AccountSaveGame = Cast<UUEAccountSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UUEAccountSaveGame::StaticClass()));
	}
}

bool UUEAccountSubsystem::SaveAccounts() const
{
	return AccountSaveGame
		&& UGameplayStatics::SaveGameToSlot(AccountSaveGame, SaveSlotName, SaveUserIndex);
}

FString UUEAccountSubsystem::NormalizeUserId(const FString& UserId)
{
	return UserId.TrimStartAndEnd().ToLower();
}

bool UUEAccountSubsystem::HasValidUserIdCharacters(const FString& UserId)
{
	for (const TCHAR Character : UserId)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
		{
			return false;
		}
	}

	return true;
}

FString UUEAccountSubsystem::HashPassword(const FString& Password, const FString& Salt)
{
	// 같은 비밀번호라도 저장 해시가 같아지지 않도록 계정별 솔트를 섞는다.
	const FString SaltedPassword = Salt + TEXT("\n") + Password;
	FTCHARToUTF8 Utf8Password(*SaltedPassword);
	const FBlake3Hash Hash = FBlake3::HashBuffer(Utf8Password.Get(), Utf8Password.Length());
	return LexToString(Hash);
}
