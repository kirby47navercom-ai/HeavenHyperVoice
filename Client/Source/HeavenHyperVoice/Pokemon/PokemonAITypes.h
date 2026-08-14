// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../Map/HHVMapTypes.h"
#include "../Map/HHVServerMapRuntime.h"

#include <cstdint>
#include <vector>

namespace HHV::PokemonAI
{
	enum class CompanionMode : std::uint8_t
	{
		NonCombat,
		Combat,
		Spawning,
		Despawning,
		Attacking,
		Downed
	};

	enum class RequestedAction : std::uint8_t
	{
		None,
		FollowOwner,
		Spawn,
		Despawn,
		Attack
	};

	enum class CommandType : std::uint8_t
	{
		None,
		Stop,
		MoveTo,
		Teleport,
		FaceTarget,
		Spawn,
		Despawn,
		Attack
	};

	struct CompanionContext
	{
		float DeltaSeconds = 0.0f;
		HHV::Map::Vec3 PokemonLocation;
		HHV::Map::Vec3 OwnerLocation;
		float OwnerYawDegrees = 0.0f;
		RequestedAction ActionRequest = RequestedAction::FollowOwner;
		const HHV::Map::ServerMapRuntime* ServerMap = nullptr;
		HHV::Map::AgentSettings Agent;
	};

	struct Command
	{
		CommandType Type = CommandType::None;
		HHV::Map::Vec3 TargetLocation;
		float AcceptanceRadius = 0.0f;
		std::vector<HHV::Map::Vec3> PathPoints;
	};
}
