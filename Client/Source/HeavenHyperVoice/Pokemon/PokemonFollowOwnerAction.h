// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PokemonAIAction.h"

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
		int FallbackCandidateCount = 16;
	};

	class FollowOwnerAction final : public IPokemonAIAction
	{
	public:
		virtual RequestedAction GetRequestType() const override;
		virtual Command Tick(const CompanionContext& Context) override;

	private:
		Command MakeStopCommand() const;
		Command MakeTeleportCommand(const HHV::Map::Vec3& TargetLocation) const;
		bool TryMakeMoveCommand(const CompanionContext& Context, const HHV::Map::Vec3& TargetLocation, Command& OutCommand) const;
		bool TryFindWalkableTarget(const CompanionContext& Context, HHV::Map::Vec3& OutTargetLocation) const;
		HHV::Map::Vec3 CalculateOffsetTarget(const CompanionContext& Context, float SideSign) const;
		float Distance2D(const HHV::Map::Vec3& A, const HHV::Map::Vec3& B) const;

		FollowOwnerSettings Settings;
	};
}
