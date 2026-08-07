// Fill out your copyright notice in the Description page of Project Settings.

#include "HHVPathfinder.h"

#include "HHVServerMapRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <unordered_map>

namespace
{
	struct GridCoordHash
	{
		std::size_t operator()(const HHV::Map::AStarPathfinder::GridCoord& Coord) const
		{
			const std::uint64_t X = static_cast<std::uint32_t>(Coord.X);
			const std::uint64_t Y = static_cast<std::uint32_t>(Coord.Y);
			return static_cast<std::size_t>((X << 32) ^ Y);
		}
	};

	struct OpenNode
	{
		HHV::Map::AStarPathfinder::GridCoord Coord;
		float Score = 0.0f;

		bool operator<(const OpenNode& Other) const
		{
			return Score > Other.Score;
		}
	};

	float Distance2D(const HHV::Map::Vec3& A, const HHV::Map::Vec3& B)
	{
		const float DeltaX = A.X - B.X;
		const float DeltaY = A.Y - B.Y;
		return std::sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
	}
}

namespace HHV::Map
{
	bool AStarPathfinder::GridCoord::operator==(const GridCoord& Other) const
	{
		return X == Other.X && Y == Other.Y;
	}

	PathResult AStarPathfinder::FindPath(const ServerMapRuntime& Map, const PathRequest& Request) const
	{
		PathResult Result;
		if (!Map.IsLoaded())
		{
			return Result;
		}

		const HeightMap& HeightMapData = Map.GetHeightMap();
		if (HeightMapData.CellSize <= 0.0f || HeightMapData.Width <= 0 || HeightMapData.Height <= 0)
		{
			return Result;
		}

		Vec3 GroundedStart;
		Vec3 GroundedGoal;
		if (!Map.IsWalkableLocation(Request.Start, Request.Agent, &GroundedStart) || !Map.IsWalkableLocation(Request.Goal, Request.Agent, &GroundedGoal))
		{
			return Result;
		}

		const GridCoord Start = ToGrid(HeightMapData, GroundedStart);
		const GridCoord Goal = ToGrid(HeightMapData, GroundedGoal);

		std::priority_queue<OpenNode> Open;
		std::unordered_map<GridCoord, float, GridCoordHash> CostSoFar;
		std::unordered_map<GridCoord, GridCoord, GridCoordHash> CameFrom;

		Open.push(OpenNode{ Start, 0.0f });
		CostSoFar[Start] = 0.0f;

		const GridCoord Directions[] = {
			{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
			{ 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
		};

		bool bReachedGoal = false;
		int SearchCount = 0;
		while (!Open.empty() && SearchCount++ < Request.Agent.MaxSearchNodes)
		{
			const GridCoord Current = Open.top().Coord;
			Open.pop();

			if (Current == Goal)
			{
				bReachedGoal = true;
				break;
			}

			const Vec3 CurrentWorld = ToWorld(HeightMapData, Current, GroundedStart.Z);
			Vec3 GroundedCurrent;
			if (!Map.IsWalkableLocation(CurrentWorld, Request.Agent, &GroundedCurrent))
			{
				continue;
			}

			for (const GridCoord& Direction : Directions)
			{
				const GridCoord Next{ Current.X + Direction.X, Current.Y + Direction.Y };
				if (Next.X < 0 || Next.Y < 0 || Next.X >= HeightMapData.Width || Next.Y >= HeightMapData.Height)
				{
					continue;
				}

				Vec3 GroundedNext;
				if (!CanStepBetween(Map, GroundedCurrent, ToWorld(HeightMapData, Next, GroundedCurrent.Z), Request.Agent, GroundedNext))
				{
					continue;
				}

				const float MoveCost = Distance2D(GroundedCurrent, GroundedNext);
				const float NewCost = CostSoFar[Current] + MoveCost;
				const auto ExistingCost = CostSoFar.find(Next);
				if (ExistingCost != CostSoFar.end() && NewCost >= ExistingCost->second)
				{
					continue;
				}

				CostSoFar[Next] = NewCost;
				const float Priority = NewCost + Heuristic(Next, Goal) * HeightMapData.CellSize;
				Open.push(OpenNode{ Next, Priority });
				CameFrom[Next] = Current;
			}
		}

		if (!bReachedGoal)
		{
			return Result;
		}

		std::vector<GridCoord> ReversedPath;
		GridCoord Current = Goal;
		ReversedPath.push_back(Current);
		while (!(Current == Start))
		{
			const auto Parent = CameFrom.find(Current);
			if (Parent == CameFrom.end())
			{
				break;
			}

			Current = Parent->second;
			ReversedPath.push_back(Current);
		}

		std::reverse(ReversedPath.begin(), ReversedPath.end());
		Result.Points.reserve(ReversedPath.size() + 1);
		for (const GridCoord& Coord : ReversedPath)
		{
			Vec3 GroundedPoint;
			if (Map.IsWalkableLocation(ToWorld(HeightMapData, Coord, GroundedStart.Z), Request.Agent, &GroundedPoint))
			{
				Result.Points.push_back(GroundedPoint);
			}
		}

		if (Result.Points.empty() || Distance2D(Result.Points.back(), GroundedGoal) > Request.Agent.CapsuleRadius)
		{
			Result.Points.push_back(GroundedGoal);
		}

		Result.bFound = true;
		return Result;
	}

	AStarPathfinder::GridCoord AStarPathfinder::ToGrid(const HeightMap& HeightMapData, const Vec3& Location) const
	{
		return GridCoord{
			static_cast<int>(std::floor((Location.X - HeightMapData.Origin.X) / HeightMapData.CellSize)),
			static_cast<int>(std::floor((Location.Y - HeightMapData.Origin.Y) / HeightMapData.CellSize))
		};
	}

	Vec3 AStarPathfinder::ToWorld(const HeightMap& HeightMapData, const GridCoord& Coord, float FallbackZ) const
	{
		return Vec3{
			HeightMapData.Origin.X + (static_cast<float>(Coord.X) + 0.5f) * HeightMapData.CellSize,
			HeightMapData.Origin.Y + (static_cast<float>(Coord.Y) + 0.5f) * HeightMapData.CellSize,
			FallbackZ
		};
	}

	float AStarPathfinder::Heuristic(const GridCoord& A, const GridCoord& B) const
	{
		const float DeltaX = static_cast<float>(A.X - B.X);
		const float DeltaY = static_cast<float>(A.Y - B.Y);
		return std::sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
	}

	bool AStarPathfinder::CanStepBetween(const ServerMapRuntime& Map, const Vec3& From, const Vec3& To, const AgentSettings& Agent, Vec3& OutGroundedTo) const
	{
		if (!Map.IsWalkableLocation(To, Agent, &OutGroundedTo))
		{
			return false;
		}

		const float FromFloorZ = From.Z - Agent.CapsuleHalfHeight;
		const float ToFloorZ = OutGroundedTo.Z - Agent.CapsuleHalfHeight;
		return std::abs(ToFloorZ - FromFloorZ) <= Agent.MaxStepHeight;
	}
}
