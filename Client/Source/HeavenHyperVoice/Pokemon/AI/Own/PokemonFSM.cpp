// Fill out your copyright notice in the Description page of Project Settings.

#include "PokemonFSM.h"

#include "PokemonDespawnAction.h"
#include "PokemonFollowOwnerAction.h"
#include "PokemonSpawnAction.h"

namespace HHV::PokemonAI
{
	PokemonFSM::PokemonFSM()
		: SpawnActionInstance(std::make_unique<SpawnAction>())
		, DespawnActionInstance(std::make_unique<DespawnAction>())
		, FollowOwnerActionInstance(std::make_unique<FollowOwnerAction>())
	{
	}

	PokemonFSM::~PokemonFSM() = default;

	Command PokemonFSM::Tick(const OwnContext& Context)
	{
		switch (CurrentMode)
		{
		case OwnMode::NonCombat:
			return TickNonCombat(Context);
		case OwnMode::Spawning:
			return SpawnActionInstance ? SpawnActionInstance->Tick(Context) : MakeStopCommand();
		case OwnMode::Despawning:
			return DespawnActionInstance ? DespawnActionInstance->Tick(Context) : MakeStopCommand();
		case OwnMode::Combat:
		case OwnMode::Attacking:
		case OwnMode::Downed:
			return MakeStopCommand();
		default:
			return MakeStopCommand();
		}
	}

	void PokemonFSM::SetMode(OwnMode NewMode)
	{
		CurrentMode = NewMode;
	}

	OwnMode PokemonFSM::GetMode() const
	{
		return CurrentMode;
	}

	Command PokemonFSM::TickNonCombat(const OwnContext& Context) const
	{
		if (Context.ActionRequest == RequestedAction::Spawn && SpawnActionInstance)
		{
			return SpawnActionInstance->Tick(Context);
		}

		if (Context.ActionRequest == RequestedAction::Despawn && DespawnActionInstance)
		{
			return DespawnActionInstance->Tick(Context);
		}

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
