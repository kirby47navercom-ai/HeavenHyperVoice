// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "HHVMapTypes.h"

namespace HHV::Map
{
	class ServerMapRuntime
	{
	public:
		bool LoadFromFile(const std::string& FilePath);
		bool IsLoaded() const;

		const HeightMap& GetHeightMap() const;
		const std::vector<Obb>& GetGroundObbs() const;
		const std::vector<Obb>& GetWallObbs() const;

		FloorSample SampleFloor(float WorldX, float WorldY, float WalkableFloorAngleDegrees) const;
		bool IsWalkableLocation(const Vec3& Location, const AgentSettings& Agent, Vec3* OutGroundedLocation = nullptr) const;
		bool IsBlockedByWall(const Vec3& CapsuleLocation, const AgentSettings& Agent) const;

	private:
		bool TryGetCell(int CellX, int CellY, HeightCell& OutCell) const;
		bool TryParseLine(const std::string& Line);
		bool IsWalkableNormal(const Vec3& Normal, float WalkableFloorAngleDegrees) const;
		bool CanInterpolateBetweenCells(const HeightCell& A, const HeightCell& B, float HorizontalDistance, float WalkableFloorAngleDegrees) const;

		std::string SourceMapPackageName;
		Aabb WorldBounds;
		HeightMap GroundHeightMap;
		std::vector<Obb> GroundObbs;
		std::vector<Obb> WallObbs;
		bool bLoaded = false;
	};
}
