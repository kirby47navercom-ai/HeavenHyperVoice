// Fill out your copyright notice in the Description page of Project Settings.

#include "PokemonWildWanderAction.h"

#include "../../../Map/HHVPathfinder.h"

#include <algorithm>
#include <cmath>

namespace HHV::PokemonAI
{
	Command WildWanderAction::Tick(const WildContext& Context)
	{
		if (!Context.ServerMap || !Context.ServerMap->IsLoaded())
		{
			return MakeStopCommand();
		}

		if (PathPoints.empty() || PathIndex >= static_cast<int>(PathPoints.size()))
		{
			if (WaitRemainingSeconds > 0.0f)
			{
				WaitRemainingSeconds = std::max(0.0f, WaitRemainingSeconds - std::max(Context.DeltaSeconds, 0.0f));
				return MakeStopCommand();
			}

			if (!TryBuildRandomPath(Context))
			{
				WaitRemainingSeconds = std::max(Context.WanderWaitSeconds, 0.0f);
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
			WaitRemainingSeconds = std::max(Context.WanderWaitSeconds, 0.0f);
			return MakeStopCommand();
		}

		return MakeMoveCommand(PathPoints[static_cast<std::size_t>(PathIndex)], Context);
	}

	void WildWanderAction::Reset()
	{
		PathPoints.clear();
		PathIndex = 0;
		WaitRemainingSeconds = 0.0f;
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

	Command WildWanderAction::MakeMoveCommand(const HHV::Map::Vec3& TargetLocation, const WildContext& Context) const
	{
		Command Result;
		Result.Type = CommandType::MoveTo;
		Result.TargetLocation = TargetLocation;
		Result.AcceptanceRadius = std::max(Context.WanderAcceptanceRadius, 1.0f);
		Result.PathPoints = PathPoints;
		return Result;
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

		const HHV::Map::HeightMap& HeightMap = Context.ServerMap->GetHeightMap();
		if (HeightMap.Width <= 0 || HeightMap.Height <= 0 || HeightMap.CellSize <= 0.0f || HeightMap.Cells.empty())
		{
			return false;
		}

		std::uniform_int_distribution<int> CellXDistribution(0, HeightMap.Width - 1);
		std::uniform_int_distribution<int> CellYDistribution(0, HeightMap.Height - 1);
		std::uniform_real_distribution<float> UnitDistribution(0.0f, 1.0f);

		const int MaxAttempts = std::max(Context.WanderSearchAttempts, 1);
		for (int AttemptIndex = 0; AttemptIndex < MaxAttempts; ++AttemptIndex)
		{
			const int CellX = CellXDistribution(RandomEngine);
			const int CellY = CellYDistribution(RandomEngine);
			const int CellIndex = CellY * HeightMap.Width + CellX;
			if (CellIndex < 0 || CellIndex >= static_cast<int>(HeightMap.Cells.size()) || !HeightMap.Cells[static_cast<std::size_t>(CellIndex)].bHasGround)
			{
				continue;
			}

			const HHV::Map::Vec3 Candidate{
				HeightMap.Origin.X + (static_cast<float>(CellX) + UnitDistribution(RandomEngine)) * HeightMap.CellSize,
				HeightMap.Origin.Y + (static_cast<float>(CellY) + UnitDistribution(RandomEngine)) * HeightMap.CellSize,
				HeightMap.Cells[static_cast<std::size_t>(CellIndex)].Height + Context.Agent.CapsuleHalfHeight
			};

			HHV::Map::Vec3 GroundedLocation;
			if (Context.ServerMap->IsWalkableLocation(Candidate, Context.Agent, &GroundedLocation))
			{
				OutLocation = GroundedLocation;
				return true;
			}
		}

		return false;
	}

	float WildWanderAction::Distance2D(const HHV::Map::Vec3& A, const HHV::Map::Vec3& B) const
	{
		const float DeltaX = A.X - B.X;
		const float DeltaY = A.Y - B.Y;
		return std::sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
	}
}
