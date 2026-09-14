#pragma once

#include "MovementReplay.h"
#include "MovementAuthority.h"
#include <deque>
#include <vector>

namespace hhv::movement
{
// 응답이 2초 이상 밀린 클라이언트도 서버에 새 기준 상태를 요청한다.
constexpr std::size_t PredictionResetThreshold = 120;

class PredictionQueue
{
  public:
	void reset(const State &authoritative)
	{
		state = authoritative;
		history.clear();
		nextSequence = 1;
		lastSent = 0;
		acknowledged = 0;
		discardedInputs = 0;
		inputEpoch = 1;
		resetRequested = false;
	}

	bool predict(Input input, const Config &config, const CollisionWorld &world)
	{
		// 서버 응답 전에는 임의로 순번을 버리지 않는다. 강제 동기화를 요청하고 기다린다.
		if (resetRequested || history.size() >= PredictionResetThreshold || nextSequence == 0)
		{
			resetRequested = true;
			return false;
		}

		input.sequence = nextSequence++;
		const State before = state;
		simulate(state, input, config, world);
		history.push_back({input, config, before, state});
		return true;
	}

	std::vector<PredictedInput> takeUnsent()
	{
		std::vector<PredictedInput> batch;
		if (resetRequested)
		{
			return batch;
		}
		for (const auto &record : history)
		{
			if (record.input.sequence <= lastSent)
			{
				continue;
			}

			batch.push_back({record.input, record.after.position});
			lastSent = record.input.sequence;
			if (batch.size() == MaxInputBatch)
			{
				break;
			}
		}
		return batch;
	}

	bool acknowledge(std::uint32_t sequence, const State &authoritative, const CollisionWorld &world,
	                 std::uint64_t discarded = 0, std::uint64_t epoch = 1)
	{
		if (epoch == 0 || epoch < inputEpoch || !finite(authoritative.position) || discarded < discardedInputs)
		{
			return false;
		}
		if (epoch > inputEpoch)
		{
			if (inputEpoch == UINT64_MAX || epoch != inputEpoch + 1 || sequence != 0)
			{
				return false;
			}
			// 이전 세대의 미전송/미확인 입력을 재실행하지 않고 서버 상태를 그대로 적용한다.
			state = authoritative;
			history.clear();
			nextSequence = 1;
			lastSent = 0;
			acknowledged = 0;
			discardedInputs = discarded;
			inputEpoch = epoch;
			resetRequested = false;
			return true;
		}
		if (sequence <= acknowledged || sequence > lastSent || discarded != discardedInputs)
		{
			return false;
		}

		while (!history.empty() && history.front().input.sequence <= sequence)
		{
			history.pop_front();
		}

		// Rebuild from the complete server state, including jump and roll state.
		state = authoritative;
		acknowledged = sequence;
		discardedInputs = discarded;
		for (auto &record : history)
		{
			record.before = state;
			simulate(state, record.input, record.config, world);
			record.after = state;
		}
		return true;
	}

	std::uint64_t epoch() const
	{
		return inputEpoch;
	}

	bool needsReset() const
	{
		return resetRequested;
	}

	State state;
	std::deque<StepRecord> history;
	std::uint64_t discardedInputs = 0;

  private:
	std::uint32_t nextSequence = 1;
	std::uint32_t lastSent = 0;
	std::uint32_t acknowledged = 0;
	std::uint64_t inputEpoch = 1;
	bool resetRequested = false;
};

} // namespace hhv::movement
