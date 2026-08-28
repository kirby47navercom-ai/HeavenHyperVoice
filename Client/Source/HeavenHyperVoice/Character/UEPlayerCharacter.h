#pragma once

#include "CoreMinimal.h"
#include "../CharacterCustomization/HHV/Data/UEHHVCustomizationTypes.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPath.h"
#include "TimerManager.h"
#include "UEPlayerCharacter.generated.h"

class UAnimInstance;
class UCameraComponent;
class UAnimSequence;
class USpringArmComponent;
class USkeletalMeshComponent;
class UUEHHVCustomizationCatalog;
class UUEFieldPartnerSyncComponent;
class UUEFieldRemotePlayerSyncComponent;
class UUEFieldWildPokemonSyncComponent;
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

	/**
	 * 외형을 밖에서 지시받는 액터로 표시한다. 로비 슬롯 프리뷰처럼 슬롯마다
	 * 다른 캐릭터를 그리는 경우에 쓴다.
	 *
	 * BeginPlay 는 GameInstance 의 PendingHHVAppearance 를 입히는데, 그건
	 * "내 캐릭터가 레벨을 넘어가도 착장을 유지한다" 는 뜻이라 프리뷰에는 맞지 않는다.
	 * 표시해 두지 않으면 밖에서 ApplyHHVAppearance 로 넣은 값을 BeginPlay 가 덮는다.
	 *
	 * Spawn 직후, ApplyHHVAppearance 를 부르기 전에 호출할 것.
	 */
	void MakeAppearanceExternallyDriven();
	bool IsAppearanceExternallyDriven() const { return bAppearanceExternallyDriven; }

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

	UFUNCTION(BlueprintPure, Category = "Field Server")
	UUEFieldWildPokemonSyncComponent* GetFieldWildPokemonSyncComponent() const { return FieldWildPokemonSyncComponent; }

	UFUNCTION(BlueprintPure, Category = "Field Server")
	UUEFieldRemotePlayerSyncComponent* GetFieldRemotePlayerSyncComponent() const { return FieldRemotePlayerSyncComponent; }

	UUEFieldPartnerSyncComponent* GetFieldPartnerSyncComponent() const { return FieldPartnerSyncComponent; }

	UFUNCTION(BlueprintPure, Category = "Animation")
	UUEPlayerAnimationDataAsset* GetPlayerAnimationData() const { return PlayerAnimationData; }

	UFUNCTION(BlueprintPure, Category = "Customization")
	EUEHHVGender GetCustomizationGender() const { return CurrentCustomizationGender; }

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void ApplyHHVAppearance(const FUEHHVAppearance& NewAppearance);

	void SetMovementInput(const FVector2D& NewMovementInput);
	void ApplyServerMovementCorrection(const FVector& ServerPosition, const FVector& ServerVelocity, const FRotator& ServerRotation, bool bUseHardCorrection);

	UFUNCTION(BlueprintCallable, Category = "Field Server|Pokemon")
	void RequestPokemonToggle();

	// 숫자키 기술 슬롯을 현재 필드에 꺼낸 소유 포켓몬의 공격 명령으로 전달한다.
	UFUNCTION(BlueprintCallable, Category = "Pokemon|Combat")
	bool CommandPokemonAttack(int32 AttackSlot);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyJumpApex() override;

	void ApplyLocalMovementInput();
	FVector GetCameraForwardAxis(const FRotator& ViewRotation) const;
	FVector GetCameraRightAxis(const FRotator& ViewRotation) const;
	FVector GetMoveDirectionFromInput(const FVector2D& Input, const FRotator& ViewRotation) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<UCameraComponent> FollowCamera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Movement Sync")
	TObjectPtr<UUEPlayerMovementSyncComponent> MovementSyncComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Server")
	TObjectPtr<UUEFieldWildPokemonSyncComponent> FieldWildPokemonSyncComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Server")
	TObjectPtr<UUEFieldRemotePlayerSyncComponent> FieldRemotePlayerSyncComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Server", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UUEFieldPartnerSyncComponent> FieldPartnerSyncComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<USkeletalMeshComponent> HHVBodyEquipmentMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<USkeletalMeshComponent> HHVHeadMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<USkeletalMeshComponent> HHVHairMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<UUEHHVCustomizationCatalog> HHVCustomizationCatalog = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Materials")
	FDirectoryPath MorphSafeMaterialDirectory;

	// 플레이어 애님 블루프린트나 몽타주 재생 코드가 참조할 기본 애니메이션 데이터 에셋이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UUEPlayerAnimationDataAsset> PlayerAnimationData = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> TypeAAnimationClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> TypeBAnimationClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement", meta = (ClampMin = "1.0"))
	float RunSpeedMultiplier = 1.5f;

private:
	void ConfigureRemoteProxyMovement();
	void UpdateRemoteProxyMovement(float DeltaSeconds);

	void RefreshMovementSpeed();
	void RefreshCharacterState();
	void FinishRoll();
	void CancelLanding();
	void FinishLanding();

	// 다른 플레이어의 복제본인가. MakeRemoteProxy 로만 켜진다.
	bool bIsRemoteProxy = false;

	// 외형을 밖에서 넣어주는가. MakeAppearanceExternallyDriven 으로만 켜진다.
	bool bAppearanceExternallyDriven = false;

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
	float GetHHVActionDuration(const FGameplayTag& StateTag, float FallbackDuration) const;
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

	// 원본 구르기 시퀀스가 약 2초이므로 게임용 회피 동작 길이에 맞춰 재생한다.
	UPROPERTY(EditAnywhere, Category = "Character|State", meta = (ClampMin = "0.01"))
	float RollAnimationPlayRate = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Character|State", meta = (ClampMin = "0.0"))
	float LandingStateDuration = 0.45f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Customization", meta = (AllowPrivateAccess = "true"))
	EUEHHVGender CurrentCustomizationGender = EUEHHVGender::TypeA;

	FVector ActiveRollDirection = FVector::ForwardVector;

	FTimerHandle RollStateTimerHandle;
	FTimerHandle LandingStateTimerHandle;
	FTimerHandle GameplayQATimerHandle;
	FVector GameplayQAStartLocation = FVector::ZeroVector;
	int32 GameplayQAPhase = 0;
	bool bLandingStateActive = false;
public:
	//행동관련
	void Roll();
};
