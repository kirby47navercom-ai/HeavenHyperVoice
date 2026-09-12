#include "MovementReplay.h"
#include <fstream>
#include <iostream>

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cerr << "Usage: MovementReplay world.hhvcollision inputs.hhvreplay\n";
		return 2;
	}
	hhv::movement::TriangleWorld world;
	std::ifstream geometry(argv[1]), inputs(argv[2]);

	if (!world.load(geometry))
	{
		std::cerr << "Invalid collision file\n";
		return 1;
	}
	std::size_t verified = 0;

	if (!hhv::movement::verifyReplay(inputs, world, verified))
	{
		std::cerr << "Replay rejected after " << verified << " verified ticks\n";
		return 1;
	}
	std::cout << "PASS " << verified << " ticks; triangles=" << world.size() << " hash=" << world.hash()
	          << '\n';
}
