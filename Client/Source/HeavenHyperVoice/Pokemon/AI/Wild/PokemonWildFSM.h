// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../Common/PokemonAITypes.h"
#include "PokemonWildWanderAction.h"

#include <cstdint>

namespace HHV::PokemonAI
{
	class PokemonWildFSM
	{
	public:
		Command Tick(const WildContext& Context);

		void SetMode(WildMode NewMode);
		WildMode GetMode() const;
		void SetRandomSeed(std::uint32_t NewRandomSeed);
		void Reset();

	private:
		Command MakeStopCommand() const;

		WildMode CurrentMode = WildMode::Wander;
		WildWanderAction WanderActionInstance;
	};
}
