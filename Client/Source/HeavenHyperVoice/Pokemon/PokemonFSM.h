// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PokemonAITypes.h"

#include <memory>

namespace HHV::PokemonAI
{
	class IPokemonAIAction;

	class PokemonFSM
	{
	public:
		PokemonFSM();
		~PokemonFSM();

		Command Tick(const CompanionContext& Context);

		void SetMode(CompanionMode NewMode);
		CompanionMode GetMode() const;

	private:
		Command TickNonCombat(const CompanionContext& Context) const;
		Command MakeStopCommand() const;

		CompanionMode CurrentMode = CompanionMode::NonCombat;
		std::unique_ptr<IPokemonAIAction> SpawnActionInstance;
		std::unique_ptr<IPokemonAIAction> DespawnActionInstance;
		std::unique_ptr<IPokemonAIAction> FollowOwnerActionInstance;
	};
}
