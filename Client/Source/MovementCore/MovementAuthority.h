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
// 400ms보다 많은 미처리 입력은 따라잡지 않고 서버 확정 상태에서 다시 시작한다.
constexpr std::size_t AuthorityResetThreshold = 24;

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
		inputEpoch = 1;
		resetRequested = false;
	}

	bool enqueue(const std::vector<PredictedInput> &batch, std::uint64_t epoch = 1,
	             bool requestReset = false)
	{
		if (epoch == 0 || epoch > inputEpoch || batch.size() > MaxInputBatch)
		{
			return false;
		}
		if (epoch < inputEpoch)
		{
			// 이미 소켓에 들어간 이전 세대 입력은 연결 오류 없이 버린다.
			return true;
		}
		if (batch.empty() && !requestReset)
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

		if (requestReset || resetRequested || pending.size() + batch.size() > AuthorityResetThreshold)
		{
			if (inputEpoch == UINT64_MAX)
			{
				return false;
			}
			discardedInputs += pending.size() + batch.size();
			pending.clear();
			resetRequested = true;
		}
		else
		{
			pending.insert(pending.end(), batch.begin(), batch.end());
		}
		received = expected;
		return true;
	}

	bool advance(float elapsed, const Config &config, const CollisionWorld &world)
	{
		correctionReady = false;
		if (resetRequested)
		{
			// 마지막으로 실제 입력을 계산한 전체 상태를 새 세대의 시작점으로 확정한다.
			// 밀린 시간과 임시 예측도 함께 폐기하여 이후 틱에서 과거를 다시 따라잡지 않는다.
			state = confirmedState;
			pending.clear();
			++inputEpoch;
			received = 0;
			acknowledged = 0;
			budget = 0;
			predictedTicks = 0;
			lastInput = {};
			needsPredictionReplacement = false;
			resetRequested = false;
			correctionReady = true;
			return true;
		}

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

	std::uint64_t epoch() const
	{
		return inputEpoch;
	}

	// 월드/다른 플레이어에게 보여 주는 상태. 확인 응답에는 correctionState()를 쓴다.
	State state;
	std::uint32_t acknowledged = 0;
	std::uint64_t mismatches = 0;
	std::uint64_t discardedInputs = 0;
	std::uint64_t predictedSteps = 0;

  private:
	State confirmedState;
	std::deque<PredictedInput> pending;
	std::uint32_t received = 0;
	float budget = 0;
	unsigned predictedTicks = 0;
	Input lastInput;
	bool correctionReady = false;
	bool needsPredictionReplacement = false;
	std::uint64_t inputEpoch = 1;
	bool resetRequested = false;
};
}
