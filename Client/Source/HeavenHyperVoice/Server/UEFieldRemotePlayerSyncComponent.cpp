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

void UUEFieldRemotePlayerSyncComponent::HandleRemotePlayerSpawned(
	const FHHVFieldEntity& Entity,
	const FVector& SpawnLocation)
{
	if (RemotePlayers.Contains(Entity.EntityId))
	{
		return;
	}

	UWorld* World = GetWorld();
	AUEPlayerCharacter* OwnerPlayer = GetOwnerPlayerCharacter();
	if (!World || !OwnerPlayer)
	{
		return;
	}

	const FTransform SpawnTransform(FRotator(0.0f, Entity.Facing, 0.0f), SpawnLocation);
	AUEPlayerCharacter* Proxy = World->SpawnActorDeferred<AUEPlayerCharacter>(
		OwnerPlayer->GetClass(),
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Proxy)
	{
		UE_LOG(LogTemp, Warning, TEXT("FieldRemotePlayerSync: remote player %llu spawn failed"), Entity.EntityId);
		return;
	}

	Proxy->MakeRemoteProxy();
	Proxy->FinishSpawning(SpawnTransform);
	RemotePlayers.Add(Entity.EntityId, Proxy);
}

void UUEFieldRemotePlayerSyncComponent::HandleRemotePlayerMoved(
	const FHHVFieldEntity& Entity,
	const FVector& TargetLocation)
{
	const TWeakObjectPtr<AUEPlayerCharacter>* Found = RemotePlayers.Find(Entity.EntityId);
	if (!Found || !Found->IsValid())
	{
		return;
	}

	Found->Get()->ApplyRemoteMoveTarget(
		TargetLocation,
		FRotator(0.0f, Entity.Facing, 0.0f),
		/*bTeleported=*/false);
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
	for (TPair<uint64, TWeakObjectPtr<AUEPlayerCharacter>>& Pair : RemotePlayers)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Destroy();
		}
	}
	RemotePlayers.Empty();
}

AUEPlayerCharacter* UUEFieldRemotePlayerSyncComponent::GetOwnerPlayerCharacter() const
{
	return Cast<AUEPlayerCharacter>(GetOwner());
}
