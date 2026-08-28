#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UEFieldRemotePlayerSyncComponent.generated.h"

class AUEPlayerCharacter;
struct FHHVFieldEntity;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENHYPERVOICE_API UUEFieldRemotePlayerSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUEFieldRemotePlayerSyncComponent();

	bool ContainsRemotePlayer(uint64 EntityId) const;
	void HandleRemotePlayerSpawned(const FHHVFieldEntity& Entity, const FVector& SpawnLocation);
	void HandleRemotePlayerMoved(const FHHVFieldEntity& Entity, const FVector& TargetLocation);
	bool HandleRemotePlayerDespawned(uint64 EntityId);
	void DestroyRemotePlayers();

private:
	AUEPlayerCharacter* GetOwnerPlayerCharacter() const;

	TMap<uint64, TWeakObjectPtr<AUEPlayerCharacter>> RemotePlayers;
};
