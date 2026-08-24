// Fill out your copyright notice in the Description page of Project Settings.

#include "PokemonWildWanderAction.h"

#include "../../../Map/HHVPathfinder.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float WanderPi = 3.14159265358979323846f;
}

namespace HHV::PokemonAI
{
	Command WildWanderAction::Tick(const WildContext& Context)
	{
		if (!Context.ServerMap || !Context.ServerMap->IsLoaded())
		{
			return MakeStopCommand();
		}

		// 최초 위치를 이 포켓몬의 생활권 중심으로 기억한다.
		// 이후 목적지는 이 위치에서 WanderHomeRadius보다 멀어지지 않게 선택한다.
		if (!bHasHomeLocation)
		{
			HomeLocation = Context.PokemonLocation;
			bHasHomeLocation = true;
		}

		if (PathPoints.empty() || PathIndex >= static_cast<int>(PathPoints.size()))
		{
			if (WaitRemainingSeconds > 0.0f)
			{
				WaitRemainingSeconds = std::max(0.0f, WaitRemainingSeconds - std::max(Context.DeltaSeconds, 0.0f));

				// 필드 행동은 목적지에 도착해 멈춘 직후 한 번만 요청한다.
				// 실제로 어떤 동작을 쓸지는 종별 DataAsset을 아는 언리얼 서버 컴포넌트가 결정한다.
				if (bHasPendingFieldAnimation)
				{
					bHasPendingFieldAnimation = false;
					Command Result;
					Result.Type = CommandType::PlayFieldAnimation;
					return Result;
				}

				// 쉬기 시작할 때 한 번만 임의의 방향을 바라본다.
				// 이후에는 Stop을 보내므로 같은 자리에서 계속 회전하지 않는다.
				if (bHasPendingLookTarget)
				{
					bHasPendingLookTarget = false;
					return MakeFaceTargetCommand(PendingLookTarget);
				}

				return MakeStopCommand();
			}

			if (!TryBuildRandomPath(Context))
			{
				// 갈 수 있는 목적지를 찾지 못했을 때 매 틱마다 A*를 반복하지 않고 잠시 쉰 뒤 다시 찾는다.
				BeginRandomPause(Context);
				return MakeStopCommand();
			}
		}

		while (PathIndex < static_cast<int>(PathPoints.size())
			&& Distance2D(Context.PokemonLocation, PathPoints[static_cast<std::size_t>(PathIndex)]) <= std::max(Context.WanderAcceptanceRadius, 1.0f))
		{
			++PathIndex;
		}

		if (PathIndex >= static_cast<int>(PathPoints.size()))
		{
			BeginRandomPause(Context);
			return MakeStopCommand();
		}

		return MakeMoveCommand(PathPoints[static_cast<std::size_t>(PathIndex)], Context);
	}

	void WildWanderAction::Reset()
	{
		PathPoints.clear();
		PathIndex = 0;
		WaitRemainingSeconds = 0.0f;
		CurrentMoveSpeedScale = 1.0f;
		HomeLocation = HHV::Map::Vec3{};
		PendingLookTarget = HHV::Map::Vec3{};
		bHasHomeLocation = false;
		bHasPendingLookTarget = false;
		bHasPendingFieldAnimation = false;
	}

	void WildWanderAction::SetRandomSeed(std::uint32_t NewRandomSeed)
	{
		RandomEngine.seed(NewRandomSeed);
		bRandomSeedInitialized = true;
		Reset();
	}

	Command WildWanderAction::MakeStopCommand() const
	{
		Command Result;
		Result.Type = CommandType::Stop;
		return Result;
	}

	Command WildWanderAction::MakeFaceTargetCommand(const HHV::Map::Vec3& TargetLocation) const
	{
		Command Result;
		Result.Type = CommandType::FaceTarget;
		Result.TargetLocation = TargetLocation;
		return Result;
	}

	Command WildWanderAction::MakeMoveCommand(const HHV::Map::Vec3& TargetLocation, const WildContext& Context) const
	{
		Command Result;
		Result.Type = CommandType::MoveTo;
		Result.TargetLocation = TargetLocation;
		Result.AcceptanceRadius = std::max(Context.WanderAcceptanceRadius, 1.0f);
		Result.MoveSpeedScale = CurrentMoveSpeedScale;
		Result.PathPoints = PathPoints;
		return Result;
	}

	void WildWanderAction::BeginRandomPause(const WildContext& Context)
	{
		PathPoints.clear();
		PathIndex = 0;

		// 휴식 시간이 매번 같으면 기계적으로 보이므로 기본값에서 변화 폭만큼 무작위로 정한다.
		const float WaitVariation = std::max(Context.WanderWaitVariationSeconds, 0.0f);
		const float MinWaitSeconds = std::max(Context.WanderWaitSeconds - WaitVariation, 0.0f);
		const float MaxWaitSeconds = std::max(Context.WanderWaitSeconds + WaitVariation, MinWaitSeconds);
		WaitRemainingSeconds = RandomRange(MinWaitSeconds, MaxWaitSeconds);

		// 매번 행동하면 산만해지므로 설정된 확률에 따라 이번 휴식의 필드 행동 여부를 정한다.
		const float FieldAnimationChance = std::clamp(Context.WanderFieldAnimationChance, 0.0f, 1.0f);
		bHasPendingFieldAnimation = RandomRange(0.0f, 1.0f) < FieldAnimationChance;

		bHasPendingLookTarget = false;
		const float LookAroundChance = std::clamp(Context.WanderLookAroundChance, 0.0f, 1.0f);
		if (RandomRange(0.0f, 1.0f) < LookAroundChance)
		{
			// 실제 목적지가 아니라 고개를 돌릴 방향만 필요하므로 현재 위치 주변의 짧은 지점을 만든다.
			const float LookAngle = RandomRange(0.0f, WanderPi * 2.0f);
			const float LookDistance = std::max(Context.WanderAcceptanceRadius * 2.0f, 100.0f);
			PendingLookTarget = HHV::Map::Vec3{
				Context.PokemonLocation.X + std::cos(LookAngle) * LookDistance,
				Context.PokemonLocation.Y + std::sin(LookAngle) * LookDistance,
				Context.PokemonLocation.Z
			};
			bHasPendingLookTarget = true;
		}
	}

	bool WildWanderAction::TryBuildRandomPath(const WildContext& Context)
	{
		if (!Context.ServerMap || !Context.ServerMap->IsLoaded())
		{
			return false;
		}

		if (!bRandomSeedInitialized)
		{
			SetRandomSeed(20260818u);
		}

		const HHV::Map::AStarPathfinder Pathfinder;
		const int MaxAttempts = std::max(Context.WanderSearchAttempts, 1);
		for (int AttemptIndex = 0; AttemptIndex < MaxAttempts; ++AttemptIndex)
		{
			HHV::Map::Vec3 TargetLocation;
			if (!TryFindRandomWalkableLocation(Context, TargetLocation))
			{
				continue;
			}

			HHV::Map::PathRequest Request;
			Request.Start = Context.PokemonLocation;
			Request.Goal = TargetLocation;
			Request.Agent = Context.Agent;

			const HHV::Map::PathResult Result = Pathfinder.FindPath(*Context.ServerMap, Request);
			if (!Result.bFound || Result.Points.empty())
			{
				continue;
			}

			PathPoints = Result.Points;
			PathIndex = 0;

			// 같은 포켓몬도 산책마다 천천히 걷거나 빠르게 달리도록 이번 경로의 속도를 한 번 정한다.
			const float MinSpeedScale = std::max(std::min(Context.WanderMinMoveSpeedScale, Context.WanderMaxMoveSpeedScale), 0.1f);
			const float MaxSpeedScale = std::max(std::max(Context.WanderMinMoveSpeedScale, Context.WanderMaxMoveSpeedScale), MinSpeedScale);
			CurrentMoveSpeedScale = RandomRange(MinSpeedScale, MaxSpeedScale);

			while (PathIndex < static_cast<int>(PathPoints.size())
				&& Distance2D(Context.PokemonLocation, PathPoints[static_cast<std::size_t>(PathIndex)]) <= std::max(Context.WanderAcceptanceRadius, 1.0f))
			{
				++PathIndex;
			}

			if (PathIndex < static_cast<int>(PathPoints.size()))
			{
				return true;
			}
		}

		Reset();
		return false;
	}

	bool WildWanderAction::TryFindRandomWalkableLocation(const WildContext& Context, HHV::Map::Vec3& OutLocation)
	{
		if (!Context.ServerMap || !Context.ServerMap->IsLoaded())
		{
			return false;
		}

		if (!bHasHomeLocation)
		{
			HomeLocation = Context.PokemonLocation;
			bHasHomeLocation = true;
		}

		// 맵 전체에서 아무 셀이나 고르지 않고 현재 위치 주변에서 다음 산책 지점을 만든다.
		const float MinDistance = std::max(std::min(Context.WanderMinDistance, Context.WanderMaxDistance), 0.0f);
		const float MaxDistance = std::max(std::max(Context.WanderMinDistance, Context.WanderMaxDistance), MinDistance);
		const float MoveAngle = RandomRange(0.0f, WanderPi * 2.0f);
		const float MoveDistance = RandomRange(MinDistance, MaxDistance);
		HHV::Map::Vec3 Candidate{
			Context.PokemonLocation.X + std::cos(MoveAngle) * MoveDistance,
			Context.PokemonLocation.Y + std::sin(MoveAngle) * MoveDistance,
			Context.PokemonLocation.Z
		};

		// 후보가 생활권을 벗어나면 중심에서 활동 반경 안쪽으로 당겨서 장거리 이탈을 막는다.
		const float HomeRadius = std::max(Context.WanderHomeRadius, 0.0f);
		const float DeltaHomeX = Candidate.X - HomeLocation.X;
		const float DeltaHomeY = Candidate.Y - HomeLocation.Y;
		const float DistanceFromHome = std::sqrt(DeltaHomeX * DeltaHomeX + DeltaHomeY * DeltaHomeY);
		if (HomeRadius > 0.0f && DistanceFromHome > HomeRadius)
		{
			const float SafeHomeRadius = HomeRadius * 0.95f;
			Candidate.X = HomeLocation.X + DeltaHomeX / DistanceFromHome * SafeHomeRadius;
			Candidate.Y = HomeLocation.Y + DeltaHomeY / DistanceFromHome * SafeHomeRadius;
		}

		HHV::Map::Vec3 GroundedLocation;
		if (!Context.ServerMap->IsWalkableLocation(Candidate, Context.Agent, &GroundedLocation))
		{
			return false;
		}

		OutLocation = GroundedLocation;
		return true;
	}

	float WildWanderAction::Distance2D(const HHV::Map::Vec3& A, const HHV::Map::Vec3& B) const
	{
		const float DeltaX = A.X - B.X;
		const float DeltaY = A.Y - B.Y;
		return std::sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
	}

	float WildWanderAction::RandomRange(float MinValue, float MaxValue)
	{
		const float SafeMin = std::min(MinValue, MaxValue);
		const float SafeMax = std::max(MinValue, MaxValue);
		std::uniform_real_distribution<float> Distribution(SafeMin, SafeMax);
		return Distribution(RandomEngine);
	}
}
