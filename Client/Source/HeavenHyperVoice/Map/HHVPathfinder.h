// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "HHVMapTypes.h"

namespace HHV::Map
{
	class ServerMapRuntime;

	class AStarPathfinder
	{
	public:
		struct GridCoord
		{
			int X = 0;
			int Y = 0;

			bool operator==(const GridCoord& Other) const;
		};

		PathResult FindPath(const ServerMapRuntime& Map, const PathRequest& Request) const;

	private:
		GridCoord ToGrid(const HeightMap& HeightMapData, const Vec3& Location) const;
		Vec3 ToWorld(const HeightMap& HeightMapData, const GridCoord& Coord, float FallbackZ) const;
		float Heuristic(const GridCoord& A, const GridCoord& B) const;
		bool CanStepBetween(const ServerMapRuntime& Map, const Vec3& From, const Vec3& To, const AgentSettings& Agent, Vec3& OutGroundedTo) const;
	};
}
