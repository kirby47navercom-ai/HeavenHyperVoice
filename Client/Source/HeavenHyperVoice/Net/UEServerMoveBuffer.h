#pragma once

#include "CoreMinimal.h"
#include "MovementCore.h"

struct FUEServerMoveSample
{
	double Time = 0.0;
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	hhv::movement::State CoreState;
};

// Delayed interpolation never predicts beyond the last validated server point.
// This keeps packet jitter out of movement without extrapolating through walls.
class FUEServerMoveBuffer
{
  public:
	bool Add(const FUEServerMoveSample &Sample, bool bReset, double Delay)
	{
		if (!FMath::IsFinite(Sample.Time) || Sample.Location.ContainsNaN() || Sample.Velocity.ContainsNaN())
		{
			return false;
		}
		if (!bReset && !Samples.IsEmpty() && Sample.Time <= Samples.Last().Time)
		{
			return false;
		}
		if (bReset || Samples.IsEmpty())
		{
			Samples.Reset();
			PlaybackTime = Sample.Time - Delay;
		}
		else if (Samples.Last().Velocity.IsNearlyZero() &&
		         Sample.Time - Samples.Last().Time > FMath::Max(Delay * 4.0, 0.5))
		{
			// A stationary entity may not have been sent for seconds. Hold its
			// last pose until movement actually resumes instead of smearing it.
			FUEServerMoveSample Hold = Samples.Last();
			Hold.Time = Sample.Time - 0.05;
			Samples.Reset();
			Samples.Add(Hold);
			PlaybackTime = Sample.Time - Delay;
		}
		Samples.Add(Sample);
		if (Samples.Num() > 32)
		{
			Samples.RemoveAt(0, Samples.Num() - 32, EAllowShrinking::No);
		}
		return true;
	}

	bool Advance(double DeltaSeconds, double Delay, FUEServerMoveSample &Out)
	{
		if (Samples.IsEmpty())
		{
			return false;
		}
		// Catch up after a burst, but retain normal playback speed for normal jitter.
		const double ExcessLag = Samples.Last().Time - PlaybackTime - Delay;
		const double Rate = ExcessLag > Delay ? 1.1 : 1.0;
		PlaybackTime += FMath::Max(DeltaSeconds, 0.0) * Rate;
		if (PlaybackTime > Samples.Last().Time)
		{
			PlaybackTime = Samples.Last().Time;
		}
		while (Samples.Num() > 2 && Samples[1].Time <= PlaybackTime)
		{
			Samples.RemoveAt(0, 1, EAllowShrinking::No);
		}
		Out = Samples[0];
		if (Samples.Num() < 2 || PlaybackTime <= Samples[0].Time)
		{
			Out.Velocity = FVector::ZeroVector;
			return true;
		}
		const FUEServerMoveSample &A = Samples[0];
		const FUEServerMoveSample &B = Samples[1];
		const double Duration = B.Time - A.Time;
		const double Alpha = FMath::Clamp((PlaybackTime - A.Time) / Duration, 0.0, 1.0);
		Out.CoreState = Alpha >= 1.0 ? B.CoreState : A.CoreState;
		Out.Location = FMath::Lerp(A.Location, B.Location, Alpha);
		Out.Rotation = FQuat::Slerp(A.Rotation, B.Rotation, Alpha).GetNormalized();
		Out.Velocity =
		    PlaybackTime >= Samples.Last().Time ? FVector::ZeroVector : (B.Location - A.Location) / Duration;
		return true;
	}

	bool IsEmpty() const
	{
		return Samples.IsEmpty();
	}

  private:
	TArray<FUEServerMoveSample> Samples;
	double PlaybackTime = 0.0;
};
