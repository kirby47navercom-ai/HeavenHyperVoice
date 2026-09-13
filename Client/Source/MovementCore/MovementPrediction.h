#pragma once

#include "MovementReplay.h"
#include <deque>
#include <vector>

namespace hhv::movement
{
// A command always represents one 1/60 second step, including idle input.
struct PredictedInput
{
	Input input;
	Vec3 position;
};

constexpr std::size_t MaxPendingInputs = 600;
constexpr std::size_t MaxInputBatch = 15;

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

	bool acknowledge(std::uint32_t sequence, const State &authoritative, const CollisionWorld &world)
	{
		if (sequence <= acknowledged || sequence > lastSent || !finite(authoritative.position))
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

  private:
	std::uint32_t nextSequence = 1;
	std::uint32_t lastSent = 0;
	std::uint32_t acknowledged = 0;
};

class AuthoritativeQueue
{
  public:
	void reset(const State &initial)
	{
		state = initial;
		pending.clear();
		received = 0;
		acknowledged = 0;
		budget = static_cast<float>(MaxInputBatch);
		mismatches = 0;
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
		// Server time bounds simulation speed; packet frequency cannot buy extra ticks.
		budget =
		    std::min(static_cast<float>(MaxInputBatch), budget + std::clamp(elapsed, 0.f, .25f) / FixedDt);
		bool advanced = false;
		while (!pending.empty() && budget >= 1.f)
		{
			const auto frame = pending.front();
			pending.pop_front();
			simulate(state, frame.input, config, world);

			if (length(state.position - frame.position) > .5f)
			{
				++mismatches;
			}

			acknowledged = frame.input.sequence;
			budget -= 1.f;
			advanced = true;
		}
		return advanced;
	}

	State state;
	std::uint32_t acknowledged = 0;
	std::uint64_t mismatches = 0;

  private:
	std::deque<PredictedInput> pending;
	std::uint32_t received = 0;
	float budget = 0;
};
} // namespace hhv::movement
