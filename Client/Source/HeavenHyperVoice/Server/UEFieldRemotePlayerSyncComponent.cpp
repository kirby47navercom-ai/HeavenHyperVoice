#include "UEFieldRemotePlayerSyncComponent.h"

#include "../Net/HHVFieldConnection.h"
#include "../Character/UEPlayerCharacter.h"

#include "Engine/World.h"

UUEFieldRemotePlayerSyncComponent::UUEFieldRemotePlayerSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UUEFieldRemotePlayerSyncComponent::ContainsRemotePlayer(uint64 EntityId) const
{
	return RemotePlayers.Contains(EntityId);
}

AUEPlayerCharacter *UUEFieldRemotePlayerSyncComponent::FindRemotePlayer(uint64 EntityId) const
{
	const TWeakObjectPtr<AUEPlayerCharacter> *Found = RemotePlayers.Find(EntityId);
	return Found && Found->IsValid() ? Found->Get() : nullptr;
}

void UUEFieldRemotePlayerSyncComponent::HandleRemotePlayerSpawned(const FHHVFieldEntity& Entity)
{
	if (RemotePlayers.Contains(Entity.EntityId))
	{
		return;
	}

	UWorld *World = GetWorld();
	AUEPlayerCharacter *OwnerPlayer = GetOwnerPlayerCharacter();
	if (!World || !OwnerPlayer)
	{
		return;
	}

	const auto& State = Entity.CoreState;
	const FVector SpawnLocation(State.position.x, State.position.y, State.position.z);
	const FTransform SpawnTransform(FRotator(0.0f, State.facing, 0.0f), SpawnLocation);
	AUEPlayerCharacter *Proxy = World->SpawnActorDeferred<AUEPlayerCharacter>(
	    OwnerPlayer->GetClass(), SpawnTransform, nullptr, nullptr,
	    ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Proxy)
	{
		UE_LOG(LogTemp, Warning, TEXT("FieldRemotePlayerSync: remote player %llu spawn failed"),
		       Entity.EntityId);
		return;
	}

	Proxy->MakeRemoteProxy();
	Proxy->FinishSpawning(SpawnTransform);

	// 여기는 spawned 에서만 불리므로 보통은 외형이 실려 있다. 그래도 확인하는
	// 이유는 이 필드가 나중에 붙은 것이라서다 — 안 싣는 옛 서버에 붙으면
	// 기본값으로 덮어써서 커마가 아니라 맨 얼굴이 된다. 그때는 액터의 기본
	// 외형을 그대로 두는 편이 낫다.
	if (Entity.bHasAppearance)
	{
		Proxy->ApplyHHVAppearance(Entity.Appearance);
	}

	Proxy->ApplyRemoteCoreState(Entity.CoreState, Entity.ServerTimeSeconds, true);
	RemotePlayers.Add(Entity.EntityId, Proxy);
}

void UUEFieldRemotePlayerSyncComponent::HandleRemotePlayerMoved(const FHHVFieldEntity& Entity)
{
	const TWeakObjectPtr<AUEPlayerCharacter> *Found = RemotePlayers.Find(Entity.EntityId);
	if (!Found || !Found->IsValid())
	{
		return;
	}

	Found->Get()->ApplyRemoteCoreState(Entity.CoreState, Entity.ServerTimeSeconds, false);
}

bool UUEFieldRemotePlayerSyncComponent::HandleRemotePlayerDespawned(uint64 EntityId)
{
	TWeakObjectPtr<AUEPlayerCharacter> RemotePlayer;
	if (!RemotePlayers.RemoveAndCopyValue(EntityId, RemotePlayer))
	{
		return false;
	}

	if (RemotePlayer.IsValid())
	{
		RemotePlayer->Destroy();
	}
	return true;
}

void UUEFieldRemotePlayerSyncComponent::DestroyRemotePlayers()
{
	for (TPair<uint64, TWeakObjectPtr<AUEPlayerCharacter>> &Pair : RemotePlayers)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Destroy();
		}
	}
	RemotePlayers.Empty();
}

AUEPlayerCharacter *UUEFieldRemotePlayerSyncComponent::GetOwnerPlayerCharacter() const
{
	return Cast<AUEPlayerCharacter>(GetOwner());
}
