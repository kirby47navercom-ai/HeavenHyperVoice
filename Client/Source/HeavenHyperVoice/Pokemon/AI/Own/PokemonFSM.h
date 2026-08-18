// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../Common/PokemonAITypes.h"

#include <memory>

namespace HHV::PokemonAI
{
	class IPokemonAIAction;

	class PokemonFSM
	{
	public:
		PokemonFSM();
		~PokemonFSM();

		Command Tick(const OwnContext& Context);

		void SetMode(OwnMode NewMode);
		OwnMode GetMode() const;

	private:
		Command TickNonCombat(const OwnContext& Context) const;
		Command MakeStopCommand() const;

		OwnMode CurrentMode = OwnMode::NonCombat;
		std::unique_ptr<IPokemonAIAction> SpawnActionInstance;
		std::unique_ptr<IPokemonAIAction> DespawnActionInstance;
		std::unique_ptr<IPokemonAIAction> FollowOwnerActionInstance;
	};
}
