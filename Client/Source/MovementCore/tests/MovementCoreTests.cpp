#include "MovementReplay.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace hhv::movement;

void require(bool pass, const char* message)
{
	if (!pass)
		throw std::runtime_error(message);
}

void rect(std::vector<Triangle>& out, Vec3 a, Vec3 b, Vec3 c, Vec3 d)
{
	out.push_back({a, b, c});
	out.push_back({a, c, d});
}

std::vector<Triangle> floorGeometry()
{
	std::vector<Triangle> out;
	rect(out, {-2000, -2000, 0}, {2000, -2000, 0}, {2000, 2000, 0}, {-2000, 2000, 0});
	return out;
}

State resting()
{
	State s;
	s.position = {0, 0, 88.1f};
	s.mode = Mode::Grounded;
	return s;
}

void accelerationJump()
{
	TriangleWorld world;
	world.build(floorGeometry());
	Config c;
	State s = resting();
	simulate(s, {1, 1, 0, 0}, c, world);
	require(s.velocity.x > 0 && s.velocity.x < c.walkSpeed, "acceleration bypassed");

	for (int n = 0; n < 60; ++n)
		simulate(s, {0, 1, 1, 0}, c, world);
	require(length(s.velocity) <= c.walkSpeed + .1f, "diagonal speed boost");

	for (int n = 0; n < 60; ++n)
		simulate(s, {}, c, world);
	require(length(s.velocity) < .01f && s.mode == Mode::Grounded, "braking/ground state");
	float z = s.position.z;
	simulate(s, {0, 0, 0, Jump}, c, world);
	require(s.mode == Mode::Falling && s.velocity.z > 0, "jump rejected");
	simulate(s, {0, 0, 0, Jump}, c, world);
	require(s.velocity.z < c.jumpSpeed, "air jump accepted");

	for (int n = 0; n < 100; ++n)
		simulate(s, {}, c, world);
	require(s.mode == Mode::Grounded && std::abs(s.position.z - z) < .3f, "landing failed");
	std::cout << "PASS acceleration, diagonal clamp, braking, jump, air-jump rejection, landing\n";
}

void obstacles()
{
	auto geometry = floorGeometry();
	rect(geometry, {150, -1000, 0}, {150, 1000, 0}, {150, 1000, 500}, {150, -1000, 500});
	rect(geometry, {-1000, 150, 0}, {1000, 150, 0}, {1000, 150, 500}, {-1000, 150, 500});
	TriangleWorld world;
	world.build(geometry);
	Config c;
	State s = resting();

	for (int n = 0; n < 90; ++n)
		simulate(s, {0, 1, .3f, 0}, c, world);
	require(s.position.x < 117 && s.position.y > 60, "wall slide/thin wall failed");

	for (int n = 0; n < 120; ++n)
		simulate(s, {0, 1, 1, 0}, c, world);
	require(s.position.x < 117 && s.position.y < 117, "inside corner penetration");
	Hit fast = world.sweep({0, 0, 88}, {1000, 0, 0}, 34, 88);
	require(fast.blocking && fast.time < .12f, "continuous thin wall sweep failed");
	// Low ceiling must cancel upward velocity.
	geometry = floorGeometry();
	rect(geometry, {-1000, -1000, 200}, {1000, -1000, 200}, {1000, 1000, 200}, {-1000, 1000, 200});
	world.build(geometry);
	s = resting();
	simulate(s, {0, 0, 0, Jump}, c, world);

	for (int n = 0; n < 6; ++n)
		simulate(s, {}, c, world);
	require(s.position.z < 113 && s.velocity.z <= 0, "ceiling penetration");
	std::cout << "PASS wall sliding, corner, swept thin wall, ceiling\n";
}

void stairsSlopesLedges()
{
	Config c;
	auto geometry = floorGeometry();
	rect(geometry, {150, -500, 0}, {150, 500, 0}, {150, 500, 30}, {150, -500, 30});
	rect(geometry, {150, -500, 30}, {600, -500, 30}, {600, 500, 30}, {150, 500, 30});
	TriangleWorld world;
	world.build(geometry);
	State s = resting();

	for (int n = 0; n < 90; ++n)
		simulate(s, {0, 1, 0, 0}, c, world);
	require(s.position.x > 250 && s.position.z > 117, "step-up failed");
	geometry.clear();
	rect(geometry, {-1000, -1000, -500}, {1000, -1000, 500}, {1000, 1000, 500}, {-1000, 1000, -500});
	world.build(geometry);
	s = resting();
	s.position.z = 100;

	for (int n = 0; n < 100; ++n)
		simulate(s, {0, 1, 0, 0}, c, world);
	require(s.position.x > 300 && s.position.z > 240 && s.mode == Mode::Grounded, "walkable slope failed");
	geometry.clear();
	rect(geometry, {-1000, -500, 0}, {100, -500, 0}, {100, 500, 0}, {-1000, 500, 0});
	world.build(geometry);
	s = resting();

	for (int n = 0; n < 80; ++n)
		simulate(s, {0, 1, 0, 0}, c, world);
	require(s.position.x > 200 && s.position.z < 0 && s.mode == Mode::Falling, "ledge did not fall");
	std::cout << "PASS step, slope, ledge\n";
}

void portableFiles()
{
	TriangleWorld original, loaded;
	original.build(floorGeometry());
	std::stringstream file;
	require(original.save(file), "save geometry");
	require(loaded.load(file) && original.hash() == loaded.hash(), "geometry roundtrip/hash");
	const auto hash = loaded.hash();

	for (const char* malformed :
	     {"", "HHVCOLLISION 2 1 0", "HHVCOLLISION 1 2000001 0", "HHVCOLLISION 1 1 0\n0 0 0 1 0 0 0 1 0"})
	{
		std::istringstream invalid(malformed);
		require(!loaded.load(invalid) && loaded.hash() == hash, "invalid file replaced world");
	}
	std::deque<StepRecord> records;
	State state = resting();
	Config config;

	for (std::uint32_t tick = 1; tick <= 180; ++tick)
	{
		Input input{tick, .6f, 0, static_cast<std::uint8_t>(tick == 20 ? Jump : tick > 100 ? Run : 0)};
		const State before = state;
		simulate(state, input, config, loaded);
		records.push_back({input, config, before, state});
	}
	std::stringstream replay;
	require(saveReplay(replay, hash, records), "save replay");
	std::size_t count;
	require(verifyReplay(replay, loaded, count) && count == 180, "replay mismatch");
	records.back().after.position.x += 5;
	std::stringstream tampered;
	saveReplay(tampered, hash, records);
	require(!verifyReplay(tampered, loaded, count), "tampered replay accepted");
	std::cout << "PASS portable geometry roundtrip, invalid files, replay, mismatch rejection\n";
}

int main(int argc, char** argv)
{
	try
	{
		portableFiles();
		accelerationJump();
		obstacles();
		stairsSlopesLedges();

		if (argc > 1)
		{
			TriangleWorld world;
			std::ifstream file(argv[1]);
			require(world.load(file), "exported map failed to load");
			std::cout << "PASS exported geometry triangles=" << world.size() << " hash=" << world.hash()
			          << '\n';
		}

		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "FAIL " << e.what() << '\n';
		return 1;
	}
}
