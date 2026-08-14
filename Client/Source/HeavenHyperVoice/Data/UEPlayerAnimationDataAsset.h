#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UEPlayerAnimationDataAsset.generated.h"

class UAnimMontage;
class UAnimSequence;

USTRUCT(BlueprintType)
struct FUEPlayerMontageEntry
{
	GENERATED_BODY()

public:
	// 코드와 블루프린트에서 몽타주를 찾을 때 사용할 태그다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	FGameplayTag MontageTag = FGameplayTag::EmptyTag;

	// 실제 재생할 애니메이션 몽타주 에셋이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> Montage = nullptr;
};

USTRUCT(BlueprintType)
struct FUEPlayerSequenceEntry
{
	GENERATED_BODY()

public:
	// 코드와 블루프린트에서 시퀀스를 찾을 때 사용할 태그다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	FGameplayTag SequenceTag = FGameplayTag::EmptyTag;

	// 실제 참조할 애니메이션 시퀀스 에셋이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> Sequence = nullptr;
};

UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUEPlayerAnimationDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// 기본 이동 상태용 시퀀스다. 애님 블루프린트에서 바로 꺼내 쓰기 좋게 명시 슬롯으로 둔다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Sequence")
	TObjectPtr<UAnimSequence> IdleSequence = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Sequence")
	TObjectPtr<UAnimSequence> WalkSequence = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Sequence")
	TObjectPtr<UAnimSequence> RunSequence = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Sequence")
	TObjectPtr<UAnimSequence> JumpSequence = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Sequence")
	TObjectPtr<UAnimSequence> FallSequence = nullptr;

	// 액션 재생용 몽타주다. 필요한 액션만 채워도 된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Montage")
	TObjectPtr<UAnimMontage> RollMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Montage")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Montage")
	TObjectPtr<UAnimMontage> HitMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Montage")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

	// 위 명시 슬롯으로 부족한 몽타주는 태그 배열에 추가한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Montage")
	TArray<FUEPlayerMontageEntry> MontageEntries;

	// 위 명시 슬롯으로 부족한 시퀀스는 태그 배열에 추가한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Sequence")
	TArray<FUEPlayerSequenceEntry> SequenceEntries;

	UFUNCTION(BlueprintPure, Category = "Animation")
	const UAnimMontage* FindMontageByTag(const FGameplayTag& MontageTag) const;

	UFUNCTION(BlueprintPure, Category = "Animation")
	const UAnimSequence* FindSequenceByTag(const FGameplayTag& SequenceTag) const;
};
