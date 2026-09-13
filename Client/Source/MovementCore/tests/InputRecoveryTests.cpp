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

void testQueueCompaction(const CollisionWorld &world)
{
	Config config;
	PredictionQueue client;
	AuthoritativeQueue server;
	client.reset(spawn());
	server.reset(spawn());
	for (int tick = 1; tick <= 90; ++tick)
	{
		Input input;
		input.x = 1;
		input.buttons = tick == 30 ? Jump : (tick == 85 ? Roll : 0);
		require(client.predict(input, config, world), "Could not build the backlog");
	}
	const float fullDistance = client.state.position.x;
	for (int batch = 0; batch < 6; ++batch)
	{
		require(server.enqueue(client.takeUnsent()), "Backlog batch rejected");
	}

	bool jumped = false;
	bool rolled = false;
	bool skipped = false;
	for (int update = 0; update < 120 && server.acknowledged < 90; ++update)
	{
		server.advance(FixedDt, config, world);
		jumped = jumped || server.correctionState().mode == Mode::Falling;
		rolled = rolled || server.correctionState().rollRemaining > 0;
		skipped = skipped || server.discardedInputs > 0;
		if (server.hasCorrection())
		{
			require(client.acknowledge(server.acknowledged, server.correctionState(), world, server.discardedInputs),
			        "Compacted correction was rejected");
		}
	}
	require(skipped && server.discardedInputs > 20, "Continuous backlog was not compacted");
	require(jumped && rolled, "Compaction lost a jump or roll transition");
	require(server.acknowledged == 90 && client.history.empty(), "Backlog did not recover");
	require(replay::near(client.state, server.correctionState(), .001f), "Compacted client did not converge");
	require(server.correctionState().position.x < fullDistance, "Dropped duration was silently restored");
	require(client.discardedInputs == server.discardedInputs, "Dropped inputs were reported as verified");

	client.predict({0, 1, 0, 0}, config, world);
	client.takeUnsent();
	require(!client.acknowledge(91, server.correctionState(), world, 0), "Regressing discard count accepted");
}

void testTransitionBoundaries(const CollisionWorld &world)
{
	Config config;
	AuthoritativeQueue server;
	server.reset(spawn());
	std::vector<std::uint32_t> important;
	for (std::uint32_t start = 1; start <= 60; start += 15)
	{
		std::vector<PredictedInput> batch;
		for (std::uint32_t sequence = start; sequence < start + 15; ++sequence)
		{
			const float x = sequence < 20 ? 1.f : (sequence < 40 ? 0.f : -1.f);
			const std::uint8_t buttons = sequence >= 30 ? Run : 0;
			batch.push_back({{sequence, x, 0, buttons}, {}});
		}
		server.enqueue(batch);
	}
	for (int tick = 0; tick < 60 && server.acknowledged < 60; ++tick)
	{
		server.advance(FixedDt, config, world);
		important.push_back(server.acknowledged);
	}
	for (std::uint32_t sequence : {19, 20, 29, 30, 39, 40})
	{
		require(std::find(important.begin(), important.end(), sequence) != important.end(),
		        "Direction, stop or run boundary was removed");
	}

	// 단발 입력이 계속 오면 압축하지 않는다. CPU 한도는 별도로 유지한다.
	server.reset(spawn());
	for (std::uint32_t start = 1; start <= 60; start += 15)
	{
		std::vector<PredictedInput> batch;
		for (std::uint32_t sequence = start; sequence < start + 15; ++sequence)
		{
			batch.push_back({{sequence, 1, 0, Jump}, {}});
		}
		server.enqueue(batch);
	}
	server.advance(.5f, config, world);
	require(server.acknowledged == MaxAuthorityStepsPerUpdate && server.discardedInputs == 0,
	        "Event-heavy backlog bypassed the work limit or dropped an event");
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

void testServerHitchBudget(const CollisionWorld &world)
{
	AuthoritativeQueue server;
	server.reset(spawn());
	for (std::uint32_t start = 1; start <= 48; start += 12)
	{
		std::vector<PredictedInput> batch;
		for (std::uint32_t sequence = start; sequence < start + 12; ++sequence)
		{
			batch.push_back({{sequence, 1, 0, Jump}, {}});
		}
		server.enqueue(batch);
	}
	server.advance(FixedDt * 48, Config{}, world);
	require(server.acknowledged == 6, "A server hitch bypassed the per-update step limit");
	for (int recovery = 0; recovery < 7; ++recovery)
	{
		server.advance(0, Config{}, world);
	}
	require(server.acknowledged == 48 && server.discardedInputs == 0,
	        "Server hitch time was lost before protected inputs could catch up");
	const auto recovered = server.state;
	server.advance(0, Config{}, world);
	require(replay::near(recovered, server.state, .001f), "Catch-up invented additional simulation time");
}

void testCompactionWhileReplacingPrediction(const CollisionWorld &world)
{
	AuthoritativeQueue server;
	server.reset(spawn());
	server.enqueue({{{1, 1, 0, 0}, {}}});
	server.advance(FixedDt, Config{}, world);
	server.advance(FixedDt * 3, Config{}, world);
	const auto predicted = server.state;
	for (std::uint32_t start = 2; start < 62; start += 15)
	{
		std::vector<PredictedInput> batch;
		for (std::uint32_t sequence = start; sequence < start + 15; ++sequence)
		{
			batch.push_back({{sequence, 1, 0, 0}, {}});
		}
		server.enqueue(batch);
	}
	require(!server.advance(0, Config{}, world), "Discarded duration should exhaust this update's credit");
	require(replay::near(server.state, predicted, .001f), "A non-advancing update silently rewound world state");
	server.advance(FixedDt, Config{}, world);
	require(server.hasCorrection() && server.acknowledged == 2,
	        "Deferred prediction replacement did not process its first retained input");
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
		testQueueCompaction(world);
		testTransitionBoundaries(world);
		testPredictionCollision();
		testServerHitchBudget(world);
		testCompactionWhileReplacingPrediction(world);
		std::cout << "Missing/late input, compaction, event boundaries, corrections and collisions passed\n";
	}
	catch (const std::exception &error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
