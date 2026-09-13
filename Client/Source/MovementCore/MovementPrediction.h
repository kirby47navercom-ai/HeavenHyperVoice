#pragma once

#include "MovementReplay.h"
#include "MovementAuthority.h"
#include <deque>
#include <vector>

namespace hhv::movement
{
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
	}

	bool predict(Input input, const Config &config, const CollisionWorld &world)
	{
		// Never discard unacknowledged input: stop predicting until it is acknowledged.
		if (history.size() >= MaxPendingInputs || nextSequence == 0)
		{
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
	                 std::uint64_t discarded = 0)
	{
		if (sequence <= acknowledged || sequence > lastSent || !finite(authoritative.position) ||
		    discarded < discardedInputs || discarded - discardedInputs > sequence - acknowledged)
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

	State state;
	std::deque<StepRecord> history;
	std::uint64_t discardedInputs = 0;

  private:
	std::uint32_t nextSequence = 1;
	std::uint32_t lastSent = 0;
	std::uint32_t acknowledged = 0;
};

} // namespace hhv::movement
