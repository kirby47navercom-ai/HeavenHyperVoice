// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../Common/PokemonAIAction.h"

namespace HHV::PokemonAI
{
	struct FollowOwnerSettings
	{
		float ForwardOffset = 150.0f;
		float RightOffset = 150.0f;
		float FallbackRadius = 230.0f;
		float TeleportDistance = 900.0f;
		float StopDistance = 45.0f;
		float MoveAcceptanceRadius = 35.0f;
		float FaceOwnerDelay = 1.0f;
		float IdleTargetChangeTolerance = 25.0f;
		int FallbackCandidateCount = 16;
	};

	class FollowOwnerAction final : public IPokemonAIAction
	{
	public:
		virtual RequestedAction GetRequestType() const override;
		virtual Command Tick(const OwnContext& Context) override;

	private:
		Command MakeStopCommand() const;
		Command MakeFaceOwnerCommand(const OwnContext& Context) const;
		Command MakeArrivedCommand(const OwnContext& Context, const HHV::Map::Vec3& StableTarget);
		Command MakeTeleportCommand(const HHV::Map::Vec3& TargetLocation);
		Command MakeDirectMoveCommand(const HHV::Map::Vec3& TargetLocation);
		bool TryMakeMoveCommandForTarget(const OwnContext& Context, const HHV::Map::Vec3& CandidateLocation, Command& OutCommand);
		bool TryMakeFallbackMoveCommand(const OwnContext& Context, Command& OutCommand);
		HHV::Map::Vec3 CalculateOffsetTarget(const OwnContext& Context, float SideSign) const;
		HHV::Map::Vec3 CalculateOwnerTeleportTarget(const OwnContext& Context) const;
		float Distance2D(const HHV::Map::Vec3& A, const HHV::Map::Vec3& B) const;
		void ResetArrivalTimer();

		FollowOwnerSettings Settings;
		HHV::Map::Vec3 LastIdleTarget;
		float IdleAtTargetSeconds = 0.0f;
		bool bHasIdleTarget = false;
	};
}
