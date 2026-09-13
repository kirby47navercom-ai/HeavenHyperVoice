#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Net/UEServerMoveBuffer.h"
#include "../Character/UEPlayerCharacter.h"
#include "Engine/World.h"
#include "../Pokemon/UEPokemonCharacter.h"
#include "../Movement/UECoreMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHHVSnapshotSlopeTest, "HHV.Movement.SnapshotSlopeAndJitter",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHHVSnapshotSlopeTest::RunTest(const FString &Parameters)
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
			TestTrue(TEXT("Slope does not reduce horizontal speed"),
			         FMath::Abs(Out.Velocity.X - 100.0) < 0.1);
			TestTrue(TEXT("Vertical coordinate uses the same sample time"),
			         FMath::Abs(Out.Location.Z - Out.Location.X * 0.7) < 0.01);
		}
		LastX = Out.Location.X;
	}
	const FVector BeforeOldPacket = Out.Location;
	TestFalse(TEXT("Old packet is rejected"), Buffer.Add({1.1, {}, {}, FQuat::Identity}, false, Delay));
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Buffer.Advance(1.0 / 120.0, Delay, Out);
	}
	TestTrue(TEXT("No extrapolation beyond server position"),
	         Out.Location.X <= (NextPacket - 1) * 5.0 + 0.01);
	TestTrue(TEXT("Stopped at last validated sample"), Out.Velocity.IsNearlyZero());
	TestTrue(TEXT("No backwards correction"), Out.Location.X >= BeforeOldPacket.X);
	Buffer.Add({20.0, FVector(1000, 0, 0), {}, FQuat::Identity}, true, Delay);
	Buffer.Advance(1.0 / 60.0, Delay, Out);
	TestEqual(TEXT("Teleport resets old samples"), Out.Location.X, 1000.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHHVPokemonCoreTest, "HHV.Movement.PokemonUsesCore",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHHVPokemonCoreTest::RunTest(const FString &Parameters)
{
	const auto *Pokemon = GetDefault<AUEPokemonCharacter>();
	TestNotNull(TEXT("Pokemon owns shared core movement"), Pokemon->GetCoreMovement());
	TestEqual(TEXT("Pawn movement adapter"), Pokemon->GetMovementComponent(),
	          static_cast<UPawnMovementComponent *>(Pokemon->GetCoreMovement()));
	return true;
}

#endif
