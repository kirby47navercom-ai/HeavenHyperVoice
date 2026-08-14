// Fill out your copyright notice in the Description page of Project Settings.

#include "PokemonFollowOwnerAction.h"

#include "../Map/HHVPathfinder.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float FollowPi = 3.14159265358979323846f;

	float FollowDegreesToRadians(float Degrees)
	{
		return Degrees * FollowPi / 180.0f;
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
		if (Distance2D(Context.PokemonLocation, Context.OwnerLocation) >= Settings.TeleportDistance)
		{
			return MakeTeleportCommand(CalculateOwnerTeleportTarget(Context));
		}

		if (!Context.ServerMap || !Context.ServerMap->IsLoaded())
		{
			const HHV::Map::Vec3 RightFrontTarget = CalculateOffsetTarget(Context, 1.0f);
			return Distance2D(Context.PokemonLocation, RightFrontTarget) <= Settings.StopDistance
				? MakeArrivedCommand(Context, RightFrontTarget)
				: MakeDirectMoveCommand(RightFrontTarget);
		}

		Command MoveCommand;
		if (TryMakeMoveCommandForTarget(Context, CalculateOffsetTarget(Context, 1.0f), MoveCommand))
		{
			return MoveCommand;
		}

		if (TryMakeMoveCommandForTarget(Context, CalculateOffsetTarget(Context, -1.0f), MoveCommand))
		{
			return MoveCommand;
		}

		if (TryMakeFallbackMoveCommand(Context, MoveCommand))
		{
			return MoveCommand;
		}

		// If every ranked target fails pathfinding, the server snaps the companion back to the owner.
		return MakeTeleportCommand(CalculateOwnerTeleportTarget(Context));
	}

	Command FollowOwnerAction::MakeStopCommand() const
	{
		Command Result;
		Result.Type = CommandType::Stop;
		return Result;
	}

	Command FollowOwnerAction::MakeFaceOwnerCommand(const CompanionContext& Context) const
	{
		Command Result;
		Result.Type = CommandType::FaceTarget;
		Result.TargetLocation = Context.OwnerLocation;
		return Result;
	}

	Command FollowOwnerAction::MakeArrivedCommand(const CompanionContext& Context, const HHV::Map::Vec3& StableTarget)
	{
		if (!bHasIdleTarget || Distance2D(LastIdleTarget, StableTarget) > Settings.IdleTargetChangeTolerance)
		{
			LastIdleTarget = StableTarget;
			IdleAtTargetSeconds = 0.0f;
			bHasIdleTarget = true;
		}
		else
		{
			IdleAtTargetSeconds += std::max(Context.DeltaSeconds, 0.0f);
		}

		return IdleAtTargetSeconds >= Settings.FaceOwnerDelay
			? MakeFaceOwnerCommand(Context)
			: MakeStopCommand();
	}

	Command FollowOwnerAction::MakeTeleportCommand(const HHV::Map::Vec3& TargetLocation)
	{
		ResetArrivalTimer();

		Command Result;
		Result.Type = CommandType::Teleport;
		Result.TargetLocation = TargetLocation;
		return Result;
	}

	Command FollowOwnerAction::MakeDirectMoveCommand(const HHV::Map::Vec3& TargetLocation)
	{
		ResetArrivalTimer();

		Command Result;
		Result.Type = CommandType::MoveTo;
		Result.TargetLocation = TargetLocation;
		Result.AcceptanceRadius = Settings.MoveAcceptanceRadius;
		return Result;
	}

	bool FollowOwnerAction::TryMakeMoveCommandForTarget(const CompanionContext& Context, const HHV::Map::Vec3& CandidateLocation, Command& OutCommand)
	{
		HHV::Map::Vec3 GroundedTarget;
		if (!Context.ServerMap || !Context.ServerMap->IsWalkableLocation(CandidateLocation, Context.Agent, &GroundedTarget))
		{
			return false;
		}

		if (Distance2D(Context.PokemonLocation, GroundedTarget) <= Settings.StopDistance)
		{
			OutCommand = MakeArrivedCommand(Context, GroundedTarget);
			return true;
		}

		HHV::Map::PathRequest PathRequest;
		PathRequest.Start = Context.PokemonLocation;
		PathRequest.Goal = GroundedTarget;
		PathRequest.Agent = Context.Agent;

		const HHV::Map::AStarPathfinder Pathfinder;
		const HHV::Map::PathResult PathResult = Pathfinder.FindPath(*Context.ServerMap, PathRequest);
		if (!PathResult.bFound || PathResult.Points.empty())
		{
			return false;
		}

		ResetArrivalTimer();

		OutCommand.Type = CommandType::MoveTo;
		OutCommand.TargetLocation = PathResult.Points.size() > 1 ? PathResult.Points[1] : PathResult.Points.back();
		OutCommand.AcceptanceRadius = Settings.MoveAcceptanceRadius;
		OutCommand.PathPoints = PathResult.Points;
		return true;
	}

	bool FollowOwnerAction::TryMakeFallbackMoveCommand(const CompanionContext& Context, Command& OutCommand)
	{
		const int CandidateCount = Settings.FallbackCandidateCount > 0 ? Settings.FallbackCandidateCount : 1;
		for (int Index = 0; Index < CandidateCount; ++Index)
		{
			const float Alpha = static_cast<float>(Index) / static_cast<float>(CandidateCount);
			const float Angle = FollowDegreesToRadians(Context.OwnerYawDegrees) + Alpha * FollowPi * 2.0f;
			const HHV::Map::Vec3 Direction = MakeDirectionFromAngle(Angle);
			const HHV::Map::Vec3 Candidate{
				Context.OwnerLocation.X + Direction.X * Settings.FallbackRadius,
				Context.OwnerLocation.Y + Direction.Y * Settings.FallbackRadius,
				Context.OwnerLocation.Z
			};

			if (TryMakeMoveCommandForTarget(Context, Candidate, OutCommand))
			{
				return true;
			}
		}

		return false;
	}

	HHV::Map::Vec3 FollowOwnerAction::CalculateOffsetTarget(const CompanionContext& Context, float SideSign) const
	{
		const float YawRadians = FollowDegreesToRadians(Context.OwnerYawDegrees);
		const HHV::Map::Vec3 Forward{ std::cos(YawRadians), std::sin(YawRadians), 0.0f };
		const HHV::Map::Vec3 Right{ -std::sin(YawRadians), std::cos(YawRadians), 0.0f };

		return HHV::Map::Vec3{
			Context.OwnerLocation.X + Forward.X * Settings.ForwardOffset + Right.X * Settings.RightOffset * SideSign,
			Context.OwnerLocation.Y + Forward.Y * Settings.ForwardOffset + Right.Y * Settings.RightOffset * SideSign,
			Context.OwnerLocation.Z
		};
	}

	HHV::Map::Vec3 FollowOwnerAction::CalculateOwnerTeleportTarget(const CompanionContext& Context) const
	{
		HHV::Map::Vec3 GroundedOwnerLocation;
		if (Context.ServerMap && Context.ServerMap->IsLoaded() && Context.ServerMap->IsWalkableLocation(Context.OwnerLocation, Context.Agent, &GroundedOwnerLocation))
		{
			return GroundedOwnerLocation;
		}

		return Context.OwnerLocation;
	}

	float FollowOwnerAction::Distance2D(const HHV::Map::Vec3& A, const HHV::Map::Vec3& B) const
	{
		const float DeltaX = A.X - B.X;
		const float DeltaY = A.Y - B.Y;
		return std::sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
	}

	void FollowOwnerAction::ResetArrivalTimer()
	{
		IdleAtTargetSeconds = 0.0f;
		bHasIdleTarget = false;
	}
}
