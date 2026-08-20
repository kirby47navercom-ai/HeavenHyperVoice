#pragma once

#include "CoreMinimal.h"
#include "../CharacterCustomization/HHV/Data/UEHHVCustomizationTypes.h"
#include "../Pokemon/AI/Own/PokemonFSM.h"
#include "../Pokemon/Server/UEPokemonServerSubsystem.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "UEPlayerCharacter.generated.h"

class AUEPokemonCharacter;
class UCameraComponent;
class UAnimSequence;
class USpringArmComponent;
class USkeletalMeshComponent;
class UUEHHVCustomizationCatalog;
class UUEPokemonSpeciesData;
class UUEPokemonWorldSubsystem;
class UUEPlayerAnimationDataAsset;
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

	UFUNCTION(BlueprintCallable, Category = "Character|Movement")
	void SetRunning(bool bNewIsRunning);

	UFUNCTION(BlueprintPure, Category = "Character|State")
	bool IsRolling() const { return bIsRolling; }

	UFUNCTION(BlueprintPure, Category = "Character|State")
	FVector2D GetMovementInput() const { return MovementInput; }

	UFUNCTION(BlueprintPure, Category = "Character|State")
	FVector GetDesiredMovementDirection() const;

	UFUNCTION(BlueprintPure, Category = "Character|Movement Sync")
	UUEPlayerMovementSyncComponent* GetMovementSyncComponent() const { return MovementSyncComponent; }

	UFUNCTION(BlueprintPure, Category = "Animation")
	UUEPlayerAnimationDataAsset* GetPlayerAnimationData() const { return PlayerAnimationData; }

	UFUNCTION(BlueprintPure, Category = "Customization")
	EUEHHVGender GetCustomizationGender() const { return CurrentCustomizationGender; }

	UFUNCTION(BlueprintPure, Category = "Pokemon|Companion")
	bool IsPokemonCompanionSpawned() const;

	UFUNCTION(BlueprintPure, Category = "Pokemon|Companion")
	AUEPokemonCharacter* GetSpawnedPokemonCompanion() const { return SpawnedPokemon.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void ApplyHHVAppearance(const FUEHHVAppearance& NewAppearance);
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Companion")
	void SetPokemonCompanionSpeciesData(UUEPokemonSpeciesData* NewSpeciesData);

	UFUNCTION(BlueprintPure, Category = "Pokemon|Companion")
	UUEPokemonSpeciesData* GetPokemonCompanionSpeciesData() const { return PokemonCompanionSpeciesData; }

	UFUNCTION(BlueprintCallable, Category = "Pokemon|Companion")
	void SetSelectedPokemonCompanionInstanceId(int32 NewPokemonInstanceId);

	UFUNCTION(BlueprintPure, Category = "Pokemon|Companion")
	int32 GetSelectedPokemonCompanionInstanceId() const { return SelectedCompanionPokemonInstanceId; }

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<USkeletalMeshComponent> HHVBodyEquipmentMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<USkeletalMeshComponent> HHVHeadMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<USkeletalMeshComponent> HHVHairMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<UUEHHVCustomizationCatalog> HHVCustomizationCatalog = nullptr;

	// 플레이어 애님 블루프린트나 몽타주 재생 코드가 참조할 기본 애니메이션 데이터 에셋이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UUEPlayerAnimationDataAsset> PlayerAnimationData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement", meta = (ClampMin = "1.0"))
	float RunSpeedMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pokemon|Companion")
	TSubclassOf<AUEPokemonCharacter> PokemonCompanionClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pokemon|Companion")
	TObjectPtr<UUEPokemonSpeciesData> PokemonCompanionSpeciesData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (ClampMin = "1"))
	int32 ServerPlayerId = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server")
	TArray<FUEPokemonServerOwnedPokemon> ServerOwnedPokemons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Server", meta = (ClampMin = "0"))
	int32 SelectedCompanionPokemonInstanceId = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Companion", meta = (ClampMin = "0.0"))
	float PokemonDespawnDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Companion", meta = (ClampMin = "0.0"))
	float PokemonSpawnAnimationDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Companion", meta = (ClampMin = "0.0"))
	float PokemonSpawnGroundTraceDistance = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Companion")
	TEnumAsByte<ECollisionChannel> PokemonSpawnCollisionChannel = ECC_Pawn;

private:
	bool TrySpawnPokemonCompanion();
	void RequestDespawnPokemonCompanion();
	void FinishPokemonDespawn();
	void RefreshMovementSpeed();
	void RegisterPokemonServerRoster();
	FUEPokemonServerSpawnResponse RequestPokemonServerSpawn();
	void ReleasePokemonServerSpawn(const FUEPokemonServerSpawnResponse& SpawnResponse);
	void NotifyPokemonServerDespawned(AUEPokemonCharacter* PokemonToDestroy);
	void NotifyPokemonWorldDespawned(AUEPokemonCharacter* PokemonToDestroy);
	UUEPokemonServerSubsystem* GetPokemonServerSubsystem() const;
	UUEPokemonWorldSubsystem* GetPokemonWorldSubsystem() const;
	HHV::PokemonAI::OwnContext MakePokemonLifecycleContext(HHV::PokemonAI::RequestedAction ActionRequest) const;
	HHV::Map::AgentSettings MakePokemonAgentSettings() const;
	bool ResolvePokemonSpawnTransform(const HHV::PokemonAI::Command& SpawnCommand, FVector& OutLocation, FRotator& OutRotation) const;
	bool TryResolvePokemonSpawnCandidate(const FVector& CandidateLocation, const FRotator& SpawnRotation, FVector& OutLocation) const;

	static HHV::Map::Vec3 ToServerVec3(const FVector& Vector);
	static FVector ToUnrealVector(const HHV::Map::Vec3& Vector);

	void PlayerCharacterInit();
	void ApplyPendingHHVAppearance();
	void ResetHHVMaterials(USkeletalMeshComponent* Component) const;
	void ApplyHHVMeshLocalMaterials(USkeletalMeshComponent* Component) const;
	void ApplyHHVMorphSafeMaterials(USkeletalMeshComponent* Component) const;
	void ApplyHHVColorToSlots(USkeletalMeshComponent* Component, const FLinearColor& Color, const TArray<FString>& SlotContains) const;
	void ApplyHHVEyeMaterial(USkeletalMeshComponent* Component, const FUEHHVCustomizationOption& EyeOption, const FLinearColor& EyeColor) const;
	bool IsHHVEyeMaterialSlot(USkeletalMeshComponent* Component, int32 MaterialIndex) const;
	void HideHHVFaceCoverSections(USkeletalMeshComponent* Component) const;
	void HideHHVBaseBodyOutfitSections(USkeletalMeshComponent* Component) const;
	void HideHHVEquipmentSkinSections(USkeletalMeshComponent* Component) const;
	void HideUnsupportedHHVAttachmentComponents() const;
	void ApplyHHVScale(const FUEHHVAppearance& NewAppearance) const;
	void UpdateHHVAnimation();
	void PlayHHVAnimation(UAnimSequence* Sequence, bool bLoop);
	void PlayHHVAnimationOnComponent(USkeletalMeshComponent* Component, UAnimSequence* Sequence, bool bLoop) const;
	

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character|State", meta = (AllowPrivateAccess = "true"))
	FVector2D MovementInput = FVector2D::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character|State", meta = (AllowPrivateAccess = "true"))
	bool bIsRunning = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character|State", meta = (AllowPrivateAccess = "true"))
	bool bIsRolling = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Customization", meta = (AllowPrivateAccess = "true"))
	EUEHHVGender CurrentCustomizationGender = EUEHHVGender::TypeA;

	UPROPERTY(Transient)
	TObjectPtr<AUEPokemonCharacter> SpawnedPokemon = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AUEPokemonCharacter> PendingDespawnPokemon = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentHHVAnimation = nullptr;

	HHV::PokemonAI::PokemonFSM PokemonLifecycleBrain;
	FTimerHandle PokemonDespawnTimerHandle;
	bool bPokemonDespawnInProgress = false;
public:
	//행동관련
	void Roll();
};
