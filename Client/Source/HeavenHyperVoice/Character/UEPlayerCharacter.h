#pragma once

#include "CoreMinimal.h"
#include "../CharacterCustomization/HHV/Data/UEHHVCustomizationTypes.h"
#include "../Pokemon/AI/Own/PokemonFSM.h"
#include "../Pokemon/Server/UEPokemonServerSubsystem.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
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
	virtual void Jump() override;
	virtual void Landed(const FHitResult& Hit) override;

	// --- 다른 플레이어를 그리는 복제본 ---
	//
	// 필드 스냅샷에 남의 캐릭터가 실려 오면 이 클래스를 그대로 한 벌 더 띄운다.
	// 로컬 플레이어와 같은 클래스를 쓰므로 메시·커마 설정을 따로 맞출 필요가 없다.
	// 다만 입력도 카메라도 없어야 하고, 무엇보다 자기 몫의 필드 연결을 열거나
	// 동행 포켓몬을 부르면 안 된다 — 인당 TLS 연결이 하나씩 더 생긴다.
	//
	// SpawnActorDeferred 로 만든 뒤 FinishSpawning **전에** 부를 것. BeginPlay 가
	// 지나면 이미 연결과 동행이 만들어진 뒤라 늦다.
	void MakeRemoteProxy();
	bool IsRemoteProxy() const { return bIsRemoteProxy; }

	// 스냅샷이 알려준 목표 지점. Tick 이 보간으로 따라간다. 20Hz 라 그대로 박으면
	// 끊겨 보인다.
	void ApplyRemoteMoveTarget(const FVector& TargetLocation, const FRotator& TargetRotation,
		bool bTeleported);

	UFUNCTION(BlueprintPure, Category = "Character|State")
	bool IsRunning() const { return bIsRunning; }

	UFUNCTION(BlueprintCallable, Category = "Character|Movement")
	void SetRunning(bool bNewIsRunning);

	UFUNCTION(BlueprintPure, Category = "Character|State")
	bool IsRolling() const { return bIsRolling; }

	UFUNCTION(BlueprintPure, Category = "Character|State")
	FGameplayTag GetCharacterStateTag() const { return CharacterStateTag; }

	// 들기, 던지기, 피격, 사망처럼 이동만으로 판단할 수 없는 상태를 외부 시스템에서 지정한다.
	UFUNCTION(BlueprintCallable, Category = "Character|State")
	void SetCharacterActionState(const FGameplayTag& NewStateTag);

	UFUNCTION(BlueprintCallable, Category = "Character|State")
	void ClearCharacterActionState();

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

	// 시작하자마자 동행 포켓몬(피카츄)을 소환한다. 입력 키로도 껐다 켤 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pokemon|Companion")
	bool bAutoSpawnPokemonCompanion = true;

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
	void ConfigureRemoteProxyMovement();
	void UpdateRemoteProxyMovement(float DeltaSeconds);

	bool TrySpawnPokemonCompanion();
	void RequestDespawnPokemonCompanion();
	void FinishPokemonDespawn();
	void RefreshMovementSpeed();
	void RefreshCharacterState();
	void FinishRoll();
	void FinishLanding();
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

	// 다른 플레이어의 복제본인가. MakeRemoteProxy 로만 켜진다.
	bool bIsRemoteProxy = false;

	// 서버가 지시한 목표. 도착하면 bHasRemoteTarget 이 꺼진다.
	FVector RemoteTargetLocation = FVector::ZeroVector;
	FRotator RemoteTargetRotation = FRotator::ZeroRotator;
	bool bHasRemoteTarget = false;

	// 로코모션 블렌드스페이스가 속도를 읽는다. 서버는 속도를 보내지 않으므로
	// 목표가 갱신될 때 좌표 차이로 만들어 넣는다.
	FVector RemoteVelocity = FVector::ZeroVector;

	// 야생 포켓몬(UEPokemonCharacter)과 같은 값으로 맞춘다. 같은 스냅샷을 보고
	// 움직이는데 둘이 다르게 미끄러지면 눈에 띈다.
	float RemoteInterpSpeed = 12.0f;
	float RemoteRotationInterpSpeed = 12.0f;
	float RemoteHardSnapDistance = 500.0f;

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
	void UpdateHHVAnimation();
	void PlayHHVAnimation(UAnimSequence* Sequence, bool bLoop);
	void PlayHHVAnimationOnComponent(USkeletalMeshComponent* Component, UAnimSequence* Sequence, bool bLoop) const;
	void ApplyHHVScale(const FUEHHVAppearance& NewAppearance) const;
	void StartGameplayQAIfRequested();
	void AdvanceGameplayQA();
	void CaptureGameplayQAFrame();
	

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character|State", meta = (AllowPrivateAccess = "true"))
	FVector2D MovementInput = FVector2D::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character|State", meta = (AllowPrivateAccess = "true"))
	bool bIsRunning = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character|State", meta = (AllowPrivateAccess = "true"))
	bool bIsRolling = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character|State", meta = (AllowPrivateAccess = "true"))
	FGameplayTag CharacterStateTag;

	UPROPERTY(Transient)
	FGameplayTag ActionStateTag;

	UPROPERTY(EditAnywhere, Category = "Character|State", meta = (ClampMin = "0.0"))
	float RollStateDuration = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Character|State", meta = (ClampMin = "0.0"))
	float LandingStateDuration = 0.15f;

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
	FTimerHandle RollStateTimerHandle;
	FTimerHandle LandingStateTimerHandle;
	FTimerHandle GameplayQATimerHandle;
	int32 GameplayQAPhase = 0;
	bool bPokemonDespawnInProgress = false;
	bool bLandingStateActive = false;
public:
	//행동관련
	void Roll();
};
