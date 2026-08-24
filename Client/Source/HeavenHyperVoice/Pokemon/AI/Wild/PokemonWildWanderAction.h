// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../Common/PokemonAITypes.h"

#include <cstdint>
#include <random>

namespace HHV::PokemonAI
{
	class WildWanderAction final
	{
	public:
		Command Tick(const WildContext& Context);

		void Reset();
		void SetRandomSeed(std::uint32_t NewRandomSeed);

	private:
		Command MakeStopCommand() const;
		Command MakeFaceTargetCommand(const HHV::Map::Vec3& TargetLocation) const;
		Command MakeMoveCommand(const HHV::Map::Vec3& TargetLocation, const WildContext& Context) const;
		void BeginRandomPause(const WildContext& Context);
		bool TryBuildRandomPath(const WildContext& Context);
		bool TryFindRandomWalkableLocation(const WildContext& Context, HHV::Map::Vec3& OutLocation);
		float Distance2D(const HHV::Map::Vec3& A, const HHV::Map::Vec3& B) const;
		float RandomRange(float MinValue, float MaxValue);

		std::vector<HHV::Map::Vec3> PathPoints;
		std::mt19937 RandomEngine;
		HHV::Map::Vec3 HomeLocation;
		HHV::Map::Vec3 PendingLookTarget;
		int PathIndex = 0;
		float WaitRemainingSeconds = 0.0f;
		float CurrentMoveSpeedScale = 1.0f;
		bool bRandomSeedInitialized = false;
		bool bHasHomeLocation = false;
		bool bHasPendingLookTarget = false;
		bool bHasPendingFieldAnimation = false;
	};
}
