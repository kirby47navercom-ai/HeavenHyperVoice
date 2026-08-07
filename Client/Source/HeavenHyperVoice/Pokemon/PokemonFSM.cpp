// Fill out your copyright notice in the Description page of Project Settings.

#include "PokemonFSM.h"

#include "PokemonFollowOwnerAction.h"

namespace HHV::PokemonAI
{
	PokemonFSM::PokemonFSM()
		: FollowOwnerActionInstance(std::make_unique<FollowOwnerAction>())
	{
	}

	PokemonFSM::~PokemonFSM() = default;

	Command PokemonFSM::Tick(const CompanionContext& Context)
	{
		switch (CurrentMode)
		{
		case CompanionMode::NonCombat:
			return TickNonCombat(Context);
		case CompanionMode::Combat:
		case CompanionMode::Spawning:
		case CompanionMode::Despawning:
		case CompanionMode::Attacking:
		case CompanionMode::Downed:
			return MakeStopCommand();
		default:
			return MakeStopCommand();
		}
	}

	void PokemonFSM::SetMode(CompanionMode NewMode)
	{
		CurrentMode = NewMode;
	}

	CompanionMode PokemonFSM::GetMode() const
	{
		return CurrentMode;
	}

	Command PokemonFSM::TickNonCombat(const CompanionContext& Context) const
	{
		if (Context.ActionRequest == RequestedAction::FollowOwner && FollowOwnerActionInstance)
		{
			return FollowOwnerActionInstance->Tick(Context);
		}

		return MakeStopCommand();
	}

	Command PokemonFSM::MakeStopCommand() const
	{
		Command Result;
		Result.Type = CommandType::Stop;
		return Result;
	}
}
