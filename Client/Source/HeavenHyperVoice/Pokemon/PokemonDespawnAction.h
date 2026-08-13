// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PokemonAIAction.h"

namespace HHV::PokemonAI
{
	class DespawnAction final : public IPokemonAIAction
	{
	public:
		virtual RequestedAction GetRequestType() const override;
		virtual Command Tick(const CompanionContext& Context) override;
	};
}
