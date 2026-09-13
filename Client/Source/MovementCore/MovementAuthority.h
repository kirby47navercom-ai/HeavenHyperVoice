#pragma once

#include "MovementInput.h"
#include <deque>
#include <vector>

namespace hhv::movement
{
// 20Hz 서버 호출 기준 정상 3스텝, 복구 중 최대 6스텝을 실행한다.
constexpr unsigned MaxAuthorityStepsPerUpdate = 6;
constexpr unsigned MaxAuthorityTimeCredit = static_cast<unsigned>(MaxPendingInputs);
constexpr unsigned PredictDirectionTicks = 6;
constexpr std::size_t CompactInputThreshold = 24;
constexpr std::size_t RecentInputsToKeep = 6;
constexpr std::size_t InputKeyframeStride = 6;

class AuthoritativeQueue
{
  public:
	void reset(const State &initial)
	{
		state = initial;
		confirmedState = initial;
		pending.clear();
		received = 0;
		acknowledged = 0;
		budget = 0;
		predictedTicks = 0;
		lastInput = {};
		correctionReady = false;
		needsPredictionReplacement = false;
		mismatches = 0;
		discardedInputs = 0;
		predictedSteps = 0;
	}

	bool enqueue(const std::vector<PredictedInput> &batch)
	{
		if (batch.empty() || batch.size() > MaxInputBatch || pending.size() + batch.size() > MaxPendingInputs)
		{
			return false;
		}

		std::uint32_t expected = received;
		for (const auto &frame : batch)
		{
			if (expected == UINT32_MAX || frame.input.sequence != ++expected || !valid(frame.input) ||
			    !finite(frame.position))
			{
				return false;
			}
		}

		received = expected;
		pending.insert(pending.end(), batch.begin(), batch.end());
		return true;
	}

	bool advance(float elapsed, const Config &config, const CollisionWorld &world)
	{
		correctionReady = false;
		const float maximumSeconds = float(MaxAuthorityTimeCredit) * FixedDt;
		const float seconds = std::isfinite(elapsed) ? std::clamp(elapsed, 0.f, maximumSeconds) : 0.f;
		budget = std::min(float(MaxAuthorityTimeCredit), budget + seconds / FixedDt);
		if (acknowledged == 0 && pending.empty())
		{
			// 입장 후 기다린 시간을 최초 입력의 이동 시간으로 몰아 주지 않는다.
			budget = std::min(budget, 3.f);
		}

		if (!pending.empty() && predictedTicks > 0)
		{
			// 늦은 실제 입력은 추측했던 이동을 대체한다. 같은 시간을 두 번 이동하지 않는다.
			budget = std::min(float(MaxAuthorityTimeCredit), budget + float(predictedTicks));
			predictedTicks = 0;
			needsPredictionReplacement = true;
		}

		const std::size_t removed = compactPending();
		if (removed > 0)
		{
			// 생략한 과거 시간을 새 예측 이동에 다시 쓰지 않는다.
			budget = std::max(0.f, budget - float(removed));
		}

		bool advanced = false;
		for (unsigned step = 0; step < MaxAuthorityStepsPerUpdate && budget + 1e-6f >= 1.f; ++step)
		{
			if (!pending.empty())
			{
				if (needsPredictionReplacement)
				{
					state = confirmedState;
					needsPredictionReplacement = false;
				}

				const auto frame = pending.front();
				pending.pop_front();
				simulate(state, frame.input, config, world);

				if (length(state.position - frame.position) > .5f)
				{
					++mismatches;
				}

				// 건너뛴 입력은 검증 성공으로 세지 않는다. 누적 폐기 수를 보정에 명시한다.
				discardedInputs += frame.input.sequence - acknowledged - 1;
				acknowledged = frame.input.sequence;
				lastInput = frame.input;
				confirmedState = state;
				correctionReady = true;
			}
			else if (acknowledged != 0)
			{
				Input predicted;
				if (predictedTicks < PredictDirectionTicks)
				{
					predicted.x = lastInput.x;
					predicted.y = lastInput.y;
					predicted.buttons = lastInput.buttons & Run;
				}

				// 점프/구르기 버튼은 재생하지 않는다. 공백이 길어지면 중립 입력으로 감속한다.
				// 중력과 충돌은 계속 계산하므로 공중에서 그대로 멈추지 않는다.
				simulate(state, predicted, config, world);
				predictedTicks = std::min(predictedTicks + 1, MaxAuthorityTimeCredit);
				++predictedSteps;
			}
			else
			{
				// 최초 입력이 오기 전에는 입장 상태를 유지한다.
				break;
			}

			budget = std::max(0.f, budget - 1.f);
			advanced = true;
		}
		return advanced;
	}

	const State &correctionState() const
	{
		return confirmedState;
	}

	bool hasCorrection() const
	{
		return correctionReady;
	}

	std::size_t queuedInputs() const
	{
		return pending.size();
	}

	// 월드/다른 플레이어에게 보여 주는 상태. 확인 응답에는 correctionState()를 쓴다.
	State state;
	std::uint32_t acknowledged = 0;
	std::uint64_t mismatches = 0;
	std::uint64_t discardedInputs = 0;
	std::uint64_t predictedSteps = 0;

  private:
	static bool sameControl(const Input &a, const Input &b)
	{
		return a.buttons == b.buttons && std::abs(a.x - b.x) < .0001f &&
		       std::abs(a.y - b.y) < .0001f;
	}

	std::size_t compactPending()
	{
		const auto &base = needsPredictionReplacement ? confirmedState : state;
		if (pending.size() <= CompactInputThreshold || base.mode != Mode::Grounded || base.rollRemaining > 0)
		{
			return 0;
		}

		std::deque<PredictedInput> keyframes;
		const std::size_t recentStart = pending.size() - RecentInputsToKeep;
		bool preserveMotion = false;
		for (std::size_t index = 0; index < pending.size(); ++index)
		{
			const auto &frame = pending[index];
			const bool edge = index == 0 || index >= recentStart;
			const bool oneShot = (frame.input.buttons & (Jump | Roll)) != 0;
			// 점프/구르기 뒤의 물리 상태는 아직 모른다. 이후 구간도 계산 전에는 줄이지 않는다.
			preserveMotion = preserveMotion || oneShot;
			const bool changedBefore = index > 0 && !sameControl(pending[index - 1].input, frame.input);
			const bool changedAfter = index + 1 < pending.size() &&
			                          !sameControl(frame.input, pending[index + 1].input);
			if (edge || preserveMotion || changedBefore || changedAfter || index % InputKeyframeStride == 0)
			{
				keyframes.push_back(frame);
			}
		}

		const auto removed = pending.size() - keyframes.size();
		pending.swap(keyframes);
		return removed;
	}

	State confirmedState;
	std::deque<PredictedInput> pending;
	std::uint32_t received = 0;
	float budget = 0;
	unsigned predictedTicks = 0;
	Input lastInput;
	bool correctionReady = false;
	bool needsPredictionReplacement = false;
};
}
