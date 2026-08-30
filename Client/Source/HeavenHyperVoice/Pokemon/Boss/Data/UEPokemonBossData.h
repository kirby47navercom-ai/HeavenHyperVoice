#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UEPokemonBossData.generated.h"

class UAnimInstance;
class UAnimSequence;
class USkeletalMesh;

/**
 * 보스전에서 바로 꺼내 쓰는 애니메이션 묶음이다.
 *
 * 일반 포켓몬의 식사·수면 같은 생활 동작은 제외하고,
 * 등장·위협·전투·피격·그로기·격파에 필요한 동작만 명시적으로 보관한다.
 * 각 항목을 이름 있는 프로퍼티로 둔 이유는 블루프린트에서 배열 번호를 외우지 않고
 * Attack01, Roar처럼 용도를 보고 안전하게 선택할 수 있게 하기 위해서다.
 */
USTRUCT(BlueprintType)
struct FUEPokemonBossAnimationSet
{
	GENERATED_BODY()

	// 보스가 전투 장소에 모습을 드러내는 3단계 동작이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Entrance")
	TObjectPtr<UAnimSequence> EntranceStart = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Entrance")
	TObjectPtr<UAnimSequence> EntranceLoop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Entrance")
	TObjectPtr<UAnimSequence> EntranceEnd = nullptr;

	// 플레이어를 발견하고 전투를 시작할 때 사용하는 위협 동작이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Presence")
	TObjectPtr<UAnimSequence> Notice = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Presence")
	TObjectPtr<UAnimSequence> Roar = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Presence")
	TObjectPtr<UAnimSequence> Aggro = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Presence")
	TObjectPtr<UAnimSequence> PhaseTransition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Presence")
	TObjectPtr<UAnimSequence> Victory = nullptr;

	// 전투 중 정지 상태에서 반복 재생할 대기 동작들이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Idle")
	TObjectPtr<UAnimSequence> Idle = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Idle")
	TObjectPtr<UAnimSequence> Idle01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Idle")
	TObjectPtr<UAnimSequence> Idle02 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Idle")
	TObjectPtr<UAnimSequence> BattleIdle = nullptr;

	// 이동과 제자리 방향 전환에 사용하는 기본 동작이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Locomotion")
	TObjectPtr<UAnimSequence> Walk = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Locomotion")
	TObjectPtr<UAnimSequence> Run = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Locomotion")
	TObjectPtr<UAnimSequence> TurnLeft90 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Locomotion")
	TObjectPtr<UAnimSequence> TurnRight90 = nullptr;

	// 공중 이동이나 낙하 공격을 나중에 붙일 수 있도록 실제 보유 동작을 보관한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Airborne")
	TObjectPtr<UAnimSequence> JumpStart = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Airborne")
	TObjectPtr<UAnimSequence> JumpLoop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Airborne")
	TObjectPtr<UAnimSequence> FallStart = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Airborne")
	TObjectPtr<UAnimSequence> FallLoop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Airborne")
	TObjectPtr<UAnimSequence> Land = nullptr;

	// 근접 공격과 원거리 차징 공격을 용도별로 나눈다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Combat")
	TObjectPtr<UAnimSequence> Attack01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Combat")
	TObjectPtr<UAnimSequence> Attack02 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Combat")
	TObjectPtr<UAnimSequence> RangeAttack01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Combat")
	TObjectPtr<UAnimSequence> RangeAttack02Start = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Combat")
	TObjectPtr<UAnimSequence> RangeAttack02Loop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Combat")
	TObjectPtr<UAnimSequence> RangeAttack02End = nullptr;

	// 짧은 피격 반응이다. 실제 체력 판정과 재생 조건은 나중 전투 로직이 결정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Reaction")
	TObjectPtr<UAnimSequence> Damage01 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Reaction")
	TObjectPtr<UAnimSequence> Damage02 = nullptr;

	// 큰 경직이나 그로기 상태의 시작·유지·복귀 동작이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Stun")
	TObjectPtr<UAnimSequence> StunStart = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Stun")
	TObjectPtr<UAnimSequence> StunLoop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Stun")
	TObjectPtr<UAnimSequence> StunEnd = nullptr;

	// 체력이 소진됐을 때 사용할 격파 동작이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Defeat")
	TObjectPtr<UAnimSequence> DefeatStart = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Defeat")
	TObjectPtr<UAnimSequence> DefeatLoop = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Defeat")
	TObjectPtr<UAnimSequence> DefeatEnd = nullptr;

	// 눈과 입처럼 본체 동작 위에 함께 재생할 수 있는 보조 애니메이션이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Face")
	TObjectPtr<UAnimSequence> Eye = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Face")
	TObjectPtr<UAnimSequence> Mouth = nullptr;

	/**
	 * 같은 용도의 다른 번호 버전까지 빠짐없이 보존하는 전체 보스 동작 목록이다.
	 * 위의 이름 있는 항목은 대표 동작이고, 이 배열은 페이즈별 변형을 고를 때 사용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Animation|Library")
	TArray<TObjectPtr<UAnimSequence>> BossAnimationLibrary;
};

/** 기라티나의 어나더폼·오리진폼처럼 한 보스가 가진 형태 하나를 설명한다. */
USTRUCT(BlueprintType)
struct FUEPokemonBossFormData
{
	GENERATED_BODY()

	// 코드와 블루프린트가 폼을 찾을 때 사용하는 안정적인 식별자다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Form")
	FName FormId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Form")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Form")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	// 폼의 스켈레톤에 맞는 애니메이션 블루프린트가 있을 때만 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Form")
	TSubclassOf<UAnimInstance> AnimInstanceClass;

	// 캡슐 기준 메시 위치·회전·크기를 블루프린트에서 조정할 수 있게 둔다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Form")
	FTransform MeshRelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Form")
	FUEPokemonBossAnimationSet Animations;
};

/**
 * 일반 야생 포켓몬 데이터와 분리된 보스 전용 데이터 에셋이다.
 * 이 클래스는 데이터만 보관하며 행동 트리, AI 컨트롤러, 공격 판정은 만들지 않는다.
 */
UCLASS(BlueprintType)
class HEAVENHYPERVOICE_API UUEPokemonBossData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Identity")
	FName BossId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Identity")
	FText DisplayName;

	// 블루프린트를 처음 배치했을 때 보여 줄 기본 폼이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Form")
	FName DefaultFormId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Form")
	TArray<FUEPokemonBossFormData> Forms;

	// 수치는 아직 전투 로직과 연결하지 않고, 이후 팀원이 조정할 기준값으로만 보관한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stats", meta = (ClampMin = "1.0"))
	float MaxHP = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Movement", meta = (ClampMin = "0.0"))
	float MoveSpeed = 180.0f;

	/** 폼 ID가 비어 있으면 DefaultFormId를 사용하고, 일치하는 폼이 없으면 첫 폼을 반환한다. */
	const FUEPokemonBossFormData* FindForm(FName FormId) const;

	/** 블루프린트에서 폼 데이터를 안전하게 복사해 읽는 함수다. */
	UFUNCTION(BlueprintPure, Category = "Boss|Form")
	bool GetForm(FName FormId, FUEPokemonBossFormData& OutForm) const;
};
