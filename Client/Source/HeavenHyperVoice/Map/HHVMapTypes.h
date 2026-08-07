// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace HHV::Map
{
	struct Vec3
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Z = 0.0f;
	};

	struct Aabb
	{
		Vec3 Min;
		Vec3 Max;
	};

	struct Obb
	{
		std::string ProfileName;
		Vec3 Center;
		Vec3 HalfExtent;
		Vec3 AxisX{ 1.0f, 0.0f, 0.0f };
		Vec3 AxisY{ 0.0f, 1.0f, 0.0f };
		Vec3 AxisZ{ 0.0f, 0.0f, 1.0f };
	};

	struct HeightCell
	{
		bool bHasGround = false;
		float Height = 0.0f;
		Vec3 Normal{ 0.0f, 0.0f, 1.0f };
	};

	struct HeightMap
	{
		Vec3 Origin;
		float CellSize = 50.0f;
		int Width = 0;
		int Height = 0;
		std::vector<HeightCell> Cells;
	};

	struct FloorSample
	{
		bool bHit = false;
		bool bWalkable = false;
		float FloorZ = 0.0f;
		Vec3 Normal{ 0.0f, 0.0f, 1.0f };
	};

	struct AgentSettings
	{
		float CapsuleRadius = 34.0f;
		float CapsuleHalfHeight = 88.0f;
		float MaxStepHeight = 45.0f;
		float WalkableFloorAngleDegrees = 55.0f;
		float FloorSnapDistance = 75.0f;
		int MaxSearchNodes = 4096;
	};

	struct PathRequest
	{
		Vec3 Start;
		Vec3 Goal;
		AgentSettings Agent;
	};

	struct PathResult
	{
		bool bFound = false;
		std::vector<Vec3> Points;
	};
}
