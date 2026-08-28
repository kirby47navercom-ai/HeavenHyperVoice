#pragma once

// 캐릭터를 따라다니는 파트너 포켓몬.
//
// 필드 서버는 파트너를 엔티티로 시뮬레이션하지 않는다. 플레이어 엔티티에
// partner_species(도감번호) 하나가 붙어 올 뿐이다. 그래서 따라다니는 동작은
// 전부 클라이언트가 만든다 — 서버로 나가는 것도, 서버에서 오는 것도 없다.
//
// 로컬 플레이어의 파트너는 로그인 때 받은 캐릭터 정보에서 오고, 남의 파트너는
// 그 플레이어가 스폰될 때 스냅샷에서 온다.

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
	// 주인 뒤로 얼마나 떨어져 따라갈지. 캡슐 반지름 두 개 남짓이면 겹치지 않는다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "0"))
	float FollowDistance = 180.0f;

	// 주인 정중앙 뒤에 서면 카메라에 계속 가린다. 옆으로 조금 비켜 세운다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner")
	float FollowSideOffset = 90.0f;

	// 이 거리 안에서는 멈춘다. 없으면 주인이 서 있어도 목표점 주변에서 떤다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "1"))
	float FollowTolerance = 60.0f;

	// 뒤처진 거리 1 당 붙는 추가 속도. 주인과 같은 속도로만 달리면 한 번 벌어진
	// 간격이 영원히 그대로 남는다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "0"))
	float CatchUpGain = 2.0f;

	// 추격 속도 상한. 없으면 멀리 떨어졌을 때 화면을 가로질러 쏘아진다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "1"))
	float MaxFollowSpeed = 1200.0f;

	// 이만큼 벌어지면 따라가기를 포기하고 붙여 놓는다. 레벨 이동처럼 정말로
	// 멀어진 경우만 걸리게 크게 잡는다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "1"))
	float TeleportDistance = 3000.0f;

private:
	struct FPartner
	{
		TWeakObjectPtr<AActor> Owner;
		TWeakObjectPtr<AUEPokemonCharacter> Actor;
	};

	// 주인 엔티티 id -> 그 주인의 파트너. 로컬 플레이어도 여기에 들어간다.
	TMap<uint64, FPartner> Partners;

	UPROPERTY()
	TSubclassOf<AUEPokemonCharacter> PartnerPokemonClass;
};
