// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../../../Map/HHVMapTypes.h"
#include "../../../Map/HHVServerMapRuntime.h"

#include <cstdint>
#include <vector>

namespace HHV::PokemonAI
{
	enum class OwnMode : std::uint8_t
	{
		NonCombat,
		Combat,
		Spawning,
		Despawning,
		Attacking,
		Downed
	};

	enum class WildMode : std::uint8_t
	{
		Wander,
		Combat,
		Downed
	};

	enum class RequestedAction : std::uint8_t
	{
		None,
		FollowOwner,
		Spawn,
		Despawn,
		Attack
	};

	enum class CommandType : std::uint8_t
	{
		None,
		Stop,
		MoveTo,
		Teleport,
		FaceTarget,
		PlayFieldAnimation,
		Spawn,
		Despawn,
		Attack
	};

	struct OwnContext
	{
		float DeltaSeconds = 0.0f;
		HHV::Map::Vec3 PokemonLocation;
		HHV::Map::Vec3 OwnerLocation;
		float OwnerYawDegrees = 0.0f;
		RequestedAction ActionRequest = RequestedAction::FollowOwner;
		const HHV::Map::ServerMapRuntime* ServerMap = nullptr;
		HHV::Map::AgentSettings Agent;
	};

	struct WildContext
	{
		float DeltaSeconds = 0.0f;
		HHV::Map::Vec3 PokemonLocation;
		const HHV::Map::ServerMapRuntime* ServerMap = nullptr;
		HHV::Map::AgentSettings Agent;

		// 목적지에 도착한 뒤 쉬는 기본 시간과 무작위 변화 폭이다.
		float WanderWaitSeconds = 3.0f;
		float WanderWaitVariationSeconds = 2.0f;

		// 한 번 산책할 때 이동할 거리와 최초 위치를 기준으로 한 활동 반경이다.
		float WanderMinDistance = 200.0f;
		float WanderMaxDistance = 700.0f;
		float WanderHomeRadius = 1200.0f;

		// 산책마다 걷기와 달리기가 섞이도록 종별 최고 속도에 곱하는 범위다.
		float WanderMinMoveSpeedScale = 0.45f;
		float WanderMaxMoveSpeedScale = 1.0f;

		// 쉬기 시작할 때 주변을 한 번 바라볼 확률이다. 0은 사용 안 함, 1은 항상 사용이다.
		float WanderLookAroundChance = 0.65f;

		// 쉬기 시작할 때 종별 필드 행동 애니메이션을 재생할 확률이다.
		float WanderFieldAnimationChance = 0.75f;
		float WanderAcceptanceRadius = 60.0f;
		int WanderSearchAttempts = 64;
	};

	struct Command
	{
		CommandType Type = CommandType::None;
		HHV::Map::Vec3 TargetLocation;
		float AcceptanceRadius = 0.0f;

		// 종별 기본 이동 속도에 곱할 배율이다. 지정하지 않은 행동은 기존 속도 1배를 사용한다.
		float MoveSpeedScale = 1.0f;
		std::vector<HHV::Map::Vec3> PathPoints;
	};
}
