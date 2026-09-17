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
		Buffer.Advance(1.0 / 120.0, Delay, Out, 0.35);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHHVRemotePlaybackRecoveryTest, "HHV.Movement.RemotePlaybackRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHHVRemotePlaybackRecoveryTest::RunTest(const FString &Parameters)
{
	constexpr double Delay = 0.1;
	constexpr double MaxLag = 0.35;
	const FVector Velocity(100.0, 0.0, 70.0);

	const auto AddSample = [&](FUEServerMoveBuffer &Buffer, double Time, bool bReset = false)
	{
		FUEServerMoveSample Sample;
		Sample.Time = Time;
		Sample.Location = Velocity * Time;
		Sample.Velocity = Velocity;
		// A roll in the skipped interval must not keep playing after recovery.
		Sample.CoreState.rollRemaining = Time < 0.5 ? 12 : 0;
		Buffer.Add(Sample, bReset, Delay);
	};

	FUEServerMoveBuffer Buffer;
	FUEServerMoveBuffer LegacyBuffer;
	FUEServerMoveSample Out;
	FUEServerMoveSample LegacyOut;
	AddSample(Buffer, 0.0, true);
	AddSample(LegacyBuffer, 0.0, true);

	// One second of 20Hz snapshots arrives before the next render frame.
	for (int32 Packet = 1; Packet <= 20; ++Packet)
	{
		AddSample(Buffer, Packet * 0.05);
		AddSample(LegacyBuffer, Packet * 0.05);
	}

	TestTrue(TEXT("Burst produces a render sample"), Buffer.Advance(0.0, Delay, Out, MaxLag));
	TestTrue(TEXT("Burst catches up to 100ms behind the latest snapshot"),
	         FMath::Abs(Out.Location.X - 90.0) < 0.001);
	TestTrue(TEXT("Recovery preserves slope interpolation"),
	         FMath::Abs(Out.Location.Z - 63.0) < 0.001);
	TestTrue(TEXT("Skipped roll state is no longer rendered"), Out.CoreState.rollRemaining == 0);

	Buffer.Advance(0.025, Delay, Out, MaxLag);
	TestTrue(TEXT("Playback continues smoothly between retained snapshots"),
	         FMath::Abs(Out.Location.X - 92.5) < 0.001);

	LegacyBuffer.Advance(0.0, Delay, LegacyOut);
	TestEqual(TEXT("Other callers retain their existing playback policy"), LegacyOut.Location.X, 0.0);

	// Mild lag still uses gradual catch-up, without jumping to the newest interval.
	FUEServerMoveBuffer MildLagBuffer;
	AddSample(MildLagBuffer, 0.0, true);
	AddSample(MildLagBuffer, 0.2);
	MildLagBuffer.Advance(0.15, Delay, Out, MaxLag);
	TestTrue(TEXT("Mild lag retains 1.1x catch-up"), FMath::Abs(Out.Location.X - 6.5) < 0.001);

	// A longer burst exceeds the 32-snapshot storage limit as well as the time limit.
	for (int32 Packet = 21; Packet <= 80; ++Packet)
	{
		AddSample(Buffer, Packet * 0.05);
	}
	Buffer.Advance(0.0, Delay, Out, MaxLag);
	TestTrue(TEXT("Storage trimming does not strand the playback clock"),
	         FMath::Abs(Out.Location.X - 390.0) < 0.001);
	TestFalse(TEXT("Late old snapshot remains rejected after recovery"),
	          Buffer.Add({2.0, {}, {}, FQuat::Identity}, false, Delay));

	Buffer.Advance(1.0, Delay, Out, MaxLag);
	TestEqual(TEXT("Missing packets never extrapolate beyond the latest position"), Out.Location.X, 400.0);
	TestTrue(TEXT("Playback stops when received movement runs out"), Out.Velocity.IsNearlyZero());

	AddSample(Buffer, 20.0, true);
	Buffer.Advance(0.0, Delay, Out, MaxLag);
	TestEqual(TEXT("Teleport still clears the previous trajectory"), Out.Location.X, 2000.0);
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
