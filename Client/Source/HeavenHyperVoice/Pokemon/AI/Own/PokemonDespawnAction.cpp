// Fill out your copyright notice in the Description page of Project Settings.

#include "PokemonDespawnAction.h"

namespace HHV::PokemonAI
{
	RequestedAction DespawnAction::GetRequestType() const
	{
		return RequestedAction::Despawn;
	}

	Command DespawnAction::Tick(const OwnContext& Context)
	{
		Command Result;
		Result.Type = CommandType::Despawn;
		Result.TargetLocation = Context.PokemonLocation;
		Result.PathPoints.push_back(Context.PokemonLocation);
		return Result;
	}
}
