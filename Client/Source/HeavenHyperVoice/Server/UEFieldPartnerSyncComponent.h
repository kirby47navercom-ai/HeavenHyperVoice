#pragma once

// 캐릭터를 따라다니는 파트너 포켓몬.
//
// 로컬 플레이어의 파트너는 로그인 때 받은 캐릭터 정보에서 오고, 남의 파트너는
// 그 플레이어가 스폰될 때 스냅샷에서 온다.
//
// main은 서버가 확정한 위치를 보간해요. Yang2만 같은 서버 추종 계산을 로컬에서 실행해요.

#include "CoreMinimal.h"
#include "MovementCore.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"
#include "Async/Future.h"
#include "PartnerFollower.h"
#include <memory>

#include "UEFieldPartnerSyncComponent.generated.h"

class AUEPokemonCharacter;
class AUEPlayerCharacter;
enum class EUEPokemonFieldAnimation : uint8;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUEFieldPartnerSyncComponent : public UActorComponent
{
	GENERATED_BODY()

  public:
	UUEFieldPartnerSyncComponent();

	void SetPartnerPokemonClass(TSubclassOf<AUEPokemonCharacter> InPartnerPokemonClass);

	// DexNumber 가 0 이면 파트너가 없는 캐릭터다 — 아무것도 만들지 않는다.
	// 같은 주인을 다시 등록하면 무시한다.
	void AddPartner(uint64 OwnerEntityId, AActor *OwnerActor, int32 DexNumber);
	// YANG2_CLIENT_AUTHORITY_ONLY: 서버와 같은 추종 계산을 로컬에서 실행해요.
	void AddLocalPartner(uint64 OwnerEntityId, AActor *OwnerActor, int32 DexNumber);
	bool PlayLocalPartnerAttack(uint64 OwnerEntityId, int32 AttackSlot);
	bool PlayLocalPartnerFieldAnimation(uint64 OwnerEntityId, EUEPokemonFieldAnimation Animation, int32 LoopCount);
	void PrepareLocalNavigation(AUEPlayerCharacter *Player);
	std::shared_ptr<heaven::Map> GetLocalNavigation() const { return LocalNavigation; }
	bool ApplyPartnerServerState(uint64 OwnerEntityId, const hhv::movement::State &State, bool bTeleported,
	                             double ServerTimeSeconds);

	// 주인이 시야에서 사라지면 파트너도 같이 없앤다.
	bool RemovePartner(uint64 OwnerEntityId);
	void DestroyPartners();

  protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction *TickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 서버 추종 설정과 같은 단위(cm)예요. BP에서 주인의 앞/옆 간격을 바꿔요.
	UPROPERTY(EditAnywhere, Category = "Yang2|Partner", meta = (ClampMin = "0"))
	float FollowForwardOffset = 150.0f;
	UPROPERTY(EditAnywhere, Category = "Yang2|Partner", meta = (ClampMin = "0"))
	float FollowSideOffset = 150.0f;
	// 이만큼 벌어지면 따라가기를 포기하고 붙여 놓는다. 예전 FollowOwnerAction 과
	// 같은 값이다.
	UPROPERTY(EditAnywhere, Category = "Field Server|Partner", meta = (ClampMin = "1"))
	float TeleportDistance = 900.0f;

  private:
	struct FPartner
	{
		TWeakObjectPtr<AUEPokemonCharacter> Actor;
		int32 DexNumber = 0;
		TWeakObjectPtr<AActor> LocalOwner;
		heaven::fieldshared::PartnerState FollowState;
	};

	// 주인 엔티티 id -> 그 주인의 파트너. 로컬 플레이어도 여기에 들어간다.
	TMap<uint64, FPartner> Partners;
	// YANG2_CLIENT_AUTHORITY_ONLY: 야생 AI도 이 불변 맵을 공유해 길찾기를 중복 생성하지 않아요.
	std::shared_ptr<heaven::Map> LocalNavigation;
	TFuture<std::shared_ptr<heaven::Map>> LocalNavigationLoading;
	bool bLocalNavigationStarted = false;
	double FollowAccumulator = 0.0;
	double FollowTime = 0.0;

	UPROPERTY()
	TSubclassOf<AUEPokemonCharacter> PartnerPokemonClass;
};
