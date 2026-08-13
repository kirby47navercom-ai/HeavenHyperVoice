// Fill out your copyright notice in the Description page of Project Settings.

#include "PokemonSpawnAction.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float SpawnPi = 3.14159265358979323846f;

	float SpawnDegreesToRadians(float Degrees)
	{
		return Degrees * SpawnPi / 180.0f;
	}
}

namespace HHV::PokemonAI
{
	RequestedAction SpawnAction::GetRequestType() const
	{
		return RequestedAction::Spawn;
	}

	Command SpawnAction::Tick(const CompanionContext& Context)
	{
		Command Result;
		Result.Type = CommandType::Spawn;

		BuildCandidateLocations(Context, Result.PathPoints);
		if (Result.PathPoints.empty())
		{
			Result.TargetLocation = Context.OwnerLocation;
			Result.PathPoints.push_back(Context.OwnerLocation);
			return Result;
		}

		Result.TargetLocation = Result.PathPoints.front();
		return Result;
	}

	void SpawnAction::BuildCandidateLocations(const CompanionContext& Context, std::vector<HHV::Map::Vec3>& OutCandidates) const
	{
		const HHV::Map::Vec3 TargetOffset = CalculateTargetOffset(Context);
		const int CandidateCount = Settings.CandidateCount > 0 ? Settings.CandidateCount : 1;
		const float MinDistanceScale = std::clamp(Settings.MinDistanceScale, 0.0f, 1.0f);

		OutCandidates.clear();
		OutCandidates.reserve(static_cast<std::size_t>(CandidateCount));

		for (int Index = 0; Index < CandidateCount; ++Index)
		{
			const float Progress = CandidateCount > 1
				? static_cast<float>(Index) / static_cast<float>(CandidateCount - 1)
				: 0.0f;
			const float DistanceScale = 1.0f - (1.0f - MinDistanceScale) * Progress;
			const HHV::Map::Vec3 Candidate{
				Context.OwnerLocation.X + TargetOffset.X * DistanceScale,
				Context.OwnerLocation.Y + TargetOffset.Y * DistanceScale,
				Context.OwnerLocation.Z + TargetOffset.Z * DistanceScale
			};

			HHV::Map::Vec3 ProjectedLocation;
			if (TryProjectCandidate(Context, Candidate, ProjectedLocation))
			{
				OutCandidates.push_back(ProjectedLocation);
			}
		}
	}

	bool SpawnAction::TryProjectCandidate(const CompanionContext& Context, const HHV::Map::Vec3& CandidateLocation, HHV::Map::Vec3& OutLocation) const
	{
		if (Context.ServerMap && Context.ServerMap->IsLoaded())
		{
			return Context.ServerMap->IsWalkableLocation(CandidateLocation, Context.Agent, &OutLocation);
		}

		OutLocation = CandidateLocation;
		return true;
	}

	HHV::Map::Vec3 SpawnAction::CalculateTargetOffset(const CompanionContext& Context) const
	{
		const float YawRadians = SpawnDegreesToRadians(Context.OwnerYawDegrees);
		const HHV::Map::Vec3 Forward{ std::cos(YawRadians), std::sin(YawRadians), 0.0f };
		const HHV::Map::Vec3 Right{ -std::sin(YawRadians), std::cos(YawRadians), 0.0f };

		return HHV::Map::Vec3{
			Forward.X * Settings.ForwardOffset + Right.X * Settings.RightOffset,
			Forward.Y * Settings.ForwardOffset + Right.Y * Settings.RightOffset,
			0.0f
		};
	}
}
