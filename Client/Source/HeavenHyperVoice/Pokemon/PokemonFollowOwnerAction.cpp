// Fill out your copyright notice in the Description page of Project Settings.

#include "PokemonFollowOwnerAction.h"

#include "../Map/HHVPathfinder.h"

#include <cmath>

namespace
{
	constexpr float Pi = 3.14159265358979323846f;

	float DegreesToRadians(float Degrees)
	{
		return Degrees * Pi / 180.0f;
	}

	HHV::Map::Vec3 MakeDirectionFromAngle(float Radians)
	{
		return HHV::Map::Vec3{ std::cos(Radians), std::sin(Radians), 0.0f };
	}
}

namespace HHV::PokemonAI
{
	RequestedAction FollowOwnerAction::GetRequestType() const
	{
		return RequestedAction::FollowOwner;
	}

	Command FollowOwnerAction::Tick(const CompanionContext& Context)
	{
		HHV::Map::Vec3 DesiredTarget;
		if (!TryFindWalkableTarget(Context, DesiredTarget))
		{
			return MakeStopCommand();
		}

		if (Distance2D(Context.PokemonLocation, DesiredTarget) <= Settings.StopDistance)
		{
			return MakeStopCommand();
		}

		Command MoveCommand;
		if (TryMakeMoveCommand(Context, DesiredTarget, MoveCommand))
		{
			return MoveCommand;
		}

		// If the owner area cannot be reached by pathfinding, the server snaps the companion back near the owner.
		return MakeTeleportCommand(DesiredTarget);
	}

	Command FollowOwnerAction::MakeStopCommand() const
	{
		Command Result;
		Result.Type = CommandType::Stop;
		return Result;
	}

	Command FollowOwnerAction::MakeTeleportCommand(const HHV::Map::Vec3& TargetLocation) const
	{
		Command Result;
		Result.Type = CommandType::Teleport;
		Result.TargetLocation = TargetLocation;
		return Result;
	}

	bool FollowOwnerAction::TryMakeMoveCommand(const CompanionContext& Context, const HHV::Map::Vec3& TargetLocation, Command& OutCommand) const
	{
		if (!Context.ServerMap || !Context.ServerMap->IsLoaded())
		{
			OutCommand.Type = CommandType::MoveTo;
			OutCommand.TargetLocation = TargetLocation;
			OutCommand.AcceptanceRadius = Settings.MoveAcceptanceRadius;
			return true;
		}

		if (Distance2D(Context.PokemonLocation, Context.OwnerLocation) >= Settings.TeleportDistance)
		{
			return false;
		}

		HHV::Map::PathRequest PathRequest;
		PathRequest.Start = Context.PokemonLocation;
		PathRequest.Goal = TargetLocation;
		PathRequest.Agent = Context.Agent;

		const HHV::Map::AStarPathfinder Pathfinder;
		const HHV::Map::PathResult PathResult = Pathfinder.FindPath(*Context.ServerMap, PathRequest);
		if (!PathResult.bFound || PathResult.Points.empty())
		{
			return false;
		}

		OutCommand.Type = CommandType::MoveTo;
		OutCommand.TargetLocation = PathResult.Points.size() > 1 ? PathResult.Points[1] : PathResult.Points.back();
		OutCommand.AcceptanceRadius = Settings.MoveAcceptanceRadius;
		OutCommand.PathPoints = PathResult.Points;
		return true;
	}

	bool FollowOwnerAction::TryFindWalkableTarget(const CompanionContext& Context, HHV::Map::Vec3& OutTargetLocation) const
	{
		const HHV::Map::Vec3 RightFrontTarget = CalculateOffsetTarget(Context, 1.0f);
		if (!Context.ServerMap)
		{
			OutTargetLocation = RightFrontTarget;
			return true;
		}

		if (Context.ServerMap->IsWalkableLocation(RightFrontTarget, Context.Agent, &OutTargetLocation))
		{
			return true;
		}

		const HHV::Map::Vec3 LeftFrontTarget = CalculateOffsetTarget(Context, -1.0f);
		if (Context.ServerMap->IsWalkableLocation(LeftFrontTarget, Context.Agent, &OutTargetLocation))
		{
			return true;
		}

		const int CandidateCount = Settings.FallbackCandidateCount > 0 ? Settings.FallbackCandidateCount : 1;
		for (int Index = 0; Index < CandidateCount; ++Index)
		{
			const float Alpha = static_cast<float>(Index) / static_cast<float>(CandidateCount);
			const float Angle = DegreesToRadians(Context.OwnerYawDegrees) + Alpha * Pi * 2.0f;
			const HHV::Map::Vec3 Direction = MakeDirectionFromAngle(Angle);
			const HHV::Map::Vec3 Candidate{
				Context.OwnerLocation.X + Direction.X * Settings.FallbackRadius,
				Context.OwnerLocation.Y + Direction.Y * Settings.FallbackRadius,
				Context.OwnerLocation.Z
			};

			if (Context.ServerMap->IsWalkableLocation(Candidate, Context.Agent, &OutTargetLocation))
			{
				return true;
			}
		}

		return false;
	}

	HHV::Map::Vec3 FollowOwnerAction::CalculateOffsetTarget(const CompanionContext& Context, float SideSign) const
	{
		const float YawRadians = DegreesToRadians(Context.OwnerYawDegrees);
		const HHV::Map::Vec3 Forward{ std::cos(YawRadians), std::sin(YawRadians), 0.0f };
		const HHV::Map::Vec3 Right{ -std::sin(YawRadians), std::cos(YawRadians), 0.0f };

		return HHV::Map::Vec3{
			Context.OwnerLocation.X + Forward.X * Settings.ForwardOffset + Right.X * Settings.RightOffset * SideSign,
			Context.OwnerLocation.Y + Forward.Y * Settings.ForwardOffset + Right.Y * Settings.RightOffset * SideSign,
			Context.OwnerLocation.Z
		};
	}

	float FollowOwnerAction::Distance2D(const HHV::Map::Vec3& A, const HHV::Map::Vec3& B) const
	{
		const float DeltaX = A.X - B.X;
		const float DeltaY = A.Y - B.Y;
		return std::sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
	}
}
