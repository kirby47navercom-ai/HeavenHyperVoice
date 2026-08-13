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

	Command PokemonFSM::Tick(const CompanionContext& Context)
	{
		switch (CurrentMode)
		{
		case CompanionMode::NonCombat:
			return TickNonCombat(Context);
		case CompanionMode::Spawning:
			return SpawnActionInstance ? SpawnActionInstance->Tick(Context) : MakeStopCommand();
		case CompanionMode::Despawning:
			return DespawnActionInstance ? DespawnActionInstance->Tick(Context) : MakeStopCommand();
		case CompanionMode::Combat:
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
