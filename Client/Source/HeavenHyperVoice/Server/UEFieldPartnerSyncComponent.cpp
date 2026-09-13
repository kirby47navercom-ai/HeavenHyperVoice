#include "UEFieldPartnerSyncComponent.h"

#include "../Pokemon/UEPokemonCharacter.h"

#include "Engine/World.h"

UUEFieldPartnerSyncComponent::UUEFieldPartnerSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UUEFieldPartnerSyncComponent::SetPartnerPokemonClass(
    TSubclassOf<AUEPokemonCharacter> InPartnerPokemonClass)
{
	if (InPartnerPokemonClass)
	{
		PartnerPokemonClass = InPartnerPokemonClass;
	}
}

void UUEFieldPartnerSyncComponent::AddPartner(uint64 OwnerEntityId, AActor *OwnerActor, int32 DexNumber)
{
	if (DexNumber <= 0 || OwnerActor == nullptr)
	{
		return;
	}
	if (const FPartner *Existing = Partners.Find(OwnerEntityId))
	{
		if (Existing->Actor.IsValid() && Existing->DexNumber == DexNumber)
		{
			return;
		}
		RemovePartner(OwnerEntityId);
	}

	UWorld *World = GetWorld();
	if (!World)
	{
		return;
	}

	// 야생 포켓몬과 같은 액터 클래스다. 네이티브 클래스에는 메시도 종족 카탈로그도
	// 없어서, 이게 비어 있으면 스폰해 봐야 화면에 아무것도 안 보인다.
	if (!PartnerPokemonClass)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("FieldPartnerSync: PartnerPokemonClass is not assigned; partner %d will not be spawned."),
		       DexNumber);
		return;
	}

	const FVector SpawnLocation = OwnerActor->GetActorLocation();
	const FTransform SpawnTransform(OwnerActor->GetActorRotation(), SpawnLocation);

	AUEPokemonCharacter *PartnerActor =
	    World->SpawnActorDeferred<AUEPokemonCharacter>(PartnerPokemonClass, SpawnTransform, nullptr, nullptr,
	                                                   ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!PartnerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("FieldPartnerSync: partner spawn failed for owner %llu"),
		       OwnerEntityId);
		return;
	}

	PartnerActor->AutoPossessAI = EAutoPossessAI::Disabled;
	PartnerActor->AIControllerClass = nullptr;
	PartnerActor->FinishSpawning(SpawnTransform);

	// 주인과 부딪히면 서로 밀어내다 둘 다 튄다. 관통 회피는 서버 추종 로직이 맡는다.
	PartnerActor->SetActorEnableCollision(false);

	// 종족은 도감번호로 찾는다. 엔티티 id 는 서버 것이 아니라 주인 것을 그대로
	// 쓴다 — 서버에 파트너 엔티티가 없어서 겹칠 번호도 없다.
	PartnerActor->InitializeServerEntity(static_cast<int64>(OwnerEntityId), DexNumber,
	                                     EUEPokemonRenderType::Own);

	// 기본값(300)은 서버 스냅샷을 받는 야생 포켓몬 기준이다. 주인을 쫓는 파트너는
	// 주인이 달리기만 해도 그만큼 뒤처져서, 그대로 두면 매 프레임 순간이동한다.
	PartnerActor->SetServerHardSnapDistance(TeleportDistance);

	Partners.Add(OwnerEntityId, FPartner{PartnerActor, DexNumber});
}

bool UUEFieldPartnerSyncComponent::ApplyPartnerServerState(uint64 OwnerEntityId,
                                                           const hhv::movement::State &State,
                                                           bool bTeleported, double ServerTimeSeconds)
{
	const FPartner *Partner = Partners.Find(OwnerEntityId);
	if (Partner == nullptr || !Partner->Actor.IsValid())
	{
		return false;
	}

	Partner->Actor->ApplyCoreSnapshot(State, ServerTimeSeconds, bTeleported);
	return true;
}

bool UUEFieldPartnerSyncComponent::RemovePartner(uint64 OwnerEntityId)
{
	FPartner Partner;
	if (!Partners.RemoveAndCopyValue(OwnerEntityId, Partner))
	{
		return false;
	}

	if (Partner.Actor.IsValid())
	{
		Partner.Actor->Destroy();
	}
	return true;
}

void UUEFieldPartnerSyncComponent::DestroyPartners()
{
	for (TPair<uint64, FPartner> &Pair : Partners)
	{
		if (Pair.Value.Actor.IsValid())
		{
			Pair.Value.Actor->Destroy();
		}
	}
	Partners.Empty();
}
