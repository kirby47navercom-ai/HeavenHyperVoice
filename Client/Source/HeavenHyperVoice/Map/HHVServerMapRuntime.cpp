// Fill out your copyright notice in the Description page of Project Settings.

#include "HHVServerMapRuntime.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace
{
	constexpr float Pi = 3.14159265358979323846f;
	constexpr float HeightContinuityTolerance = 1.0f;

	float Dot(const HHV::Map::Vec3& A, const HHV::Map::Vec3& B)
	{
		return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
	}

	float Length(const HHV::Map::Vec3& Vector)
	{
		return std::sqrt(Dot(Vector, Vector));
	}

	HHV::Map::Vec3 Subtract(const HHV::Map::Vec3& A, const HHV::Map::Vec3& B)
	{
		return HHV::Map::Vec3{ A.X - B.X, A.Y - B.Y, A.Z - B.Z };
	}

	HHV::Map::Vec3 NormalizeOrDefault(const HHV::Map::Vec3& Vector, const HHV::Map::Vec3& DefaultValue)
	{
		const float VectorLength = Length(Vector);
		if (VectorLength <= 0.0001f)
		{
			return DefaultValue;
		}

		return HHV::Map::Vec3{ Vector.X / VectorLength, Vector.Y / VectorLength, Vector.Z / VectorLength };
	}

	float DegreesToRadians(float Degrees)
	{
		return Degrees * Pi / 180.0f;
	}

	float CosDegrees(float Degrees)
	{
		return std::cos(DegreesToRadians(Degrees));
	}

	float CapsuleSupportOnAxis(const HHV::Map::Vec3& Axis, const HHV::Map::AgentSettings& Agent)
	{
		const float VerticalPart = std::abs(Axis.Z) * Agent.CapsuleHalfHeight;
		const float HorizontalPart = std::sqrt(std::max(0.0f, 1.0f - Axis.Z * Axis.Z)) * Agent.CapsuleRadius;
		return VerticalPart + HorizontalPart;
	}

	void CalculateInterpolationPair(float CenterCoord, int Count, int& OutA, int& OutB, float& OutAlpha)
	{
		if (Count <= 1)
		{
			OutA = 0;
			OutB = 0;
			OutAlpha = 0.0f;
			return;
		}

		const int Base = static_cast<int>(std::floor(CenterCoord));
		if (Base < 0)
		{
			OutA = 0;
			OutB = 0;
			OutAlpha = 0.0f;
			return;
		}
		if (Base >= Count - 1)
		{
			OutA = Count - 1;
			OutB = Count - 1;
			OutAlpha = 0.0f;
			return;
		}

		OutA = Base;
		OutB = Base + 1;
		OutAlpha = CenterCoord - static_cast<float>(Base);
	}
}

namespace HHV::Map
{
	bool ServerMapRuntime::LoadFromFile(const std::string& FilePath)
	{
		std::ifstream File(FilePath);
		if (!File.is_open())
		{
			bLoaded = false;
			return false;
		}

		SourceMapPackageName.clear();
		WorldBounds = Aabb{};
		GroundHeightMap = HeightMap{};
		GroundObbs.clear();
		WallObbs.clear();

		std::string Line;
		while (std::getline(File, Line))
		{
			if (Line.empty() || Line[0] == '#')
			{
				continue;
			}

			if (!TryParseLine(Line))
			{
				bLoaded = false;
				return false;
			}
		}

		bLoaded =
			GroundHeightMap.Width > 0 &&
			GroundHeightMap.Height > 0 &&
			!GroundHeightMap.Cells.empty();
		return bLoaded;
	}

	bool ServerMapRuntime::IsLoaded() const
	{
		return bLoaded;
	}

	const HeightMap& ServerMapRuntime::GetHeightMap() const
	{
		return GroundHeightMap;
	}

	const std::vector<Obb>& ServerMapRuntime::GetGroundObbs() const
	{
		return GroundObbs;
	}

	const std::vector<Obb>& ServerMapRuntime::GetWallObbs() const
	{
		return WallObbs;
	}

	FloorSample ServerMapRuntime::SampleFloor(float WorldX, float WorldY, float WalkableFloorAngleDegrees) const
	{
		FloorSample Result;
		if (!bLoaded || GroundHeightMap.CellSize <= 0.0f)
		{
			return Result;
		}

		const float MapX = (WorldX - GroundHeightMap.Origin.X) / GroundHeightMap.CellSize;
		const float MapY = (WorldY - GroundHeightMap.Origin.Y) / GroundHeightMap.CellSize;
		if (MapX < 0.0f || MapY < 0.0f || MapX >= static_cast<float>(GroundHeightMap.Width) || MapY >= static_cast<float>(GroundHeightMap.Height))
		{
			return Result;
		}

		const int ContainingCellX = static_cast<int>(std::floor(MapX));
		const int ContainingCellY = static_cast<int>(std::floor(MapY));
		HeightCell ContainingCell;
		if (!TryGetCell(ContainingCellX, ContainingCellY, ContainingCell) || !ContainingCell.bHasGround)
		{
			return Result;
		}

		int CellX0 = 0;
		int CellX1 = 0;
		int CellY0 = 0;
		int CellY1 = 0;
		float AlphaX = 0.0f;
		float AlphaY = 0.0f;

		CalculateInterpolationPair(MapX - 0.5f, GroundHeightMap.Width, CellX0, CellX1, AlphaX);
		CalculateInterpolationPair(MapY - 0.5f, GroundHeightMap.Height, CellY0, CellY1, AlphaY);

		struct WeightedCell
		{
			int X = 0;
			int Y = 0;
			float Weight = 0.0f;
			HeightCell Cell;
		};

		WeightedCell WeightedCells[] = {
			{ CellX0, CellY0, (1.0f - AlphaX) * (1.0f - AlphaY), HeightCell{} },
			{ CellX1, CellY0, AlphaX * (1.0f - AlphaY), HeightCell{} },
			{ CellX0, CellY1, (1.0f - AlphaX) * AlphaY, HeightCell{} },
			{ CellX1, CellY1, AlphaX * AlphaY, HeightCell{} }
		};

		for (WeightedCell& Weighted : WeightedCells)
		{
			if (Weighted.Weight <= 0.0001f)
			{
				continue;
			}

			if (!TryGetCell(Weighted.X, Weighted.Y, Weighted.Cell) || !Weighted.Cell.bHasGround || !IsWalkableNormal(Weighted.Cell.Normal, WalkableFloorAngleDegrees))
			{
				Result.bHit = ContainingCell.bHasGround;
				Result.bWalkable = IsWalkableNormal(ContainingCell.Normal, WalkableFloorAngleDegrees);
				Result.FloorZ = ContainingCell.Height;
				Result.Normal = ContainingCell.Normal;
				return Result;
			}
		}

		const auto IsUsed = [](const WeightedCell& Weighted)
		{
			return Weighted.Weight > 0.0001f;
		};

		const bool bCanInterpolate =
			(!IsUsed(WeightedCells[0]) || !IsUsed(WeightedCells[1]) || CanInterpolateBetweenCells(WeightedCells[0].Cell, WeightedCells[1].Cell, GroundHeightMap.CellSize, WalkableFloorAngleDegrees)) &&
			(!IsUsed(WeightedCells[2]) || !IsUsed(WeightedCells[3]) || CanInterpolateBetweenCells(WeightedCells[2].Cell, WeightedCells[3].Cell, GroundHeightMap.CellSize, WalkableFloorAngleDegrees)) &&
			(!IsUsed(WeightedCells[0]) || !IsUsed(WeightedCells[2]) || CanInterpolateBetweenCells(WeightedCells[0].Cell, WeightedCells[2].Cell, GroundHeightMap.CellSize, WalkableFloorAngleDegrees)) &&
			(!IsUsed(WeightedCells[1]) || !IsUsed(WeightedCells[3]) || CanInterpolateBetweenCells(WeightedCells[1].Cell, WeightedCells[3].Cell, GroundHeightMap.CellSize, WalkableFloorAngleDegrees));

		if (!bCanInterpolate)
		{
			Result.bHit = ContainingCell.bHasGround;
			Result.bWalkable = IsWalkableNormal(ContainingCell.Normal, WalkableFloorAngleDegrees);
			Result.FloorZ = ContainingCell.Height;
			Result.Normal = ContainingCell.Normal;
			return Result;
		}

		float TotalWeight = 0.0f;
		float SampledHeight = 0.0f;
		Vec3 SampledNormal{};
		for (const WeightedCell& Weighted : WeightedCells)
		{
			if (Weighted.Weight <= 0.0f)
			{
				continue;
			}

			TotalWeight += Weighted.Weight;
			SampledHeight += Weighted.Cell.Height * Weighted.Weight;
			SampledNormal.X += Weighted.Cell.Normal.X * Weighted.Weight;
			SampledNormal.Y += Weighted.Cell.Normal.Y * Weighted.Weight;
			SampledNormal.Z += Weighted.Cell.Normal.Z * Weighted.Weight;
		}

		if (TotalWeight <= 0.0001f)
		{
			return Result;
		}

		Result.bHit = true;
		Result.FloorZ = SampledHeight / TotalWeight;
		Result.Normal = NormalizeOrDefault(
			Vec3{ SampledNormal.X / TotalWeight, SampledNormal.Y / TotalWeight, SampledNormal.Z / TotalWeight },
			ContainingCell.Normal
		);
		Result.bWalkable = IsWalkableNormal(Result.Normal, WalkableFloorAngleDegrees);
		return Result;
	}

	bool ServerMapRuntime::IsWalkableLocation(const Vec3& Location, const AgentSettings& Agent, Vec3* OutGroundedLocation) const
	{
		const FloorSample Floor = SampleFloor(Location.X, Location.Y, Agent.WalkableFloorAngleDegrees);
		if (!Floor.bWalkable)
		{
			return false;
		}

		const Vec3 GroundedLocation{ Location.X, Location.Y, Floor.FloorZ + Agent.CapsuleHalfHeight };
		if (IsBlockedByWall(GroundedLocation, Agent))
		{
			return false;
		}

		if (OutGroundedLocation)
		{
			*OutGroundedLocation = GroundedLocation;
		}
		return true;
	}

	bool ServerMapRuntime::IsBlockedByWall(const Vec3& CapsuleLocation, const AgentSettings& Agent) const
	{
		for (const Obb& Wall : WallObbs)
		{
			const Vec3 Delta = Subtract(CapsuleLocation, Wall.Center);
			const float LocalX = Dot(Delta, Wall.AxisX);
			const float LocalY = Dot(Delta, Wall.AxisY);
			const float LocalZ = Dot(Delta, Wall.AxisZ);

			const float ExpandX = CapsuleSupportOnAxis(Wall.AxisX, Agent);
			const float ExpandY = CapsuleSupportOnAxis(Wall.AxisY, Agent);
			const float ExpandZ = CapsuleSupportOnAxis(Wall.AxisZ, Agent);

			if (
				std::abs(LocalX) <= Wall.HalfExtent.X + ExpandX &&
				std::abs(LocalY) <= Wall.HalfExtent.Y + ExpandY &&
				std::abs(LocalZ) <= Wall.HalfExtent.Z + ExpandZ
			)
			{
				return true;
			}
		}

		return false;
	}

	bool ServerMapRuntime::TryGetCell(int CellX, int CellY, HeightCell& OutCell) const
	{
		if (
			CellX < 0 ||
			CellY < 0 ||
			CellX >= GroundHeightMap.Width ||
			CellY >= GroundHeightMap.Height ||
			GroundHeightMap.Cells.empty()
		)
		{
			return false;
		}

		OutCell = GroundHeightMap.Cells[static_cast<std::size_t>(CellY * GroundHeightMap.Width + CellX)];
		return true;
	}

	bool ServerMapRuntime::TryParseLine(const std::string& Line)
	{
		std::istringstream Stream(Line);
		std::string Type;
		Stream >> Type;

		if (Type == "source")
		{
			Stream >> SourceMapPackageName;
			return true;
		}

		if (Type == "bounds")
		{
			Stream
				>> WorldBounds.Min.X >> WorldBounds.Min.Y >> WorldBounds.Min.Z
				>> WorldBounds.Max.X >> WorldBounds.Max.Y >> WorldBounds.Max.Z;
			return !Stream.fail();
		}

		if (Type == "heightmap")
		{
			Stream >> GroundHeightMap.Origin.X >> GroundHeightMap.Origin.Y >> GroundHeightMap.CellSize >> GroundHeightMap.Width >> GroundHeightMap.Height;
			if (GroundHeightMap.Width > 0 && GroundHeightMap.Height > 0)
			{
				GroundHeightMap.Cells.assign(static_cast<std::size_t>(GroundHeightMap.Width * GroundHeightMap.Height), HeightCell{});
			}
			return !Stream.fail();
		}

		if (Type == "height_cell")
		{
			int CellX = 0;
			int CellY = 0;
			HeightCell Cell;
			Stream >> CellX >> CellY >> Cell.Height >> Cell.Normal.X >> Cell.Normal.Y >> Cell.Normal.Z;
			if (Stream.fail() || CellX < 0 || CellY < 0 || CellX >= GroundHeightMap.Width || CellY >= GroundHeightMap.Height || GroundHeightMap.Cells.empty())
			{
				return false;
			}

			Cell.bHasGround = true;
			Cell.Normal = NormalizeOrDefault(Cell.Normal, Vec3{ 0.0f, 0.0f, 1.0f });
			GroundHeightMap.Cells[static_cast<std::size_t>(CellY * GroundHeightMap.Width + CellX)] = Cell;
			return true;
		}

		if (Type == "ground_obb" || Type == "wall_obb")
		{
			Obb ParsedObb;
			Stream
				>> ParsedObb.ProfileName
				>> ParsedObb.Center.X >> ParsedObb.Center.Y >> ParsedObb.Center.Z
				>> ParsedObb.HalfExtent.X >> ParsedObb.HalfExtent.Y >> ParsedObb.HalfExtent.Z
				>> ParsedObb.AxisX.X >> ParsedObb.AxisX.Y >> ParsedObb.AxisX.Z
				>> ParsedObb.AxisY.X >> ParsedObb.AxisY.Y >> ParsedObb.AxisY.Z
				>> ParsedObb.AxisZ.X >> ParsedObb.AxisZ.Y >> ParsedObb.AxisZ.Z;
			if (Stream.fail())
			{
				return false;
			}

			ParsedObb.AxisX = NormalizeOrDefault(ParsedObb.AxisX, Vec3{ 1.0f, 0.0f, 0.0f });
			ParsedObb.AxisY = NormalizeOrDefault(ParsedObb.AxisY, Vec3{ 0.0f, 1.0f, 0.0f });
			ParsedObb.AxisZ = NormalizeOrDefault(ParsedObb.AxisZ, Vec3{ 0.0f, 0.0f, 1.0f });

			if (Type == "ground_obb")
			{
				GroundObbs.push_back(ParsedObb);
			}
			else
			{
				WallObbs.push_back(ParsedObb);
			}
			return true;
		}

		return true;
	}

	bool ServerMapRuntime::IsWalkableNormal(const Vec3& Normal, float WalkableFloorAngleDegrees) const
	{
		return Normal.Z >= CosDegrees(WalkableFloorAngleDegrees);
	}

	bool ServerMapRuntime::CanInterpolateBetweenCells(const HeightCell& A, const HeightCell& B, float HorizontalDistance, float WalkableFloorAngleDegrees) const
	{
		if (!A.bHasGround || !B.bHasGround || !IsWalkableNormal(A.Normal, WalkableFloorAngleDegrees) || !IsWalkableNormal(B.Normal, WalkableFloorAngleDegrees))
		{
			return false;
		}

		const float ClampedAngle = std::clamp(WalkableFloorAngleDegrees, 0.0f, 89.0f);
		const float MaxWalkableHeightDelta =
			std::tan(DegreesToRadians(ClampedAngle)) * HorizontalDistance +
			HeightContinuityTolerance;

		return std::abs(A.Height - B.Height) <= MaxWalkableHeightDelta;
	}
}
