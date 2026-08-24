#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UEPokemonSpeciesData.generated.h"

class UAnimInstance;
class UAnimMontage;
class USkeletalMesh;

USTRUCT(BlueprintType)
struct FUEPokemonSkillData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Skill")
	FName SkillId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Skill")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Skill", meta = (ClampMin = "0.0"))
	float Power = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Skill", meta = (ClampMin = "0.0"))
	float Range = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Skill", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;
};

UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUEPokemonSpeciesData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Identity")
	FName SpeciesId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Visual")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	// 스켈레탈 메시가 없을 때 큐브를 칠할 색. 알파가 0 이면 미설정으로 보고
	// 종족 id 로 정한 기본 팔레트를 쓴다. 실제 모델이 들어오면 이 값은 무시된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Visual")
	FLinearColor DebugColor = FLinearColor(0.f, 0.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Visual")
	TSubclassOf<UAnimInstance> AnimInstanceClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Visual")
	FTransform MeshRelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Animation")
	TObjectPtr<UAnimMontage> SpawnMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Animation")
	TObjectPtr<UAnimMontage> DespawnMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Collision", meta = (ClampMin = "0.0"))
	float CapsuleRadius = 34.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Collision", meta = (ClampMin = "0.0"))
	float CapsuleHalfHeight = 88.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Movement", meta = (ClampMin = "0.0"))
	float MoveSpeed = 280.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Movement", meta = (ClampMin = "0.0"))
	float MaxStepHeight = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Movement", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float WalkableFloorAngleDegrees = 44.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Stats", meta = (ClampMin = "1.0"))
	float MaxHP = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Skill")
	TArray<FUEPokemonSkillData> Skills;
};
