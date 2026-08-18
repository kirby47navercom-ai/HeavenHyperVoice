// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../Common/PokemonAIAction.h"

namespace HHV::PokemonAI
{
	struct SpawnSettings
	{
		float ForwardOffset = 150.0f;
		float RightOffset = 150.0f;
		float MinDistanceScale = 0.0f;
		float FallbackRadius = 230.0f;
		int CandidateCount = 8;
		int FallbackCandidateCount = 16;
	};

	class SpawnAction final : public IPokemonAIAction
	{
	public:
		virtual RequestedAction GetRequestType() const override;
		virtual Command Tick(const OwnContext& Context) override;

	private:
		void BuildCandidateLocations(const OwnContext& Context, std::vector<HHV::Map::Vec3>& OutCandidates) const;
		void AppendOffsetCandidate(const OwnContext& Context, float SideSign, float DistanceScale, std::vector<HHV::Map::Vec3>& OutCandidates) const;
		void AppendOffsetCandidates(const OwnContext& Context, float SideSign, std::vector<HHV::Map::Vec3>& OutCandidates) const;
		void AppendFallbackCandidates(const OwnContext& Context, std::vector<HHV::Map::Vec3>& OutCandidates) const;
		bool TryProjectCandidate(const OwnContext& Context, const HHV::Map::Vec3& CandidateLocation, HHV::Map::Vec3& OutLocation) const;
		HHV::Map::Vec3 CalculateTargetOffset(const OwnContext& Context, float SideSign) const;

		SpawnSettings Settings;
	};
}
