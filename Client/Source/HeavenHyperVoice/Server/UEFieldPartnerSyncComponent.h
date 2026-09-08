#pragma once

// 캐릭터를 따라다니는 파트너 포켓몬.
//
// 로컬 플레이어의 파트너는 로그인 때 받은 캐릭터 정보에서 오고, 남의 파트너는
// 그 플레이어가 스폰될 때 스냅샷에서 온다.
//
// 따라갈 위치는 서버가 맵/navmesh 로 확정해서 보낸다. 클라는 파트너 액터를
// 만들고 서버 위치를 보간해 보여 주는 일만 한다.

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
	bool ApplyPartnerServerState(uint64 OwnerEntityId, const FVector& ServerLocation,
		const FVector& ServerVelocity, const FRotator& ServerRotation, bool bTeleported, double ServerTimeSeconds);

	// 주인이 시야에서 사라지면 파트너도 같이 없앤다.
	bool RemovePartner(uint64 OwnerEntityId);
	void DestroyPartners();

protected:
	// 이만큼 벌어지면 따라가기를 포기하고 붙여 놓는다. 예전 FollowOwnerAction 과
	// 같은 값이다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "1"))
	float TeleportDistance = 900.0f;

private:
	struct FPartner
	{
		TWeakObjectPtr<AUEPokemonCharacter> Actor;
		int32 DexNumber = 0;
	};

	// 주인 엔티티 id -> 그 주인의 파트너. 로컬 플레이어도 여기에 들어간다.
	TMap<uint64, FPartner> Partners;

	UPROPERTY()
	TSubclassOf<AUEPokemonCharacter> PartnerPokemonClass;
};
