// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PokemonAITypes.h"

namespace HHV::PokemonAI
{
	class IPokemonAIAction
	{
	public:
		virtual ~IPokemonAIAction() = default;

		virtual RequestedAction GetRequestType() const = 0;
		virtual Command Tick(const OwnContext& Context) = 0;
	};
}
