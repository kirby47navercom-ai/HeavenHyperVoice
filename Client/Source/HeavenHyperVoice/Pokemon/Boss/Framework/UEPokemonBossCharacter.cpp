#include "UEPokemonBossCharacter.h"

#include "../Data/UEPokemonBossData.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AUEPokemonBossCharacter::AUEPokemonBossCharacter()
{
	// 일반 포켓몬 부모 클래스의 AI 기본값을 보스 전용 기반에서는 명시적으로 제거한다.
	AIControllerClass = nullptr;
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void AUEPokemonBossCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyBossData();
}

void AUEPokemonBossCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyBossData();
}

void AUEPokemonBossCharacter::ApplyBossData()
{
	if (!BossData)
	{
		return;
	}

	const FUEPokemonBossFormData* Form = BossData->FindForm(InitialFormId);
	if (!Form)
	{
		return;
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (Form->SkeletalMesh)
		{
			MeshComponent->SetSkeletalMesh(Form->SkeletalMesh);
		}

		if (Form->AnimInstanceClass)
		{
			MeshComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			MeshComponent->SetAnimInstanceClass(Form->AnimInstanceClass);
		}

		MeshComponent->SetRelativeTransform(Form->MeshRelativeTransform);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// 이동 수치만 준비한다. 실제 이동 명령이나 AI 판단은 이 클래스에 포함하지 않는다.
		Movement->MaxWalkSpeed = BossData->MoveSpeed;
	}
}
