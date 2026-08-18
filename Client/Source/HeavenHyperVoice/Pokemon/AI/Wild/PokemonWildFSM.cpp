// Fill out your copyright notice in the Description page of Project Settings.

#include "PokemonWildFSM.h"

namespace HHV::PokemonAI
{
	Command PokemonWildFSM::Tick(const WildContext& Context)
	{
		switch (CurrentMode)
		{
		case WildMode::Wander:
			return WanderActionInstance.Tick(Context);
		case WildMode::Combat:
		case WildMode::Downed:
		default:
			return MakeStopCommand();
		}
	}

	void PokemonWildFSM::SetMode(WildMode NewMode)
	{
		if (CurrentMode == NewMode)
		{
			return;
		}

		CurrentMode = NewMode;
		Reset();
	}

	WildMode PokemonWildFSM::GetMode() const
	{
		return CurrentMode;
	}

	void PokemonWildFSM::SetRandomSeed(std::uint32_t NewRandomSeed)
	{
		WanderActionInstance.SetRandomSeed(NewRandomSeed);
	}

	void PokemonWildFSM::Reset()
	{
		WanderActionInstance.Reset();
	}

	Command PokemonWildFSM::MakeStopCommand() const
	{
		Command Result;
		Result.Type = CommandType::Stop;
		return Result;
	}
}
