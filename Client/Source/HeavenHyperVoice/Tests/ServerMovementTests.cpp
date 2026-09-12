#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Net/UEServerMoveBuffer.h"
#include "../Character/UEPlayerCharacter.h"
#include "../Server/UEPlayerMovementSyncComponent.h"
#include "Engine/World.h"
#include "../Movement/UECoreMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHHVSnapshotSlopeTest, "HHV.Movement.SnapshotSlopeAndJitter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHHVSnapshotSlopeTest::RunTest(const FString& Parameters)
{
	FUEServerMoveBuffer Buffer;
	FUEServerMoveSample Out;
	constexpr double Delay = 0.1;
	const FVector Velocity(100, 0, 70);
	Buffer.Add({1.0, FVector::ZeroVector, Velocity, FQuat::Identity}, true, Delay);
	int32 NextPacket = 1;
	double LastX = 0.0;
	for (int32 Frame = 1; Frame <= 600; ++Frame)
	{
		const double Time = Frame / 120.0;
		while (NextPacket * 0.05 + (NextPacket % 3) * 0.01 <= Time)
		{
			const double SampleTime = NextPacket * 0.05;
			Buffer.Add({1.0 + SampleTime, Velocity * SampleTime, Velocity, FQuat::Identity}, false, Delay);
			++NextPacket;
		}
		Buffer.Advance(1.0 / 120.0, Delay, Out);
		if (Frame > 60)
		{
			TestTrue(TEXT("No walking/idle oscillation under 20ms jitter"), Out.Location.X > LastX);
			TestTrue(TEXT("Slope does not reduce horizontal speed"), FMath::Abs(Out.Velocity.X - 100.0) < 0.1);
			TestTrue(TEXT("Vertical coordinate uses the same sample time"), FMath::Abs(Out.Location.Z - Out.Location.X * 0.7) < 0.01);
		}
		LastX = Out.Location.X;
	}
	const FVector BeforeOldPacket = Out.Location;
	TestFalse(TEXT("Old packet is rejected"), Buffer.Add({1.1, {}, {}, FQuat::Identity}, false, Delay));
	for (int32 Frame = 0; Frame < 120; ++Frame) Buffer.Advance(1.0 / 120.0, Delay, Out);
	TestTrue(TEXT("No extrapolation beyond server position"), Out.Location.X <= (NextPacket - 1) * 5.0 + 0.01);
	TestTrue(TEXT("Stopped at last validated sample"), Out.Velocity.IsNearlyZero());
	TestTrue(TEXT("No backwards correction"), Out.Location.X >= BeforeOldPacket.X);
	Buffer.Add({20.0, FVector(1000, 0, 0), {}, FQuat::Identity}, true, Delay);
	Buffer.Advance(1.0 / 60.0, Delay, Out);
	TestEqual(TEXT("Teleport resets old samples"), Out.Location.X, 1000.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHHVPlayerCorrectionTest, "HHV.Movement.PlayerCorrection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHHVPlayerCorrectionTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Settings = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("HHVMovementTestWorld")),
		nullptr, true, ERHIFeatureLevel::Num, &Settings);
	AUEPlayerCharacter* Player = World->SpawnActor<AUEPlayerCharacter>();
	UUEPlayerMovementSyncComponent* Sync = Player->FindComponentByClass<UUEPlayerMovementSyncComponent>();
	if (!TestNotNull(TEXT("Player movement sync component"), Sync))
	{
		World->DestroyWorld(false);
		return false;
	}
	Player->SetActorLocation(FVector(100, 0, 500));
	const auto First = Sync->CaptureMovementPacket();
	Player->SetActorLocation(FVector(110, 0, 505));
	const auto Second = Sync->CaptureMovementPacket();
	Player->SetActorLocation(FVector(120, 0, 510));
	Player->SetActorRotation(FRotator(0, 90, 0));
	Player->GetCoreMovement()->Velocity = FVector(100, 0, -30);
	Sync->HandleServerCorrection(First.Sequence, FVector(80, 0, -12000), 0);
	TestEqual(TEXT("Retain movement since acknowledged packet"), Player->GetActorLocation().X, 100.0);
	TestEqual(TEXT("Do not force grounded Z onto falling character"), Player->GetActorLocation().Z, 510.0);
	TestEqual(TEXT("Preserve velocity"), Player->GetVelocity(), FVector(100, 0, -30));
	TestTrue(TEXT("Do not undo newer facing input"), FMath::IsNearlyEqual(Player->GetActorRotation().Yaw, 90.0));
	Sync->HandleServerCorrection(Second.Sequence, FVector(90, 0, -12000), 0);
	TestEqual(TEXT("Do not apply the same correction twice"), Player->GetActorLocation().X, 100.0);
	const auto Third = Sync->CaptureMovementPacket();
	Sync->HandleServerCorrection(Third.Sequence, FVector(100, 0, -12000), 0);
	TestEqual(TEXT("Z-only discrepancy does not interrupt movement"), Player->GetActorLocation().Z, 510.0);
	World->DestroyWorld(false);
	return true;
}

#endif
