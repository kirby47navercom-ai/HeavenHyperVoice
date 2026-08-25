// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../Common/PokemonAIAction.h"

namespace HHV::PokemonAI
{
	struct FollowOwnerSettings
	{
		float ForwardOffset = 150.0f;
		float RightOffset = 150.0f;
		float FallbackRadius = 230.0f;
		float TeleportDistance = 900.0f;
		float StopDistance = 45.0f;
		float MoveAcceptanceRadius = 35.0f;
		float FaceOwnerDelay = 1.0f;
		float IdleTargetChangeTolerance = 25.0f;
		int FallbackCandidateCount = 16;

		// 주인에게서 멀어질수록 빨리 걷는다. 배율이 늘 1.0 이면 주인이 뛸 때
		// (플레이어 260 x 1.5 = 390) 파트너가 종별 MoveSpeed(기본 280)로는
		// 영영 못 따라잡고, 900uu 를 벌리는 순간 TeleportDistance 에 걸려
		// 순간이동한다.
		//
		// CatchUpNearDistance 안에서는 등속(1.0)이라 붙어 걸을 때 부산스럽지
		// 않고, TeleportDistance 근처에서 CatchUpMaxScale 에 닿는다.
		// 실행부가 배율을 [0.1, 2.0] 으로 clamp 하므로 그 위는 의미가 없다.
		float CatchUpNearDistance = 250.0f;
		float CatchUpMaxScale = 2.0f;
	};

	class FollowOwnerAction final : public IPokemonAIAction
	{
	public:
		virtual RequestedAction GetRequestType() const override;
		virtual Command Tick(const OwnContext& Context) override;

	private:
		Command MakeStopCommand() const;
		Command MakeFaceOwnerCommand(const OwnContext& Context) const;
		Command MakeArrivedCommand(const OwnContext& Context, const HHV::Map::Vec3& StableTarget);
		Command MakeTeleportCommand(const HHV::Map::Vec3& TargetLocation);
		Command MakeDirectMoveCommand(const HHV::Map::Vec3& TargetLocation);
		bool TryMakeMoveCommandForTarget(const OwnContext& Context, const HHV::Map::Vec3& CandidateLocation, Command& OutCommand);
		bool TryMakeFallbackMoveCommand(const OwnContext& Context, Command& OutCommand);
		HHV::Map::Vec3 CalculateOffsetTarget(const OwnContext& Context, float SideSign) const;
		HHV::Map::Vec3 CalculateOwnerTeleportTarget(const OwnContext& Context) const;
		float Distance2D(const HHV::Map::Vec3& A, const HHV::Map::Vec3& B) const;
		float CalculateCatchUpSpeedScale(float DistanceToOwner) const;
		void ResetArrivalTimer();

		FollowOwnerSettings Settings;
		HHV::Map::Vec3 LastIdleTarget;
		float IdleAtTargetSeconds = 0.0f;
		bool bHasIdleTarget = false;

		// Tick 이 매번 갱신한다. MoveTo 를 만드는 두 곳이 이 값을 싣는다.
		float CurrentCatchUpScale = 1.0f;
	};
}
