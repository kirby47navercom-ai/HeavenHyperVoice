#include "MovementPrediction.h"
#include "TriangleWorld.h"
#include <iostream>
#include <stdexcept>

using namespace hhv::movement;

namespace
{
void require(bool value, const char *message)
{
	if (!value)
	{
		throw std::runtime_error(message);
	}
}

State spawn()
{
	State state;
	state.position = {0, 0, 88.1f};
	state.mode = Mode::Grounded;
	return state;
}

void testMissingAndLateInputs(const CollisionWorld &world)
{
	Config config;
	AuthoritativeQueue server;
	State expected = spawn();
	server.reset(expected);
	Input first{1, 1, 0, Run};
	simulate(expected, first, config, world);
	require(server.enqueue({{first, expected.position}}), "Initial input rejected");
	server.advance(FixedDt, config, world);
	const auto confirmed = server.correctionState();

	for (int tick = 0; tick < 3; ++tick)
	{
		simulate(expected, {0, 1, 0, Run}, config, world);
	}
	server.advance(FixedDt * 3, config, world);
	require(replay::near(server.state, expected, .001f), "Missing input did not continue the last direction");
	require(server.acknowledged == 1 && !server.hasCorrection(), "Prediction acknowledged an unreceived input");
	require(replay::near(server.correctionState(), confirmed, .001f), "Prediction changed the confirmed state");

	// 실제로는 방향을 바꾸면서 점프했다. 기존 추측에 더하지 않고 대체해야 한다.
	expected = confirmed;
	std::vector<PredictedInput> late;
	for (std::uint32_t sequence = 2; sequence <= 4; ++sequence)
	{
		Input input{sequence, 0, 1, static_cast<std::uint8_t>(sequence == 2 ? Jump : 0)};
		simulate(expected, input, config, world);
		late.push_back({input, expected.position});
	}
	require(server.enqueue(late), "Late inputs rejected");
	server.advance(0, config, world);
	require(replay::near(server.state, expected, .001f), "Late input double-counted predicted movement");
	require(server.acknowledged == 4 && server.hasCorrection(), "Late input was not confirmed");
	require(!server.enqueue(late), "Duplicate late input was accepted");

	// 공백이 길어져도 중력은 계속 진행되고, 점프를 재발동하지 않는다.
	for (int tick = 0; tick < 120; ++tick)
	{
		server.advance(FixedDt, config, world);
	}
	require(server.state.mode == Mode::Grounded && length(server.state.velocity) < .1f,
	        "Long input loss failed to stop and land");
	require(server.acknowledged == 4 && server.discardedInputs == 0,
	        "Speculation consumed actual input sequences");
}

void testConfirmedCorrection(const CollisionWorld &world)
{
	Config config;
	PredictionQueue client;
	AuthoritativeQueue server;
	client.reset(spawn());
	server.reset(spawn());
	for (int tick = 0; tick < 3; ++tick)
	{
		client.predict({0, 1, 0, 0}, config, world);
	}
	server.enqueue(client.takeUnsent());
	server.advance(FixedDt * 6, config, world);
	require(server.state.position.x > server.correctionState().position.x, "Expected speculative future state");
	require(client.acknowledge(server.acknowledged, server.correctionState(), world, server.discardedInputs),
	        "Confirmed correction rejected");
	require(replay::near(client.state, server.correctionState(), .001f), "Speculative future leaked into replay");
}

void testHardReset(const CollisionWorld &world)
{
	Config config;
	PredictionQueue client;
	AuthoritativeQueue server;
	client.reset(spawn());
	server.reset(spawn());
	client.predict({0, 1, 0, 0}, config, world);
	server.enqueue(client.takeUnsent(), client.epoch());
	server.advance(FixedDt, config, world);
	client.acknowledge(server.acknowledged, server.correctionState(), world);
	const auto baseline = server.correctionState();
	const auto oldEpoch = client.epoch();

	for (int tick = 0; tick < 75; ++tick)
	{
		const auto buttons = static_cast<std::uint8_t>(tick == 0 ? Jump : (tick == 60 ? Roll : 0));
		require(client.predict({0, 1, 0, buttons}, config, world), "Could not build reset backlog");
	}
	server.enqueue(client.takeUnsent(), oldEpoch);
	server.enqueue(client.takeUnsent(), oldEpoch);
	const auto inFlight = client.takeUnsent();
	require(server.queuedInputs() == 0, "Overflow left old input on the server");
	require(server.advance(2.f, config, world), "Overflow did not publish a correction");
	require(server.hasCorrection() && server.epoch() == oldEpoch + 1 && server.acknowledged == 0,
	        "Reset did not open a fresh input epoch");
	require(server.discardedInputs == 30 && replay::near(server.state, baseline, .001f),
	        "Overflow executed stale movement or changed the confirmed state");
	require(client.acknowledge(0, server.correctionState(), world, server.discardedInputs, server.epoch()),
	        "New epoch correction rejected");
	require(client.history.empty() && client.takeUnsent().empty() && replay::near(client.state, baseline, .001f),
	        "Hard reset replayed sent or unsent client inputs");
	require(server.enqueue(inFlight, oldEpoch) && server.queuedInputs() == 0,
	        "In-flight old inputs returned to the server queue");
	require(!client.acknowledge(1, spawn(), world, 0, oldEpoch), "Old correction moved the reset client");
	require(!client.acknowledge(0, spawn(), world, server.discardedInputs, server.epoch()),
	        "Duplicate reset correction was applied twice");
	require(!server.advance(0, config, world) && replay::near(server.state, baseline, .001f),
	        "Discarded catch-up time advanced the reset server");

	client.predict({0, 0, 1, 0}, config, world);
	const auto fresh = client.takeUnsent();
	require(fresh.size() == 1 && fresh.front().input.sequence == 1, "New input numbering did not restart");
	require(server.enqueue(fresh, client.epoch()), "New epoch input rejected");
	server.advance(FixedDt, config, world);
	require(client.acknowledge(1, server.correctionState(), world, server.discardedInputs, server.epoch()),
	        "New epoch acknowledgement failed");
	require(client.history.empty() && replay::near(client.state, server.state, .001f),
	        "Fresh movement diverged after reset");
	require(!server.enqueue(fresh, client.epoch()), "Same-epoch duplicate was accepted");
	require(!server.enqueue(fresh, client.epoch() + 1), "Client selected an unauthorized future epoch");
}

void testResetPreservesExecutedState(const CollisionWorld &world)
{
	for (const auto button : {Jump, Roll})
	{
		AuthoritativeQueue server;
		server.reset(spawn());
		server.enqueue({{{1, 1, 0, static_cast<std::uint8_t>(button)}, {}}});
		server.advance(FixedDt, Config{}, world);
		const auto confirmed = server.correctionState();
		server.advance(FixedDt * 3, Config{}, world);
		for (std::uint32_t first : {2u, 17u})
		{
			std::vector<PredictedInput> batch;
			for (std::uint32_t sequence = first; sequence < first + 15; ++sequence)
			{
				batch.push_back({{sequence, -1, 0, Jump}, {}});
			}
			require(server.enqueue(batch), "Event-heavy backlog rejected");
		}
		server.advance(.5f, Config{}, world);
		require(server.epoch() == 2 && server.queuedInputs() == 0 && server.discardedInputs == 30,
		        "Airborne or rolling backlog escaped reset");
		require(replay::near(server.state, confirmed, .001f),
		        "Reset lost executed jump/roll state or kept speculative motion");
	}
}

void testClientRequestedReset(const CollisionWorld &world)
{
	PredictionQueue client;
	AuthoritativeQueue server;
	client.reset(spawn());
	server.reset(spawn());
	// 서버는 정상 처리하지만 확인 응답이 클라이언트에 도착하지 않는 상황이다.
	for (std::size_t tick = 0; tick < PredictionResetThreshold; ++tick)
	{
		require(client.predict({0, 1, 0, 0}, Config{}, world), "Client reached its limit too early");
		require(server.enqueue(client.takeUnsent(), client.epoch()), "Normal input rejected");
		server.advance(FixedDt, Config{}, world);
	}
	const auto baseline = server.correctionState();
	require(!client.predict({0, 1, 0, Jump}, Config{}, world) && client.needsReset(),
	        "Client did not request reset after prolonged missing acknowledgements");
	require(client.takeUnsent().empty(), "Reset-waiting client kept sending stale movement");
	require(server.enqueue({}, client.epoch(), true), "Empty reset request rejected");
	server.advance(.5f, Config{}, world);
	require(client.acknowledge(0, server.correctionState(), world, server.discardedInputs, server.epoch()),
	        "Client-initiated reset response rejected");
	require(!client.needsReset() && client.history.empty() && replay::near(client.state, baseline, .001f),
	        "Client-initiated reset did not clear history and waiting state");
	const auto epoch = server.epoch();
	require(server.enqueue({}, epoch - 1, true), "Late reset request caused a disconnect");
	server.advance(0, Config{}, world);
	require(server.epoch() == epoch, "Late request reset the new generation again");
	require(!server.enqueue({}, epoch + 1, true), "Future-epoch reset request accepted");
}

void testThresholdAndWorkLimit(const CollisionWorld &world)
{
	AuthoritativeQueue server;
	server.reset(spawn());
	for (std::uint32_t first : {1u, 13u})
	{
		std::vector<PredictedInput> batch;
		for (std::uint32_t sequence = first; sequence < first + 12; ++sequence)
		{
			batch.push_back({{sequence, 1, 0, Jump}, {}});
		}
		server.enqueue(batch);
	}
	server.advance(FixedDt * 24, Config{}, world);
	require(server.epoch() == 1 && server.acknowledged == MaxAuthorityStepsPerUpdate,
	        "Threshold boundary reset unnecessarily or bypassed the work limit");
	for (int update = 0; update < 3; ++update)
	{
		server.advance(0, Config{}, world);
	}
	require(server.acknowledged == 24 && server.discardedInputs == 0,
	        "Within-threshold inputs failed to catch up with earned time");
}

void testPredictionCollision()
{
	TriangleWorld world;
	world.build({{{-1000, -1000, 0}, {1000, -1000, 0}, {1000, 1000, 0}},
	             {{-1000, -1000, 0}, {1000, 1000, 0}, {-1000, 1000, 0}},
	             {{60, -1000, 0}, {60, 1000, 0}, {60, 1000, 1000}},
	             {{60, -1000, 0}, {60, 1000, 1000}, {60, -1000, 1000}}});
	AuthoritativeQueue server;
	server.reset(spawn());
	server.enqueue({{{1, 1, 0, Run}, {99999, 99999, 99999}}});
	server.advance(FixedDt, Config{}, world);
	for (int tick = 0; tick < 30; ++tick)
	{
		server.advance(FixedDt, Config{}, world);
	}
	require(server.state.position.x < 27.f && server.state.mode == Mode::Grounded,
	        "Server prediction crossed a wall or adopted a client position");
}

}

int main()
{
	try
	{
		TriangleWorld world;
		world.build({{{-10000, -10000, 0}, {10000, -10000, 0}, {10000, 10000, 0}},
		             {{-10000, -10000, 0}, {10000, 10000, 0}, {-10000, 10000, 0}}});
		testMissingAndLateInputs(world);
		testConfirmedCorrection(world);
		testHardReset(world);
		testResetPreservesExecutedState(world);
		testClientRequestedReset(world);
		testThresholdAndWorkLimit(world);
		testPredictionCollision();
		std::cout << "Missing/late input, hard reset, stale epochs, client reset requests and collisions passed\n";
	}
	catch (const std::exception &error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
