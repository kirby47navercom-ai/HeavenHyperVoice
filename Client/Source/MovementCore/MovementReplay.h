#pragma once

#include "TriangleWorld.h"
#include <deque>

namespace hhv::movement
{
struct StepRecord
{
	Input input;
	Config config;
	State before, after;
};

namespace replay
{
inline void write(std::ostream& out, const Config& v)
{
	out << ' ' << v.radius << ' ' << v.halfHeight << ' ' << v.walkSpeed << ' ' << v.runSpeed << ' '
	    << v.acceleration << ' ' << v.braking << ' ' << v.friction << ' ' << v.gravity << ' ' << v.jumpSpeed
	    << ' ' << v.airControl << ' ' << v.terminalSpeed << ' ' << v.stepHeight << ' ' << v.slopeDegrees
	    << ' ' << v.floorSnap << ' ' << v.skin << ' ' << v.rotationSpeed << ' ' << v.rollSpeed << ' '
	    << v.rollTicks << '\n';
}

inline bool read(std::istream& in, Config& v)
{
	if (!(in >> v.radius >> v.halfHeight >> v.walkSpeed >> v.runSpeed >> v.acceleration >> v.braking >>
	      v.friction >> v.gravity >> v.jumpSpeed >> v.airControl >> v.terminalSpeed >> v.stepHeight >>
	      v.slopeDegrees >> v.floorSnap >> v.skin >> v.rotationSpeed >> v.rollSpeed >> v.rollTicks))
		return false;
	return std::isfinite(v.radius) && std::isfinite(v.halfHeight) && std::isfinite(v.walkSpeed) &&
	       std::isfinite(v.runSpeed) && std::isfinite(v.acceleration) && std::isfinite(v.braking) &&
	       std::isfinite(v.friction) && std::isfinite(v.gravity) && std::isfinite(v.jumpSpeed) &&
	       std::isfinite(v.airControl) && std::isfinite(v.terminalSpeed) && std::isfinite(v.stepHeight) &&
	       std::isfinite(v.slopeDegrees) && std::isfinite(v.floorSnap) && std::isfinite(v.skin) &&
	       std::isfinite(v.rotationSpeed) && std::isfinite(v.rollSpeed) && v.radius > 0 &&
	       v.halfHeight >= v.radius && v.walkSpeed >= 0 && v.runSpeed >= 0 && v.acceleration >= 0 &&
	       v.braking >= 0 && v.friction >= 0 && v.gravity >= 0 && v.jumpSpeed >= 0 && v.airControl >= 0 &&
	       v.airControl <= 1 && v.terminalSpeed > 0 && v.stepHeight >= 0 && v.slopeDegrees >= 0 &&
	       v.slopeDegrees < 90 && v.floorSnap >= 0 && v.skin > 0 && v.rollSpeed >= 0;
}

inline void write(std::ostream& out, const State& v)
{
	out << ' ' << v.position.x << ' ' << v.position.y << ' ' << v.position.z << ' ' << v.velocity.x << ' '
	    << v.velocity.y << ' ' << v.velocity.z << ' ' << v.acceleration.x << ' ' << v.acceleration.y << ' '
	    << v.acceleration.z << ' ' << v.floorNormal.x << ' ' << v.floorNormal.y << ' ' << v.floorNormal.z
	    << ' ' << v.facing << ' ' << v.rollDirection.x << ' ' << v.rollDirection.y << ' ' << v.rollDirection.z
	    << ' ' << unsigned(v.mode) << ' ' << v.rollRemaining << ' ' << v.wallSliding << '\n';
}

inline bool read(std::istream& in, State& v)
{
	unsigned mode = 0;

	if (!(in >> v.position.x >> v.position.y >> v.position.z >> v.velocity.x >> v.velocity.y >>
	      v.velocity.z >> v.acceleration.x >> v.acceleration.y >> v.acceleration.z >> v.floorNormal.x >>
	      v.floorNormal.y >> v.floorNormal.z >> v.facing >> v.rollDirection.x >> v.rollDirection.y >>
	      v.rollDirection.z >> mode >> v.rollRemaining >> v.wallSliding))
		return false;
	v.mode = static_cast<Mode>(mode);
	return std::isfinite(v.position.x) && std::isfinite(v.position.y) && std::isfinite(v.position.z) &&
	       std::isfinite(v.velocity.x) && std::isfinite(v.velocity.y) && std::isfinite(v.velocity.z) &&
	       std::isfinite(v.acceleration.x) && std::isfinite(v.acceleration.y) &&
	       std::isfinite(v.acceleration.z) && std::isfinite(v.floorNormal.x) &&
	       std::isfinite(v.floorNormal.y) && std::isfinite(v.floorNormal.z) && std::isfinite(v.facing) &&
	       std::isfinite(v.rollDirection.x) && std::isfinite(v.rollDirection.y) &&
	       std::isfinite(v.rollDirection.z) && mode <= 2;
}

inline bool near(const State& a, const State& b, float epsilon = .001f)
{
	return length(a.position - b.position) <= epsilon && length(a.velocity - b.velocity) <= epsilon &&
	       length(a.acceleration - b.acceleration) <= epsilon &&
	       length(a.floorNormal - b.floorNormal) <= epsilon && std::abs(a.facing - b.facing) <= epsilon &&
	       length(a.rollDirection - b.rollDirection) <= epsilon && a.mode == b.mode &&
	       a.rollRemaining == b.rollRemaining && a.wallSliding == b.wallSliding;
}
} // namespace replay

inline bool saveReplay(std::ostream& out, std::uint64_t worldHash, const std::deque<StepRecord>& records)
{
	if (records.empty())
		return false;
	out.imbue(std::locale::classic());
	out << std::setprecision(std::numeric_limits<float>::max_digits10);
	out << "HHVREPLAY " << Version << ' ' << worldHash << ' ' << records.size() << '\n';
	replay::write(out, records.front().before);

	for (const auto& r : records)
	{
		out << r.input.sequence << ' ' << r.input.x << ' ' << r.input.y << ' ' << unsigned(r.input.buttons)
		    << '\n';
		replay::write(out, r.config);
		replay::write(out, r.after);
	}

	return bool(out);
}

// Offline comparison helper; a server calls simulate() directly with its own authoritative state.
inline bool verifyReplay(std::istream& in, const TriangleWorld& world, std::size_t& verified)
{
	verified = 0;
	in.imbue(std::locale::classic());
	std::string magic;
	std::uint32_t version = 0;
	std::uint64_t hash = 0;
	std::size_t count = 0;
	State state;

	if (!(in >> magic >> version >> hash >> count) || magic != "HHVREPLAY" || version != Version ||
	    hash != world.hash() || count == 0 || count > 1000000 || !replay::read(in, state))
		return false;
	std::uint32_t previous = 0;

	for (std::size_t i = 0; i < count; ++i)
	{
		Input input;
		Config config;
		State expected;
		unsigned buttons = 0;

		if (!(in >> input.sequence >> input.x >> input.y >> buttons) || buttons > 7 ||
		    (i && input.sequence != previous + 1) || !replay::read(in, config) || !replay::read(in, expected))
			return false;
		input.buttons = static_cast<std::uint8_t>(buttons);

		if (!valid(input))
			return false;
		previous = input.sequence;
		simulate(state, input, config, world);

		if (!replay::near(state, expected))
			return false;
		++verified;
	}
	in >> std::ws;
	return in.eof();
}
} // namespace hhv::movement
