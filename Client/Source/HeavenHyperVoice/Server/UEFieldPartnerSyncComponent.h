#pragma once

// 캐릭터를 따라다니는 파트너 포켓몬.
//
// 필드 서버는 파트너를 엔티티로 시뮬레이션하지 않는다. 플레이어 엔티티에
// partner_species(도감번호) 하나가 붙어 올 뿐이라 따라다니는 동작은 전부
// 클라이언트가 만든다 — 서버로 나가는 것도, 서버에서 오는 것도 없다.
//
// 로컬 플레이어의 파트너는 로그인 때 받은 캐릭터 정보에서 오고, 남의 파트너는
// 그 플레이어가 스폰될 때 스냅샷에서 온다.
//
// 따라가는 규칙은 예전 서버의 FollowOwnerAction 을 그대로 가져왔다 (커밋
// 4bfafa94 에서 임시 서버 코드와 함께 지워졌다). 주인 앞 좌우 중 가까운 쪽에
// 서고, 도착하면 멈췄다가 잠시 뒤 주인을 바라본다.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"

#include "UEFieldPartnerSyncComponent.generated.h"

class AUEPokemonCharacter;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUEFieldPartnerSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUEFieldPartnerSyncComponent();

	void SetPartnerPokemonClass(TSubclassOf<AUEPokemonCharacter> InPartnerPokemonClass);

	// DexNumber 가 0 이면 파트너가 없는 캐릭터다 — 아무것도 만들지 않는다.
	// 같은 주인을 다시 등록하면 무시한다.
	void AddPartner(uint64 OwnerEntityId, AActor* OwnerActor, int32 DexNumber);

	// 주인이 시야에서 사라지면 파트너도 같이 없앤다.
	bool RemovePartner(uint64 OwnerEntityId);
	void DestroyPartners();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// 주인 앞쪽으로 얼마나 나가서 설지. 뒤가 아니라 앞이라 화면에 보인다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "0"))
	float FollowForwardOffset = 150.0f;

	// 주인 정면을 막지 않게 좌우로 비켜선다. 둘 중 가까운 쪽을 고른다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "0"))
	float FollowSideOffset = 150.0f;

	// 이 거리 안에 들면 도착으로 본다. 없으면 목표점 주변에서 떤다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "1"))
	float ArriveDistance = 45.0f;

	// 반대쪽이 이만큼 더 가까워야 자리를 바꾼다. 없으면 주인이 방향을 틀 때마다
	// 좌우가 번갈아 뒤집힌다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "0"))
	float SideSwitchMargin = 80.0f;

	// 주인을 이 반경 안으로 관통하지 않는다. 캡슐 반지름보다 넉넉해야 한다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "0"))
	float OwnerAvoidRadius = 90.0f;

	// 도착해서 이만큼 서 있으면 주인 쪽으로 몸을 돌린다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "0"))
	float FaceOwnerDelay = 1.0f;

	// 뒤처진 거리 1 당 붙는 추가 속도. 주인과 같은 속도로만 달리면 한 번 벌어진
	// 간격이 영원히 그대로 남는다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "0"))
	float CatchUpGain = 2.0f;

	// 추격 속도 상한. 없으면 멀리 떨어졌을 때 화면을 가로질러 쏘아진다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "1"))
	float MaxFollowSpeed = 1200.0f;

	// 이만큼 벌어지면 따라가기를 포기하고 붙여 놓는다. 예전 FollowOwnerAction 과
	// 같은 값이다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "1"))
	float TeleportDistance = 900.0f;

private:
	struct FPartner
	{
		TWeakObjectPtr<AActor> Owner;
		TWeakObjectPtr<AUEPokemonCharacter> Actor;

		// 주인 앞 어느 쪽에 서 있는지. +1 이 오른쪽이다.
		float SideSign = 1.0f;

		// 목표점에 서 있던 시간. FaceOwnerDelay 를 넘으면 주인을 본다.
		float IdleSeconds = 0.0f;
	};

	// 주인 앞 좌우 중 한 자리. SideSign 이 +1 이면 오른쪽이다.
	FVector StandingSpot(const AActor& Owner, float SideSign) const;

	// 주인을 관통하지 않게, 필요하면 옆으로 돌아가는 경유지를 돌려준다.
	FVector AvoidOwner(const FVector& From, const FVector& To, const AActor& Owner,
		float SideSign) const;

	// 주인 엔티티 id -> 그 주인의 파트너. 로컬 플레이어도 여기에 들어간다.
	TMap<uint64, FPartner> Partners;

	UPROPERTY()
	TSubclassOf<AUEPokemonCharacter> PartnerPokemonClass;
};
