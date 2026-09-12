#include "MovementPrediction.h"
#include "TriangleWorld.h"
#include <iostream>
#include <stdexcept>

using namespace hhv::movement;

void require(bool value, const char *message)
{
	if (!value)
	{
		throw std::runtime_error(message);
	}
}

int main()
{
	try
	{
		TriangleWorld world;
		world.build({{{-10000, -10000, 0}, {10000, -10000, 0}, {10000, 10000, 0}},
		             {{-10000, -10000, 0}, {10000, 10000, 0}, {-10000, 10000, 0}}});
		Config config;
		State initial;
		initial.position = {0, 0, 88.1f};
		PredictionQueue client;
		AuthoritativeQueue server;
		client.reset(initial);
		server.reset(initial);

		// Delay responses while jumping and rolling. Replaying pending input must preserve both.
		std::deque<std::pair<std::uint32_t, State>> responses;
		for (int tick = 0; tick < 1200; ++tick)
		{
			Input input;
			input.x = tick < 600 ? .6f : -.6f;
			input.y = .3f;
			input.buttons = tick % 180 == 10 ? Jump : (tick % 180 == 100 ? Roll : Run);
			require(client.predict(input, config, world), "prediction queue overflow");

			if (tick % 3 == 2)
			{
				require(server.enqueue(client.takeUnsent()), "server rejected sequential input");
				require(server.advance(FixedDt * 3, config, world), "server did not consume input");
				responses.push_back({server.acknowledged, server.state});
			}

			if (responses.size() > 5)
			{
				const auto response = responses.front();
				responses.pop_front();
				require(client.acknowledge(response.first, response.second, world), "acknowledgement failed");
			}
		}
		require(server.mismatches == 0, "identical core simulation diverged");
		require(length(client.state.position - server.state.position) < .001f, "delayed replay diverged");
		require(client.acknowledge(server.acknowledged, server.state, world), "final ack failed");
		require(client.history.empty(), "acknowledged input was not removed");
		require(!client.acknowledge(server.acknowledged, server.state, world), "duplicate ack applied twice");

		// Coordinates are evidence to compare, never a source of authoritative movement.
		client.predict({0, 1, 0, 0}, config, world);
		auto forged = client.takeUnsent();
		forged.front().position = {999999, 999999, 999999};
		const auto before = server.state.position;
		require(server.enqueue(forged), "valid input should still be simulated");
		server.advance(FixedDt, config, world);
		require(server.mismatches == 1, "forged position was not detected");
		require(length(server.state.position - before) < 20, "claimed position became authoritative");
		require(!server.enqueue(forged), "duplicate command accepted");

		forged.front().input.sequence += 2;
		require(!server.enqueue(forged), "sequence gap accepted");
		forged.front().input.sequence -= 1;
		forged.front().input.x = std::numeric_limits<float>::quiet_NaN();
		require(!server.enqueue(forged), "NaN input accepted");

		// Replaying from an actual correction must also replace vertical velocity and roll state.
		client.reset(initial);
		for (int tick = 0; tick < 30; ++tick)
		{
			client.predict({0, 1, 0, static_cast<std::uint8_t>(tick == 0 ? Jump : 0)}, config, world);
		}
		client.takeUnsent();
		State corrected = initial;
		corrected.position.z += 100;
		corrected.velocity.z = -100;
		State expected = corrected;
		for (const auto &record : client.history)
		{
			if (record.input.sequence > 15)
			{
				simulate(expected, record.input, record.config, world);
			}
		}
		require(client.acknowledge(15, corrected, world), "correction not applied");
		require(length(expected.position - client.state.position) < .001f, "full-state replay failed");

		// Repeated calls without elapsed server time must not grant more simulation time.
		server.reset(initial);
		std::uint32_t sequence = 0;
		for (int batch = 0; batch < 20; ++batch)
		{
			std::vector<PredictedInput> burst;
			for (int tick = 0; tick < 15; ++tick)
			{
				burst.push_back({{++sequence, 1, 0, 0}, {}});
			}
			require(server.enqueue(burst), "bounded burst could not be queued");
			server.advance(0, config, world);
		}
		require(server.acknowledged == 15, "packet frequency accelerated simulation");
		server.advance(FixedDt * 3, config, world);
		require(server.acknowledged == 18, "server time budget did not replenish");
		std::cout << "Prediction, replay, validation and server-time budget passed\n";
	}
	catch (const std::exception &error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
