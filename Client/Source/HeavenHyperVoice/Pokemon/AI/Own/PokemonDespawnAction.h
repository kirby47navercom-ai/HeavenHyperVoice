// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../Common/PokemonAIAction.h"

namespace HHV::PokemonAI
{
	class DespawnAction final : public IPokemonAIAction
	{
	public:
		virtual RequestedAction GetRequestType() const override;
		virtual Command Tick(const OwnContext& Context) override;
	};
}
