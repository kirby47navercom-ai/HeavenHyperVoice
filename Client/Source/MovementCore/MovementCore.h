#pragma once

// Engine-independent, Z-up, centimetres. One command always advances exactly one tick.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hhv::movement
{
constexpr float FixedDt = 1.f / 60.f;

// Simulation/replay compatibility. The independent collision file format is still version 1.
constexpr std::uint32_t Version = 2;

struct Vec3
{
	float x = 0, y = 0, z = 0;

	Vec3 operator+(Vec3 b) const
	{
		return {x + b.x, y + b.y, z + b.z};
	}

	Vec3 operator-(Vec3 b) const
	{
		return {x - b.x, y - b.y, z - b.z};
	}

	Vec3 operator*(float s) const
	{
		return {x * s, y * s, z * s};
	}

	Vec3 operator/(float s) const
	{
		return *this * (1.f / s);
	}

	Vec3& operator+=(Vec3 b)
	{
		return *this = *this + b;
	}
};

inline float dot(Vec3 a, Vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(Vec3 a, Vec3 b)
{
	return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float length(Vec3 v)
{
	return std::sqrt(dot(v, v));
}

inline Vec3 normalized(Vec3 v)
{
	float n = length(v);
	return n > 1e-6f ? v / n : Vec3{};
}

inline bool finite(Vec3 v)
{
	return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline Vec3 clampLength(Vec3 v, float max)
{
	float n = length(v);
	return n > max && n > 0 ? v * (max / n) : v;
}

inline Vec3 approach(Vec3 v, Vec3 target, float amount)
{
	return v + clampLength(target - v, amount);
}

inline Vec3 slide(Vec3 v, Vec3 normal)
{
	return v - normal * std::min(0.f, dot(v, normal));
}

enum class Mode : std::uint8_t
{
	Grounded,
	Falling,
	Disabled
};

enum Buttons : std::uint8_t
{
	Jump = 1,
	Run = 2,
	Roll = 4
};

struct Config
{
	float radius = 34, halfHeight = 88;
	float walkSpeed = 260, runSpeed = 390, acceleration = 2048, braking = 2048, friction = 8;
	float gravity = 980, jumpSpeed = 420, airControl = .35f, terminalSpeed = 4000;
	float stepHeight = 45, slopeDegrees = 44, floorSnap = 3, skin = .1f;
	float rotationSpeed = 540, rollSpeed = 600;
	std::uint16_t rollTicks = 36;
};

struct Input
{
	std::uint32_t sequence = 0;
	float x = 0, y = 0;
	std::uint8_t buttons = 0;
};

struct State
{
	Vec3 position, velocity, acceleration, floorNormal{0, 0, 1};
	float facing = 0;
	Mode mode = Mode::Falling;
	std::uint16_t rollRemaining = 0;
	Vec3 rollDirection;
	bool wallSliding = false;
};

struct Hit
{
	float time = 1;
	Vec3 normal{0, 0, 1};
	bool blocking = false;
	float penetration = 0;
	Vec3 surfaceNormal{0, 0, 1};
};

// Implementations must return earliest swept upright capsule contact, with unit normal.
class CollisionWorld
{
public:
	virtual ~CollisionWorld() = default;
	virtual Hit sweep(Vec3 start, Vec3 delta, float radius, float halfHeight) const = 0;

	// Support queries may look past a side contact, but must not move the capsule themselves.
	// Worlds with multiple contact candidates should return the nearest supporting face.
	virtual Hit sweepFloor(Vec3 start, float distance, float radius, float halfHeight,
	                       float minimumNormalZ) const
	{
		const Hit hit = sweep(start, {0, 0, -distance}, radius, halfHeight);

		if (hit.surfaceNormal.z < minimumNormalZ || hit.normal.z <= .001f)
		{
			return {};
		}

		return hit;
	}
};

inline bool valid(const Input& in)
{
	return std::isfinite(in.x) && std::isfinite(in.y) && std::abs(in.x) <= 1.001f &&
	       std::abs(in.y) <= 1.001f && (in.buttons & ~(Jump | Run | Roll)) == 0;
}

inline bool walkable(Vec3 n, const Config& c)
{
	return n.z >= std::cos(c.slopeDegrees * .01745329252f);
}

inline bool supportsCapsule(const Hit& hit, const Config& config)
{
	// The rounded bottom can rest on a walkable ledge even when its contact normal
	// is steeper than the ledge itself. A side/underside contact cannot support us.
	return hit.blocking && walkable(hit.surfaceNormal, config) && hit.normal.z > .001f &&
	       hit.penetration <= config.skin * 2;
}

inline bool findFloor(State& state, const Config& config, const CollisionWorld& world, float distance)
{
	const Hit obstruction = world.sweep(state.position, {0, 0, -distance}, config.radius, config.halfHeight);
	Hit floor = obstruction;

	if (!supportsCapsule(floor, config))
	{
		const float minimumNormalZ = std::cos(config.slopeDegrees * .01745329252f);
		floor = world.sweepFloor(state.position, distance, config.radius, config.halfHeight, minimumNormalZ);
	}

	if (!supportsCapsule(floor, config))
	{
		return false;
	}

	const float floorDistance = distance * floor.time;
	float snapDistance = std::max(0.f, floorDistance - config.skin);

	if (obstruction.blocking)
	{
		const float obstructionDistance = distance * obstruction.time;

		// A nearby side contact must not hide support, but we must never snap through it.
		if (obstruction.penetration > config.skin * 2 || floorDistance > obstructionDistance + config.skin)
		{
			return false;
		}

		snapDistance = std::min(snapDistance, std::max(0.f, obstructionDistance - config.skin));
	}

	state.position.z -= snapDistance;
	state.floorNormal = floor.surfaceNormal;
	state.mode = Mode::Grounded;
	state.velocity.z = 0;
	return true;
}

inline bool stepUp(State& s, Vec3 delta, const Config& c, const CollisionWorld& world)
{
	if (c.stepHeight <= 0 || length({delta.x, delta.y, 0}) < 1e-5f)
		return false;
	const Vec3 up{0, 0, c.stepHeight + c.skin};

	if (world.sweep(s.position, up, c.radius, c.halfHeight).blocking)
		return false;
	State candidate = s;
	candidate.position += up;
	delta.z = 0;
	const Hit forward = world.sweep(candidate.position, delta, c.radius, c.halfHeight);

	if (forward.blocking)
		return false;
	candidate.position += delta;

	if (!findFloor(candidate, c, world, c.stepHeight + c.floorSnap + c.skin))
		return false;

	if (candidate.position.z > s.position.z + c.stepHeight + c.skin * 2)
		return false;
	s = candidate;
	return true;
}

inline void simulate(State& s, const Input& in, const Config& c, const CollisionWorld& world)
{
	if (!valid(in) || !finite(s.position) || !finite(s.velocity) || s.mode == Mode::Disabled)
		return;
	s.wallSliding = false;
	// Small bounded depenetration for spawn/contact rounding. Never teleport through a wall.
	for (int i = 0; i < 4; ++i)
	{
		const Hit overlap = world.sweep(s.position, {}, c.radius, c.halfHeight);

		if (!overlap.blocking || overlap.penetration <= c.skin)
			break;
		s.position += overlap.normal * std::min(overlap.penetration + c.skin, c.radius * .5f);
	}

	if (s.mode == Mode::Grounded && !findFloor(s, c, world, c.floorSnap + c.skin))
		s.mode = Mode::Falling;
	Vec3 wish = clampLength({in.x, in.y, 0}, 1);
	const bool grounded = s.mode == Mode::Grounded;

	if ((in.buttons & Roll) && grounded && s.rollRemaining == 0)
	{
		s.rollRemaining = c.rollTicks;
		s.rollDirection = length(wish) > .01f ? normalized(wish)
		                                      : Vec3{std::cos(s.facing * .01745329252f),
		                                             std::sin(s.facing * .01745329252f), 0};
	}

	if ((in.buttons & Jump) && grounded && s.rollRemaining == 0)
	{
		s.velocity.z = c.jumpSpeed;
		s.mode = Mode::Falling;
	}
	s.acceleration = wish * (c.acceleration * (s.mode == Mode::Grounded ? 1.f : c.airControl));
	Vec3 horizontal{s.velocity.x, s.velocity.y, 0};

	if (s.rollRemaining > 0)
	{
		horizontal = s.rollDirection * c.rollSpeed;
		--s.rollRemaining;
	}
	else if (length(wish) > 1e-5f)
	{
		const float speed = (in.buttons & Run) ? c.runSpeed : c.walkSpeed;
		horizontal = approach(horizontal, wish * speed, length(s.acceleration) * FixedDt);
	}
	else if (s.mode == Mode::Grounded)
	{
		horizontal = approach(horizontal, {}, (c.braking + c.friction * length(horizontal)) * FixedDt);
	}
	s.velocity.x = horizontal.x;
	s.velocity.y = horizontal.y;

	if (s.mode == Mode::Falling)
		s.velocity.z = std::max(-c.terminalSpeed, s.velocity.z - c.gravity * FixedDt);
	else
		s.velocity.z = 0;
	Vec3 delta = s.velocity * FixedDt;

	if (s.mode == Mode::Grounded && s.floorNormal.z > 1e-4f)
	{
		// Preserve horizontal speed while following walkable slopes.
		delta.z = -(delta.x * s.floorNormal.x + delta.y * s.floorNormal.y) / s.floorNormal.z;
	}
	Vec3 firstNormal;

	for (int iteration = 0; iteration < 4 && length(delta) > 1e-5f; ++iteration)
	{
		const Hit hit = world.sweep(s.position, delta, c.radius, c.halfHeight);

		if (!hit.blocking)
		{
			s.position += delta;
			break;
		}
		const float travel = std::max(0.f, hit.time - c.skin / std::max(length(delta), c.skin));
		s.position += delta * travel;
		Vec3 remainder = delta * (1.f - travel);

		const bool supportingContact = supportsCapsule(hit, c);
		const bool stepContact = !walkable(hit.surfaceNormal, c) || !walkable(hit.normal, c);

		if (s.mode == Mode::Grounded && stepContact && stepUp(s, remainder, c, world))
		{
			break;
		}

		if (supportingContact && s.velocity.z <= 0)
		{
			s.mode = Mode::Grounded;
			s.floorNormal = hit.surfaceNormal;
			s.velocity.z = 0;
		}
		else if (std::abs(hit.normal.z) < .7f)
			s.wallSliding = true;
		Vec3 normal = hit.normal;

		// Ground movement treats steep faces as walls. Falling must retain the real
		// contact normal so gravity can slide the capsule down an unsupported slope.
		if (s.mode == Mode::Grounded && !supportingContact && normal.z > 0)
		{
			normal = normalized({normal.x, normal.y, 0});
		}

		const float previousVerticalSpeed = s.velocity.z;
		delta = slide(remainder, normal);
		s.velocity = slide(s.velocity, normal);

		if (s.mode == Mode::Grounded)
		{
			// Slopes change the travelled height, not the character's jump velocity.
			s.velocity.z = 0;
		}
		else
		{
			// A collision may reduce a real jump, but must never add upward momentum.
			s.velocity.z = std::min(s.velocity.z, std::max(0.f, previousVerticalSpeed));
			delta.z = std::min(delta.z, std::max(0.f, remainder.z));
		}

		if (iteration == 0)
			firstNormal = normal;
		else if (dot(delta, firstNormal) < -1e-5f)
		{
			Vec3 crease = normalized(cross(firstNormal, normal));
			delta = crease * dot(remainder, crease);

			if (dot(delta, remainder) <= 0)
				delta = {};
		}

		// The two-wall adjustment above must obey the same no-boost rule.
		if (s.mode == Mode::Falling)
		{
			delta.z = std::min(delta.z, std::max(0.f, remainder.z));
		}
	}

	if (s.velocity.z <= 0)
	{
		const float snap = s.mode == Mode::Grounded ? c.stepHeight + c.floorSnap : c.floorSnap;

		if (!findFloor(s, c, world, snap))
			s.mode = Mode::Falling;
	}

	if (length(wish) > .01f)
	{
		float target = std::atan2(wish.y, wish.x) * 57.295779513f;
		float angle = std::remainder(target - s.facing, 360.f);
		s.facing = std::remainder(
		    s.facing + std::clamp(angle, -c.rotationSpeed * FixedDt, c.rotationSpeed * FixedDt), 360.f);
	}
}
} // namespace hhv::movement
