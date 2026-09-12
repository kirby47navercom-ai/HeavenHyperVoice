#include "MovementReplay.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace hhv::movement;

namespace
{
void require(bool condition, const std::string& message)
{
	if (!condition)
	{
		throw std::runtime_error(message);
	}
}

void addQuad(std::vector<Triangle>& triangles, Vec3 a, Vec3 b, Vec3 c, Vec3 d)
{
	triangles.push_back({a, b, c});
	triangles.push_back({a, c, d});
}

State placeOnFloor(const TriangleWorld& world, Vec3 position, const Config& config)
{
	State state;
	state.position = position;
	state.position.z = 2000;

	require(findFloor(state, config, world, 4000), "Could not place the capsule on the map");

	for (int tick = 0; tick < 90; ++tick)
	{
		simulate(state, {}, config, world);
	}

	return state;
}

void groundedSlopeDoesNotCreateJumpVelocity()
{
	std::vector<Triangle> triangles;
	addQuad(triangles, {-1000, -1000, 0}, {150, -1000, 0}, {150, 1000, 0}, {-1000, 1000, 0});
	addQuad(triangles, {150, -1000, 0}, {1000, -1000, 425}, {1000, 1000, 425}, {150, 1000, 0});

	TriangleWorld world;
	world.build(triangles);
	Config config;
	State state = placeOnFloor(world, {}, config);

	for (int tick = 0; tick < 100; ++tick)
	{
		simulate(state, {0, 1, 0, Run}, config, world);
		require(state.mode == Mode::Grounded, "Lost the floor on a continuous walkable slope");
		require(std::abs(state.velocity.z) < .001f, "Walking produced jump velocity");
	}

	require(state.position.x > 500, "Grounding fix stopped forward movement");
}

void fallingSlopeContactDoesNotBoostUpward()
{
	std::vector<Triangle> triangles;
	addQuad(triangles, {-1000, -1000, -500}, {1000, -1000, 500},
	        {1000, 1000, 500}, {-1000, 1000, -500});

	TriangleWorld world;
	world.build(triangles);
	Config config;
	State state;
	state.position = {0, 0, 93};
	state.velocity = {390, 0, 20};

	simulate(state, {0, 1, 0, Run}, config, world);

	const float remainingJumpSpeed = 20 - config.gravity * FixedDt;
	require(state.velocity.z <= remainingJumpSpeed + .001f, "Slope collision boosted an airborne capsule");
}

void jumpAndUnsupportedSteepSlope()
{
	std::vector<Triangle> triangles;
	addQuad(triangles, {-1000, -1000, -500}, {1000, -1000, 500},
	        {1000, 1000, 500}, {-1000, 1000, -500});

	TriangleWorld world;
	world.build(triangles);
	Config config;
	State state = placeOnFloor(world, {}, config);
	const float startZ = state.position.z;

	simulate(state, {0, 0, 0, Jump}, config, world);

	for (int tick = 0; tick < 10; ++tick)
	{
		require(state.mode == Mode::Falling && state.velocity.z > 0, "Floor snap cancelled a real jump");
		simulate(state, {}, config, world);
	}

	require(state.position.z > startZ + 40, "Jump did not leave the slope");

	for (int tick = 0; tick < 100; ++tick)
	{
		simulate(state, {}, config, world);
	}

	require(state.mode == Mode::Grounded, "Jump did not land on the slope");

	triangles.clear();
	addQuad(triangles, {-1000, -1000, -1500}, {1000, -1000, 1500},
	        {1000, 1000, 1500}, {-1000, 1000, -1500});
	world.build(triangles);
	state = {};
	state.position = {0, 0, 116};
	state.velocity = {0, 0, -100};

	for (int tick = 0; tick < 60; ++tick)
	{
		simulate(state, {}, config, world);
		require(state.mode == Mode::Falling, "Unsupported steep slope became walkable");
		require(state.velocity.z <= 0, "Steep slope launched the capsule upward");
	}

	require(state.position.z < 66, "Falling capsule remained stuck on a steep face");
}

void floorSnapDoesNotCrossSteepGeometry()
{
	std::vector<Triangle> triangles;
	addQuad(triangles, {-1000, -1000, 0}, {1000, -1000, 0}, {1000, 1000, 0}, {-1000, 1000, 0});
	addQuad(triangles, {-1000, -1000, -1495}, {1000, -1000, 1505},
	        {1000, 1000, 1505}, {-1000, 1000, -1495});

	TriangleWorld world;
	world.build(triangles);
	Config config;
	State state;
	state.position = {0, 0, 125};
	const Vec3 start = state.position;

	require(!findFloor(state, config, world, 48), "Floor search snapped through a steep obstacle");
	require(length(state.position - start) < .001f, "Rejected floor search moved the capsule");
}

void playerTestLevelContacts(const TriangleWorld& world)
{
	Config config;
	State state;
	state.position = {-560.944641f, 1016.12836f, 88.7420654f};

	for (float distance : {.5f, 3.1f, 48.f})
	{
		const Hit contact = world.sweep(state.position, {0, 0, -distance}, config.radius, config.halfHeight);
		require(contact.blocking && contact.surfaceNormal.z > .98f,
		        "Shared-edge face selection changed with the sweep distance");
	}

	require(findFloor(state, config, world, 3.1f), "10 degree entrance selected the end face instead of support");
	require(state.floorNormal.z > .98f, "Entrance support did not select the ramp top");

	state.position = {-1529.09216f, 2527.38525f, 87.5999756f};
	require(findFloor(state, config, world, 3.1f), "50 degree face hid the flat ground");
	require(state.floorNormal.z > .99f, "Steep face was treated as the floor");
}

void playerTestLevelApproaches(const TriangleWorld& world)
{
	struct Ramp
	{
		int angle;
		Vec3 start;
		Vec3 direction;
		int walkingTicks;
	};

	const Ramp ramps[] = {
	    {10, {-645.843744f, 944.889573f, 0}, {.766044490f, .642787554f, 0}, 153},
	    {20, {-960.086124f, 1320.858243f, 0}, {.766044221f, .642787874f, 0}, 146},
	    {30, {-1234.732181f, 1664.782070f, 0}, {.766044221f, .642787874f, 0}, 136},
	    {40, {-1495.742275f, 2007.094199f, 0}, {.766044878f, .642787092f, 0}, 121},
	    {50, {-1743.583377f, 2347.401860f, 0}, {.766044878f, .642787092f, 0}, 104},
	    {60, {-2004.639333f, 2715.782738f, 0}, {.766044212f, .642787886f, 0}, 83},
	};

	Config config;
	int failures = 0;
	std::string firstFailure;

	for (const Ramp& ramp : ramps)
	{
		for (int phase = 0; phase < 17; ++phase)
		{
			for (const std::uint8_t buttons : {std::uint8_t{0}, std::uint8_t{Run}, std::uint8_t{Roll}})
			{
				State state = placeOnFloor(world, ramp.start + ramp.direction * (phase * .25f), config);
				const Vec3 start = state.position;
				const float speed = buttons == Roll ? config.rollSpeed
				                    : buttons == Run ? config.runSpeed : config.walkSpeed;
				const int ticks = ramp.angle > 44 ? 600 : int(ramp.walkingTicks * config.walkSpeed / speed);
				bool failed = false;

				for (int tick = 0; tick < ticks; ++tick)
				{
					simulate(state, {0, ramp.direction.x, ramp.direction.y, buttons}, config, world);

					if (state.mode != Mode::Grounded || std::abs(state.velocity.z) > .001f)
					{
						failed = true;

						if (firstFailure.empty())
						{
							firstFailure = std::to_string(ramp.angle) + " degrees, phase " + std::to_string(phase)
							               + ", buttons " + std::to_string(buttons) + ", tick " + std::to_string(tick);
						}
					}
				}

				if (ramp.angle <= 44)
				{
					failed |= dot(state.position - start, ramp.direction) < 250;
				}
				else
				{
					failed |= std::abs(state.position.z - start.z) > .5f;
				}

				failures += failed ? 1 : 0;
			}
		}
	}

	require(failures == 0, std::to_string(failures) + "/306 map approaches failed; first: " + firstFailure);
}

void playerTestLevelRollingEdge(const TriangleWorld& world)
{
	Config config;
	State state = placeOnFloor(world, {-1601.802145f, 2133.491604f, 0}, config);

	for (int tick = 0; tick < 117; ++tick)
	{
		simulate(state, {0, .766044878f, .642787092f, Roll}, config, world);
		require(state.velocity.z <= .001f, "Rolling along the 40 degree edge created upward velocity");
	}
}
} // namespace

int main(int argc, char** argv)
{
	int failures = 0;
	auto run = [&](const char* name, auto test)
	{
		try
		{
			test();
			std::cout << "PASS " << name << '\n';
		}
		catch (const std::exception& error)
		{
			++failures;
			std::cerr << "FAIL " << name << ": " << error.what() << '\n';
		}
	};

	run("grounded slope velocity", groundedSlopeDoesNotCreateJumpVelocity);
	run("airborne slope boost", fallingSlopeContactDoesNotBoostUpward);
	run("real jump and unsupported steep slope", jumpAndUnsupportedSteepSlope);
	run("floor snap obstruction", floorSnapDoesNotCrossSteepGeometry);

	if (argc > 1)
	{
		TriangleWorld world;
		std::ifstream map(argv[1]);

		if (!world.load(map))
		{
			std::cerr << "FAIL could not load PlayerTestLevel collision\n";
			return 1;
		}

		run("map support contacts", [&] { playerTestLevelContacts(world); });
		run("map approaches at 17 starting offsets", [&] { playerTestLevelApproaches(world); });
		run("map rolling edge", [&] { playerTestLevelRollingEdge(world); });
	}

	return failures == 0 ? 0 : 1;
}
