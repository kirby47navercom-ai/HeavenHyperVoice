#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UEPokemonAnimationDataAsset.generated.h"

class UAnimSequence;

USTRUCT(BlueprintType)
struct FUEPokemonSequenceEntry
{
	GENERATED_BODY()

public:
	// 애니메이션을 태그로 찾아야 할 때 쓰는 보조 슬롯이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	FGameplayTag SequenceTag = FGameplayTag::EmptyTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> Sequence = nullptr;
};

UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUEPokemonAnimationDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// 기본 이동 상태용 애니메이션이다. 포켓몬 애님 BP가 바로 참조할 수 있게 명시 슬롯으로 둔다.
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Sequence")
	TObjectPtr<UAnimSequence> RollSequence = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Sequence")
	TArray<FUEPokemonSequenceEntry> SequenceEntries;

	UFUNCTION(BlueprintPure, Category = "Animation")
	const UAnimSequence* FindSequenceByTag(const FGameplayTag& SequenceTag) const;
};
