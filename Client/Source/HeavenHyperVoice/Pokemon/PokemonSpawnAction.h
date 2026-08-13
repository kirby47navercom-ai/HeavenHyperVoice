// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PokemonAIAction.h"

namespace HHV::PokemonAI
{
	struct SpawnSettings
	{
		float ForwardOffset = 150.0f;
		float RightOffset = 150.0f;
		float MinDistanceScale = 0.0f;
		int CandidateCount = 8;
	};

	class SpawnAction final : public IPokemonAIAction
	{
	public:
		virtual RequestedAction GetRequestType() const override;
		virtual Command Tick(const CompanionContext& Context) override;

	private:
		void BuildCandidateLocations(const CompanionContext& Context, std::vector<HHV::Map::Vec3>& OutCandidates) const;
		bool TryProjectCandidate(const CompanionContext& Context, const HHV::Map::Vec3& CandidateLocation, HHV::Map::Vec3& OutLocation) const;
		HHV::Map::Vec3 CalculateTargetOffset(const CompanionContext& Context) const;

		SpawnSettings Settings;
	};
}
