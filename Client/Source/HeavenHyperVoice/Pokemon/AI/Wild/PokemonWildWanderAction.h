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
		Command MakeMoveCommand(const HHV::Map::Vec3& TargetLocation, const WildContext& Context) const;
		bool TryBuildRandomPath(const WildContext& Context);
		bool TryFindRandomWalkableLocation(const WildContext& Context, HHV::Map::Vec3& OutLocation);
		float Distance2D(const HHV::Map::Vec3& A, const HHV::Map::Vec3& B) const;

		std::vector<HHV::Map::Vec3> PathPoints;
		std::mt19937 RandomEngine;
		int PathIndex = 0;
		float WaitRemainingSeconds = 0.0f;
		bool bRandomSeedInitialized = false;
	};
}
