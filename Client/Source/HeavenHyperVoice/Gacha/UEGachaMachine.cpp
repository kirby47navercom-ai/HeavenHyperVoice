#include "UEGachaMachine.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AUEGachaMachine::AUEGachaMachine()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("MachineRoot")));
	HandlePivot = CreateDefaultSubobject<USceneComponent>(TEXT("HandlePivot"));
	HandlePivot->SetupAttachment(RootComponent);
	RewardBall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RewardBall"));
	RewardBall->SetupAttachment(RootComponent);
	RewardBall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StageLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StageLight"));
	StageLight->SetupAttachment(RootComponent);
	StageLight->SetCastShadows(false);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
}

void AUEGachaMachine::BeginPlay()
{
	Super::BeginPlay();
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh->ComponentHasTag(TEXT("GachaCapsule")))
		{
			Capsules.Add(Mesh);
			Velocities.Add(FVector::ZeroVector);
		}
		if (Mesh->ComponentHasTag(TEXT("StageGlow")))
			StageMaterials.Add(Mesh->CreateAndSetMaterialInstanceDynamic(0));
	}
	ResetDraw();
}

void AUEGachaMachine::ResetDraw()
{
	if (State == EUEGachaState::Turning || State == EUEGachaState::Dispensing) return;
	State = EUEGachaState::Ready;
	CompletedTurns = 0;
	TurnDegrees = 0;
	DispenseElapsed = 0;
	HandlePivot->SetRelativeRotation(FRotator::ZeroRotator);
	RewardBall->SetVisibility(false);
	StatusMessage = FText::FromString(TEXT("손잡이를 잡고 시계 방향으로 세 바퀴 돌려 주세요"));
	UpdateStage();
}

void AUEGachaMachine::TurnHandle(float Degrees)
{
	if (!CanTurn() || !FMath::IsFinite(Degrees) || FMath::Abs(Degrees) > 45 || FMath::Abs(Degrees) < .01f) return;
	if (State == EUEGachaState::Ready)
	{
		if (Degrees <= 0) return;
		if (!Pool || !Pool->Draw(SelectedEntry))
		{
			StatusMessage = FText::FromString(TEXT("뽑을 수 있는 포켓몬이 없습니다. 데이터 에셋의 가중치를 확인해 주세요."));
			return;
		}
		State = EUEGachaState::Turning;
	}
	// 반대로 돌리면 현재 회전량이 줄어든다. 손잡이를 앞뒤로 흔들어 횟수를 채울 수는 없다.
	TurnDegrees = FMath::Clamp(TurnDegrees + Degrees, CompletedTurns * 360.f, 1080.f);
	// 정면 카메라에서 +Roll은 시계 방향이다. 화면 입력과 같은 방향으로 회전한다.
	HandlePivot->SetRelativeRotation(FRotator(0, 0, TurnDegrees));
	for (int32 I = 0; I < Capsules.Num(); ++I)
	{
		if (!Capsules[I].IsValid()) continue;
		const FVector P = Capsules[I]->GetRelativeLocation() - ChamberCenter;
		Velocities[I] += FVector(FMath::Sin(I * 2.4f) * 2, -P.Z * .05f, P.Y * .05f + 2.2f) * Degrees;
		Velocities[I] = Velocities[I].GetClampedToMaxSize(320);
	}
	const int32 Turns = FMath::Min(3, FMath::FloorToInt(TurnDegrees / 360.f));
	if (Turns > CompletedTurns)
	{
		CompletedTurns = Turns;
		Pulse = 1;
		UpdateStage();
	}
	if (CompletedTurns == 3)
	{
		State = EUEGachaState::Dispensing;
		RewardBall->SetStaticMesh(BallMeshes.IsValidIndex(static_cast<int32>(SelectedEntry.Rarity)) ? BallMeshes[static_cast<int32>(SelectedEntry.Rarity)] : nullptr);
		StatusMessage = FText::FromString(TEXT("캡슐이 나오고 있어요…"));
	}
}

void AUEGachaMachine::UpdateStage()
{
	RevealColor = FLinearColor(.025f, .3f, 1.f);
	if (CompletedTurns >= 2 && SelectedEntry.Rarity != EUEGachaRarity::Normal && State != EUEGachaState::Ready)
		RevealColor = FLinearColor(.55f, .04f, 1.f);
	if (CompletedTurns >= 3 && SelectedEntry.Rarity == EUEGachaRarity::SuperRare)
		RevealColor = FLinearColor(1.f, .55f, .025f);
	StageLight->SetLightColor(RevealColor);
	if (State == EUEGachaState::Turning)
		StatusMessage = FText::FromString(FString::Printf(TEXT("%d / 3 바퀴 · 손잡이를 계속 돌려 주세요"), CompletedTurns));
}

void AUEGachaMachine::SimulateCapsules(float Seconds)
{
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Seconds * 120), 1, 8);
	const float Dt = FMath::Min(Seconds, .066f) / Steps;
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		for (int32 I = 0; I < Capsules.Num(); ++I)
		{
			if (!Capsules[I].IsValid()) continue;
			FVector P = Capsules[I]->GetRelativeLocation();
			Velocities[I].Z -= 180 * Dt;
			Velocities[I] *= FMath::Exp(-.65f * Dt);
			P += Velocities[I] * Dt;
			FVector Radial = P - ChamberCenter;
			if (Radial.SizeSquared() > FMath::Square(ChamberRadius - CapsuleRadius))
			{
				const FVector N = Radial.GetSafeNormal();
				P = ChamberCenter + N * (ChamberRadius - CapsuleRadius);
				const float Outward = FVector::DotProduct(Velocities[I], N);
				if (Outward > 0) Velocities[I] -= 1.4f * Outward * N;
			}
			for (int32 J = 0; J < I; ++J)
			{
				if (!Capsules[J].IsValid()) continue;
				const FVector Other = Capsules[J]->GetRelativeLocation();
				const FVector Delta = P - Other;
				const float Distance = Delta.Size();
				if (Distance >= CapsuleRadius * 2) continue;
				const FVector N = Distance > .001f ? Delta / Distance : FVector::UpVector;
				const FVector Offset = N * (CapsuleRadius * 2 - Distance) * .5f;
				P += Offset;
				Capsules[J]->SetRelativeLocation(Other - Offset);
				const float Closing = FVector::DotProduct(Velocities[I] - Velocities[J], N);
				if (Closing < 0) { Velocities[I] -= N * Closing * .65f; Velocities[J] += N * Closing * .65f; }
			}
			Capsules[I]->SetRelativeLocation(P);
			Capsules[I]->AddLocalRotation(FRotator(Velocities[I].Y, Velocities[I].X, Velocities[I].Z) * Dt);
		}
	}
}

void AUEGachaMachine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SimulateCapsules(DeltaSeconds);
	Pulse = FMath::Max(0.f, Pulse - DeltaSeconds * 1.5f);
	for (UMaterialInstanceDynamic* Material : StageMaterials) if (Material) Material->SetVectorParameterValue(TEXT("Tint"), RevealColor * (2 + Pulse * 5));
	StageLight->SetIntensity(900 + Pulse * 2200);
	if (State != EUEGachaState::Dispensing) return;
	DispenseElapsed += DeltaSeconds;
	const float T = FMath::Clamp((DispenseElapsed - .45f) / 1.15f, 0.f, 1.f);
	RewardBall->SetVisibility(DispenseElapsed >= .45f);
	RewardBall->SetRelativeLocation(FVector(FMath::Lerp(-61.f, -105.f, T), 0, FMath::Lerp(67.f, 40.f, T) + FMath::Abs(FMath::Sin(T * PI * 2)) * (1 - T) * 22));
	RewardBall->SetRelativeRotation(FRotator(T * 540, T * 80, T * 210));
	if (DispenseElapsed >= 2)
	{
		State = EUEGachaState::Result;
		StatusMessage = FText::FromString(TEXT("캡슐을 열었어요!"));
		OnRewardRevealed.Broadcast(SelectedEntry);
	}
}
