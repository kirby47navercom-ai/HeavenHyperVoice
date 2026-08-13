#pragma once

#include "CoreMinimal.h"
#include "../CharacterCustomization/Palworld/Data/UEPalworldCustomizationTypes.h"
#include "../Pokemon/PokemonFSM.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "UEPlayerCharacter.generated.h"

class AUEPokemonCharacter;
class UCameraComponent;
class USpringArmComponent;
class USkeletalMeshComponent;
class UUEPalworldCustomizationCatalog;
class UUEPlayerMovementSyncComponent;

UCLASS(Blueprintable)
class HEAVENHYPERVOICE_API AUEPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AUEPlayerCharacter();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Character|State")
	bool IsRunning() const { return bIsRunning; }

	UFUNCTION(BlueprintPure, Category = "Character|State")
	bool IsRolling() const { return bIsRolling; }

	UFUNCTION(BlueprintPure, Category = "Character|State")
	FVector2D GetMovementInput() const { return MovementInput; }

	UFUNCTION(BlueprintPure, Category = "Character|State")
	FVector GetDesiredMovementDirection() const;

	UFUNCTION(BlueprintPure, Category = "Character|Movement Sync")
	UUEPlayerMovementSyncComponent* GetMovementSyncComponent() const { return MovementSyncComponent; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Companion")
	bool IsPokemonCompanionSpawned() const;

	UFUNCTION(BlueprintPure, Category = "Pokemon|Companion")
	AUEPokemonCharacter* GetSpawnedPokemonCompanion() const { return SpawnedPokemon.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Palworld|Customization")
	void ApplyPalworldAppearance(const FUEPalworldAppearance& NewAppearance);

	void SetMovementInput(const FVector2D& NewMovementInput);
	void ApplyServerMovementCorrection(const FVector& ServerPosition, const FVector& ServerVelocity, const FRotator& ServerRotation, bool bUseHardCorrection);

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Companion")
	void TogglePokemonCompanion();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ApplyLocalMovementInput();
	FVector GetCameraForwardAxis(const FRotator& ViewRotation) const;
	FVector GetCameraRightAxis(const FRotator& ViewRotation) const;
	FVector GetMoveDirectionFromInput(const FVector2D& Input, const FRotator& ViewRotation) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Pokemon|Companion", meta = (DisplayName = "On Pokemon Spawn Requested"))
	void BP_OnPokemonSpawnRequested(const FVector& SpawnLocation, const FRotator& SpawnRotation);

	UFUNCTION(BlueprintImplementableEvent, Category = "Pokemon|Companion", meta = (DisplayName = "On Pokemon Spawned"))
	void BP_OnPokemonSpawned(AUEPokemonCharacter* SpawnedCompanion);

	UFUNCTION(BlueprintImplementableEvent, Category = "Pokemon|Companion", meta = (DisplayName = "On Pokemon Despawn Requested"))
	void BP_OnPokemonDespawnRequested(AUEPokemonCharacter* DespawningPokemon);

	UFUNCTION(BlueprintImplementableEvent, Category = "Pokemon|Companion", meta = (DisplayName = "On Pokemon Despawned"))
	void BP_OnPokemonDespawned();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<UCameraComponent> FollowCamera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Movement Sync")
	TObjectPtr<UUEPlayerMovementSyncComponent> MovementSyncComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Customization")
	TObjectPtr<USkeletalMeshComponent> PalworldBodyEquipmentMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Customization")
	TObjectPtr<USkeletalMeshComponent> PalworldHeadMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Palworld|Customization")
	TObjectPtr<USkeletalMeshComponent> PalworldHairMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Palworld|Customization")
	TObjectPtr<UUEPalworldCustomizationCatalog> PalworldCustomizationCatalog = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Companion")
	TSubclassOf<AUEPokemonCharacter> PokemonCompanionClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Companion", meta = (ClampMin = "0.0"))
	float PokemonDespawnDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Companion", meta = (ClampMin = "0.0"))
	float PokemonSpawnGroundTraceDistance = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Companion")
	TEnumAsByte<ECollisionChannel> PokemonSpawnCollisionChannel = ECC_Pawn;

private:
	bool TrySpawnPokemonCompanion();
	void RequestDespawnPokemonCompanion();
	void FinishPokemonDespawn();
	HHV::PokemonAI::CompanionContext MakePokemonLifecycleContext(HHV::PokemonAI::RequestedAction ActionRequest) const;
	HHV::Map::AgentSettings MakePokemonAgentSettings() const;
	bool ResolvePokemonSpawnTransform(const HHV::PokemonAI::Command& SpawnCommand, FVector& OutLocation, FRotator& OutRotation) const;
	bool TryResolvePokemonSpawnCandidate(const FVector& CandidateLocation, const FRotator& SpawnRotation, FVector& OutLocation) const;

	static HHV::Map::Vec3 ToServerVec3(const FVector& Vector);
	static FVector ToUnrealVector(const HHV::Map::Vec3& Vector);

	void ApplyPendingPalworldAppearance();
	void ResetPalworldMaterials(USkeletalMeshComponent* Component) const;
	void ApplyPalworldMorphSafeMaterials(USkeletalMeshComponent* Component) const;
	void ApplyPalworldColorToSlots(USkeletalMeshComponent* Component, const FLinearColor& Color, const TArray<FString>& SlotContains) const;
	void ApplyPalworldEyeMaterial(USkeletalMeshComponent* Component, const FUEPalworldCustomizationOption& EyeOption, const FLinearColor& EyeColor) const;
	bool IsPalworldEyeMaterialSlot(USkeletalMeshComponent* Component, int32 MaterialIndex) const;
	void HidePalworldFaceCoverSections(USkeletalMeshComponent* Component) const;
	void HidePalworldBaseBodyOutfitSections(USkeletalMeshComponent* Component) const;
	void HideUnsupportedPalworldAttachmentComponents() const;
	void ApplyPalworldScale(const FUEPalworldAppearance& NewAppearance) const;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character|State", meta = (AllowPrivateAccess = "true"))
	FVector2D MovementInput = FVector2D::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character|State", meta = (AllowPrivateAccess = "true"))
	bool bIsRunning = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character|State", meta = (AllowPrivateAccess = "true"))
	bool bIsRolling = false;

	UPROPERTY(Transient)
	TObjectPtr<AUEPokemonCharacter> SpawnedPokemon = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AUEPokemonCharacter> PendingDespawnPokemon = nullptr;

	HHV::PokemonAI::PokemonFSM PokemonLifecycleBrain;
	FTimerHandle PokemonDespawnTimerHandle;
	bool bPokemonDespawnInProgress = false;
};
