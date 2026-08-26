#include "UEPokemonSummonEffectComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	const FName PokemonSummonTrailTag(TEXT("PokemonSummonLightTrail"));
	const FName PokemonSummonOrbTag(TEXT("PokemonSummonLightOrb"));
	const FName PokemonSummonTargetTag(TEXT("PokemonSummonTarget"));

	FVector EvaluateCubicBezier(
		const FVector& P0,
		const FVector& P1,
		const FVector& P2,
		const FVector& P3,
		float T)
	{
		// 3차 베지어 매개변수 방정식:
		// B(t)=(1-t)^3P0+3(1-t)^2tP1+3(1-t)t^2P2+t^3P3, 0<=t<=1
		const float OneMinusT = 1.0f - T;
		return OneMinusT * OneMinusT * OneMinusT * P0
			+ 3.0f * OneMinusT * OneMinusT * T * P1
			+ 3.0f * OneMinusT * T * T * P2
			+ T * T * T * P3;
	}

	FVector EvaluateCubicBezierDerivative(
		const FVector& P0,
		const FVector& P1,
		const FVector& P2,
		const FVector& P3,
		float T)
	{
		// B'(t)는 움직이는 빛구슬의 진행 방향과 부분 곡선의 접선을 정확히 맞춘다.
		const float OneMinusT = 1.0f - T;
		return 3.0f * OneMinusT * OneMinusT * (P1 - P0)
			+ 6.0f * OneMinusT * T * (P2 - P1)
			+ 3.0f * T * T * (P3 - P2);
	}

	template <typename ComponentType>
	ComponentType* FindTaggedComponent(AActor* OwnerActor, const FName ComponentTag)
	{
		if (!OwnerActor)
		{
			return nullptr;
		}

		TInlineComponentArray<UActorComponent*> Components;
		OwnerActor->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			if (Component && Component->ComponentHasTag(ComponentTag))
			{
				return Cast<ComponentType>(Component);
			}
		}

		return nullptr;
	}
}

UUEPokemonSummonEffectComponent::UUEPokemonSummonEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

float UUEPokemonSummonEffectComponent::PlaySpawnEffect(float Duration)
{
	return BeginEffect(EUEPokemonSummonEffectPhase::Spawning, Duration);
}

float UUEPokemonSummonEffectComponent::PlayDespawnEffect(float Duration)
{
	return BeginEffect(EUEPokemonSummonEffectPhase::Despawning, Duration);
}

void UUEPokemonSummonEffectComponent::SetEffectBodyActor(AActor* InBodyActor)
{
	EffectBodyActor = InBodyActor;
}

float UUEPokemonSummonEffectComponent::BeginEffect(
	EUEPokemonSummonEffectPhase NewPhase,
	float RequestedDuration)
{
	ACharacter* PokemonCharacter = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* MeshComponent = PokemonCharacter ? PokemonCharacter->GetMesh() : nullptr;
	if (!MeshComponent)
	{
		return 0.0f;
	}

	EffectPhase = NewPhase;
	EffectElapsedSeconds = 0.0f;
	EffectDurationSeconds = RequestedDuration > UE_KINDA_SMALL_NUMBER
		? RequestedDuration
		: DefaultEffectDuration;
	OriginalMeshScale = MeshComponent->GetRelativeScale3D();

	// 메시를 줄이기 전에 목표점과 종별 크기를 임시 컴포넌트에 기억한다.
	// 이후 메시 Scale이 변해도 베지어 곡선의 도착점은 흔들리지 않는다.
	CreateLightTrail(MeshComponent);

	if (UMaterialInterface* LoadedGlowMaterial = GlowOverlayMaterial.LoadSynchronous())
	{
		LoadedGlowMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh);
		GlowMaterialInstance = UMaterialInstanceDynamic::Create(LoadedGlowMaterial, this);
		if (GlowMaterialInstance)
		{
			GlowMaterialInstance->SetVectorParameterValue(TEXT("EffectColor"), GlowColor);
			GlowMaterialInstance->SetScalarParameterValue(TEXT("EffectStrength"), 0.0f);
			MeshComponent->SetOverlayMaterial(GlowMaterialInstance);
		}
	}

	if (EffectPhase == EUEPokemonSummonEffectPhase::Spawning)
	{
		// 빛이 소환 위치에 도착할 때까지 포켓몬 메시만 아주 작게 감춘다.
		// 액터와 캡슐은 유지하므로 서버 위치, 충돌, AI에는 영향을 주지 않는다.
		MeshComponent->SetRelativeScale3D(OriginalMeshScale * MinimumMeshScale);
	}
	else
	{
		// 귀환은 현재 포켓몬 위치에서 먼저 반짝인 뒤 몸 쪽으로 빛이 이동한다.
		SpawnBurst(MeshComponent);
	}

	SetComponentTickEnabled(true);
	return EffectDurationSeconds;
}

void UUEPokemonSummonEffectComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (EffectPhase == EUEPokemonSummonEffectPhase::None)
	{
		return;
	}

	ACharacter* PokemonCharacter = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* MeshComponent = PokemonCharacter ? PokemonCharacter->GetMesh() : nullptr;
	if (!MeshComponent)
	{
		FinishEffect();
		return;
	}

	const float PreviousProgress = FMath::Clamp(
		EffectElapsedSeconds / FMath::Max(EffectDurationSeconds, UE_KINDA_SMALL_NUMBER),
		0.0f,
		1.0f);
	EffectElapsedSeconds += DeltaTime;
	const float Progress = FMath::Clamp(
		EffectElapsedSeconds / FMath::Max(EffectDurationSeconds, UE_KINDA_SMALL_NUMBER),
		0.0f,
		1.0f);

	const bool bSpawning = EffectPhase == EUEPokemonSummonEffectPhase::Spawning;
	const float SafeTravelFraction = FMath::Clamp(LightTravelFraction, 0.1f, 0.9f);
	const float RecallCollapseEnd = 1.0f - SafeTravelFraction;
	float VisibleProgress = 0.0f;
	float TravelProgress = 0.0f;
	bool bTrailVisible = false;

	if (bSpawning)
	{
		// 1단계: 몸에서 포켓몬 위치까지 빛이 이동한다.
		// 2단계: 도착한 자리에서 포켓몬 몸이 원래 크기로 펼쳐진다.
		TravelProgress = FMath::Clamp(Progress / SafeTravelFraction, 0.0f, 1.0f);
		VisibleProgress = FMath::Clamp(
			(Progress - SafeTravelFraction) / (1.0f - SafeTravelFraction),
			0.0f,
			1.0f);
		bTrailVisible = Progress < SafeTravelFraction;

		if (PreviousProgress < SafeTravelFraction && Progress >= SafeTravelFraction)
		{
			// 빛이 도착한 한 프레임에만 Niagara를 터뜨려 중복 생성을 막는다.
			SpawnBurst(MeshComponent);
		}
	}
	else
	{
		// 1단계: 포켓몬 몸이 작은 빛으로 접힌다.
		// 2단계: 그 빛이 포켓몬 위치에서 플레이어 몸으로 돌아온다.
		VisibleProgress = 1.0f - FMath::Clamp(Progress / RecallCollapseEnd, 0.0f, 1.0f);
		TravelProgress = FMath::Clamp(
			(Progress - RecallCollapseEnd) / SafeTravelFraction,
			0.0f,
			1.0f);
		bTrailVisible = Progress >= RecallCollapseEnd;
	}

	UpdateLightTrail(TravelProgress, bSpawning, bTrailVisible);

	const float EasedVisibleProgress = FMath::InterpEaseOut(
		MinimumMeshScale,
		1.0f,
		VisibleProgress,
		2.0f);
	MeshComponent->SetRelativeScale3D(OriginalMeshScale * EasedVisibleProgress);

	if (GlowMaterialInstance)
	{
		// 소환 전에는 포켓몬을 드러내지 않고, 도착 뒤에만 발광을 줄이며 모습을 연다.
		// 귀환은 반대로 몸이 접힐수록 발광이 강해져 빛으로 변하는 느낌을 만든다.
		const float GlowEnvelope = bSpawning
			? (Progress >= SafeTravelFraction ? 1.0f - VisibleProgress : 0.0f)
			: 1.0f - VisibleProgress;
		const float SoftPulse = 0.85f + 0.15f * FMath::Abs(FMath::Sin(Progress * UE_TWO_PI * 2.0f));
		GlowMaterialInstance->SetScalarParameterValue(
			TEXT("EffectStrength"),
			PeakGlowStrength * GlowEnvelope * SoftPulse);
	}

	if (Progress >= 1.0f)
	{
		FinishEffect();
	}
}

void UUEPokemonSummonEffectComponent::FinishEffect()
{
	ACharacter* PokemonCharacter = Cast<ACharacter>(GetOwner());
	if (USkeletalMeshComponent* MeshComponent = PokemonCharacter ? PokemonCharacter->GetMesh() : nullptr)
	{
		// 소환 완료 때는 정확한 원래 크기로 복구한다. 귀환 완료 액터는 같은 프레임에 파괴된다.
		if (EffectPhase == EUEPokemonSummonEffectPhase::Spawning)
		{
			MeshComponent->SetRelativeScale3D(OriginalMeshScale);
		}
		MeshComponent->SetOverlayMaterial(nullptr);
	}

	DestroyLightTrail();
	GlowMaterialInstance = nullptr;
	EffectPhase = EUEPokemonSummonEffectPhase::None;
	SetComponentTickEnabled(false);
}

void UUEPokemonSummonEffectComponent::CreateLightTrail(const USkeletalMeshComponent* MeshComponent)
{
	AActor* OwnerActor = GetOwner();
	USceneComponent* RootComponent = OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
	UMaterialInterface* LoadedGlowMaterial = GlowOverlayMaterial.LoadSynchronous();
	UStaticMesh* LoadedTrailMesh = TrailMesh.LoadSynchronous();
	UStaticMesh* LoadedOrbMesh = OrbMesh.LoadSynchronous();
	if (!OwnerActor || !RootComponent || !MeshComponent || !LoadedGlowMaterial || !LoadedTrailMesh || !LoadedOrbMesh)
	{
		return;
	}

	DestroyLightTrail();
	LoadedGlowMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_StaticMesh);
	LoadedGlowMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_SplineMesh);

	UMaterialInstanceDynamic* TrailMaterial = UMaterialInstanceDynamic::Create(LoadedGlowMaterial, OwnerActor);
	if (TrailMaterial)
	{
		TrailMaterial->SetVectorParameterValue(TEXT("EffectColor"), GlowColor);
		TrailMaterial->SetScalarParameterValue(TEXT("EffectStrength"), TrailGlowStrength);
	}

	USplineMeshComponent* TrailComponent = NewObject<USplineMeshComponent>(
		OwnerActor,
		MakeUniqueObjectName(OwnerActor, USplineMeshComponent::StaticClass(), TEXT("PokemonSummonLightTrail")));
	TrailComponent->ComponentTags.Add(PokemonSummonTrailTag);
	// 포켓몬 캡슐이 움직이므로 런타임 연출 컴포넌트도 반드시 Movable이어야 붙을 수 있다.
	TrailComponent->SetMobility(EComponentMobility::Movable);
	TrailComponent->SetupAttachment(RootComponent);
	TrailComponent->SetStaticMesh(LoadedTrailMesh);
	TrailComponent->SetForwardAxis(ESplineMeshAxis::Z, false);
	TrailComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrailComponent->SetGenerateOverlapEvents(false);
	TrailComponent->SetCastShadow(false);
	TrailComponent->SetReceivesDecals(false);
	TrailComponent->SetMaterial(0, TrailMaterial ? TrailMaterial : LoadedGlowMaterial);
	OwnerActor->AddInstanceComponent(TrailComponent);
	TrailComponent->RegisterComponent();
	TrailComponent->SetVisibility(false, true);

	UStaticMeshComponent* OrbComponent = NewObject<UStaticMeshComponent>(
		OwnerActor,
		MakeUniqueObjectName(OwnerActor, UStaticMeshComponent::StaticClass(), TEXT("PokemonSummonLightOrb")));
	OrbComponent->ComponentTags.Add(PokemonSummonOrbTag);
	OrbComponent->SetMobility(EComponentMobility::Movable);
	OrbComponent->SetupAttachment(RootComponent);
	OrbComponent->SetStaticMesh(LoadedOrbMesh);
	OrbComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OrbComponent->SetGenerateOverlapEvents(false);
	OrbComponent->SetCastShadow(false);
	OrbComponent->SetReceivesDecals(false);
	OrbComponent->SetMaterial(0, TrailMaterial ? TrailMaterial : LoadedGlowMaterial);
	OwnerActor->AddInstanceComponent(OrbComponent);
	OrbComponent->RegisterComponent();
	OrbComponent->SetWorldScale3D(FVector(OrbScale));
	OrbComponent->SetVisibility(false, true);

	USceneComponent* TargetComponent = NewObject<USceneComponent>(
		OwnerActor,
		MakeUniqueObjectName(OwnerActor, USceneComponent::StaticClass(), TEXT("PokemonSummonTarget")));
	TargetComponent->ComponentTags.Add(PokemonSummonTargetTag);
	TargetComponent->SetMobility(EComponentMobility::Movable);
	TargetComponent->SetupAttachment(RootComponent);
	OwnerActor->AddInstanceComponent(TargetComponent);
	TargetComponent->RegisterComponent();
	TargetComponent->SetWorldLocation(MeshComponent->Bounds.Origin);

	// Target의 Scale에는 원래 Bounds 반지름을 보관한다. 메시가 작아진 뒤에도
	// 종마다 Niagara 크기를 원래 몸집에 맞춰 계산하기 위한 실행 중 캐시다.
	const float BoundsScale = FMath::Max(MeshComponent->Bounds.SphereRadius / 100.0f, 0.35f);
	TargetComponent->SetRelativeScale3D(FVector(BoundsScale));
}

void UUEPokemonSummonEffectComponent::DestroyLightTrail()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	TInlineComponentArray<UActorComponent*> Components;
	OwnerActor->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (Component
			&& (Component->ComponentHasTag(PokemonSummonTrailTag)
				|| Component->ComponentHasTag(PokemonSummonOrbTag)
				|| Component->ComponentHasTag(PokemonSummonTargetTag)))
		{
			Component->DestroyComponent();
		}
	}
}

void UUEPokemonSummonEffectComponent::UpdateLightTrail(
	float TravelProgress,
	bool bSpawning,
	bool bVisible) const
{
	AActor* OwnerActor = GetOwner();
	USplineMeshComponent* TrailComponent = FindTaggedComponent<USplineMeshComponent>(OwnerActor, PokemonSummonTrailTag);
	UStaticMeshComponent* OrbComponent = FindTaggedComponent<UStaticMeshComponent>(OwnerActor, PokemonSummonOrbTag);
	if (!TrailComponent || !OrbComponent)
	{
		return;
	}

	TrailComponent->SetVisibility(bVisible, true);
	OrbComponent->SetVisibility(bVisible, true);
	if (!bVisible)
	{
		return;
	}

	const FVector BodyLocation = GetBodyEffectLocation();
	const FVector PokemonLocation = GetPokemonEffectLocation();
	const AActor* PokemonActor = GetOwner();
	const AActor* BodyActor = EffectBodyActor.IsValid()
		? EffectBodyActor.Get()
		: (PokemonActor ? PokemonActor->GetOwner() : nullptr);
	const FVector BodyForward = BodyActor ? BodyActor->GetActorForwardVector() : FVector::ForwardVector;
	const FVector BodyRight = BodyActor ? BodyActor->GetActorRightVector() : FVector::RightVector;

	// P0/P3는 매 프레임 실제 가슴과 포켓몬 위치를 다시 읽는다.
	// P1/P2는 몸 앞·옆·위로 꺼내 곡선이 캐릭터 몸속이나 포켓몬 뒤에 숨지 않게 만든다.
	const FVector BodyControlPoint = BodyLocation
		+ BodyForward * CurveForwardOffset
		+ BodyRight * CurveSideOffset
		+ FVector::UpVector * (CurveHeight * 0.25f);
	const FVector PokemonControlPoint = PokemonLocation
		+ BodyRight * CurveSideOffset
		+ FVector::UpVector * CurveHeight;

	const FVector P0 = bSpawning ? BodyLocation : PokemonLocation;
	const FVector P1 = bSpawning ? BodyControlPoint : PokemonControlPoint;
	const FVector P2 = bSpawning ? PokemonControlPoint : BodyControlPoint;
	const FVector P3 = bSpawning ? PokemonLocation : BodyLocation;
	const float CurveParameter = FMath::SmoothStep(0.0f, 1.0f, TravelProgress);
	const FVector OrbLocation = EvaluateCubicBezier(P0, P1, P2, P3, CurveParameter);
	OrbComponent->SetWorldLocation(OrbLocation);

	// B(t)의 0~현재 t 구간만 SplineMesh로 그린다. Hermite의 시작/끝 접선을
	// B'(0)*t와 B'(t)*t로 넣으면 현재까지의 3차 베지어 부분 곡선과 정확히 일치한다.
	const FTransform TrailTransform = TrailComponent->GetComponentTransform();
	const FVector LocalStart = TrailTransform.InverseTransformPosition(P0);
	const FVector LocalEnd = TrailTransform.InverseTransformPosition(OrbLocation);
	const FVector StartTangentWorld =
		EvaluateCubicBezierDerivative(P0, P1, P2, P3, 0.0f) * CurveParameter;
	const FVector EndTangentWorld =
		EvaluateCubicBezierDerivative(P0, P1, P2, P3, CurveParameter) * CurveParameter;
	const FVector LocalStartTangent = TrailTransform.InverseTransformVectorNoScale(StartTangentWorld);
	const FVector LocalEndTangent = TrailTransform.InverseTransformVectorNoScale(EndTangentWorld);

	TrailComponent->SetStartAndEnd(LocalStart, LocalStartTangent, LocalEnd, LocalEndTangent, false);
	TrailComponent->SetStartScale(FVector2D(TrailThickness * 0.65f), false);
	TrailComponent->SetEndScale(FVector2D(TrailThickness), true);

	const float Pulse = 0.88f + 0.12f * FMath::Abs(FMath::Sin(TravelProgress * UE_TWO_PI * 3.0f));
	OrbComponent->SetWorldScale3D(FVector(OrbScale * Pulse));
	if (UMaterialInstanceDynamic* TrailMaterial = Cast<UMaterialInstanceDynamic>(TrailComponent->GetMaterial(0)))
	{
		TrailMaterial->SetScalarParameterValue(TEXT("EffectStrength"), TrailGlowStrength * Pulse);
	}
}

const USkeletalMeshComponent* UUEPokemonSummonEffectComponent::FindVisibleBodyMesh(
	const ACharacter* BodyCharacter) const
{
	if (!BodyCharacter)
	{
		return nullptr;
	}

	TInlineComponentArray<USkeletalMeshComponent*> BodyMeshes;
	BodyCharacter->GetComponents(BodyMeshes);
	const USkeletalMeshComponent* BestMesh = nullptr;
	int32 BestScore = MIN_int32;

	for (const USkeletalMeshComponent* BodyMesh : BodyMeshes)
	{
		if (!BodyMesh
			|| !BodyMesh->GetSkeletalMeshAsset()
			|| BodyMesh->GetBoneIndex(BodySocketName) == INDEX_NONE)
		{
			continue;
		}

		// 실제로 보이는 몸/의상 메시를 우선한다. 기본 CharacterMesh0가 비어 있는
		// 모듈형 캐릭터에서도 HHVBodyEquipmentMesh의 가슴 뼈를 찾을 수 있다.
		int32 Score = 0;
		Score += BodyMesh->IsVisible() ? 100 : 0;
		Score += BodyMesh->IsRegistered() ? 20 : 0;
		Score += BodyMesh->GetName().Contains(TEXT("Body")) ? 50 : 0;
		Score += BodyMesh == BodyCharacter->GetMesh() ? 10 : 0;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestMesh = BodyMesh;
		}
	}

	return BestMesh;
}

FVector UUEPokemonSummonEffectComponent::GetBodyEffectLocation() const
{
	const AActor* PokemonActor = GetOwner();
	const AActor* BodyActor = EffectBodyActor.IsValid()
		? EffectBodyActor.Get()
		: (PokemonActor ? PokemonActor->GetOwner() : nullptr);
	const ACharacter* BodyCharacter = Cast<ACharacter>(BodyActor);
	const USkeletalMeshComponent* BodyMesh = FindVisibleBodyMesh(BodyCharacter);
	auto GetCameraFacingSurfaceDirection = [BodyActor, BodyCharacter](const FVector& AnchorLocation)
	{
		// 3인칭 카메라에서 가슴 정면(+X)은 몸에 가려질 수 있다.
		// 카메라가 바라보는 표면 방향을 사용하면 어떤 시점에서도 출발점 P0가 화면에 보인다.
		if (BodyCharacter)
		{
			if (const APlayerController* PlayerController = Cast<APlayerController>(BodyCharacter->GetController()))
			{
				if (const APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
				{
					const FVector ToCamera =
						(CameraManager->GetCameraLocation() - AnchorLocation).GetSafeNormal();
					if (!ToCamera.IsNearlyZero())
					{
						return ToCamera;
					}
				}
			}
		}

		return BodyActor ? -BodyActor->GetActorForwardVector() : FVector::BackwardVector;
	};

	// 뼈 중심에서 카메라 쪽 표면으로 조금 꺼내야 빛구슬이 몸 안에 가려지지 않는다.
	if (BodyMesh)
	{
		const FVector ChestLocation = BodyMesh->GetSocketLocation(BodySocketName);
		return ChestLocation
			+ GetCameraFacingSurfaceDirection(ChestLocation) * BodySurfaceCameraOffset;
	}

	if (BodyCharacter && BodyCharacter->GetCapsuleComponent())
	{
		const FVector ChestFallback = BodyCharacter->GetActorLocation()
			+ BodyCharacter->GetActorUpVector()
				* BodyCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.55f;
		return ChestFallback
			+ GetCameraFacingSurfaceDirection(ChestFallback) * BodySurfaceCameraOffset;
	}

	return BodyActor ? BodyActor->GetActorLocation() : GetPokemonEffectLocation();
}

FVector UUEPokemonSummonEffectComponent::GetPokemonEffectLocation() const
{
	AActor* OwnerActor = GetOwner();
	if (const USceneComponent* TargetComponent =
		FindTaggedComponent<USceneComponent>(OwnerActor, PokemonSummonTargetTag))
	{
		return TargetComponent->GetComponentLocation();
	}

	if (const ACharacter* PokemonCharacter = Cast<ACharacter>(OwnerActor))
	{
		if (const USkeletalMeshComponent* MeshComponent = PokemonCharacter->GetMesh())
		{
			return MeshComponent->Bounds.Origin;
		}
	}

	return OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
}

void UUEPokemonSummonEffectComponent::SpawnBurst(const USkeletalMeshComponent* MeshComponent) const
{
	UNiagaraSystem* LoadedBurstSystem = SummonBurstSystem.LoadSynchronous();
	if (!LoadedBurstSystem || !MeshComponent || !GetWorld())
	{
		return;
	}

	float BoundsScale = FMath::Max(MeshComponent->Bounds.SphereRadius / 100.0f, 0.35f);
	if (const USceneComponent* TargetComponent =
		FindTaggedComponent<USceneComponent>(GetOwner(), PokemonSummonTargetTag))
	{
		BoundsScale = FMath::Max(TargetComponent->GetRelativeScale3D().X, 0.35f);
	}

	// Point Light는 전혀 만들지 않는다. Unlit/Additive 입자만 화면에 잠깐 보인다.
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		LoadedBurstSystem,
		GetPokemonEffectLocation(),
		FRotator::ZeroRotator,
		FVector(BoundsScale * BurstScaleMultiplier),
		true,
		true,
		ENCPoolMethod::AutoRelease,
		true);
}
