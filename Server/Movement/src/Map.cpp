#include "Map.h"
#include <fstream>

namespace heaven {
namespace movement = hhv::movement;

movement::Config nav::Agent::config() const {
    movement::Config result;
    result.radius = radius;
    result.halfHeight = halfHeight;
    result.stepHeight = maxStepHeight;
    result.slopeDegrees = maxSlopeAngleDegrees;
    return result;
}

bool Map::loadFromFile(const std::string &path, std::string &error) {
    std::ifstream input(path);
    if (!input || !collision_.load(input)) {
        error = "cannot load canonical collision file: " + path;
        return false;
    }

    bounds_ = {};
    for (const auto &triangle : collision_.triangles()) {
        for (auto vertex : {triangle.a, triangle.b, triangle.c}) {
            vertex = toServer(vertex);
            if (!bounds_.valid) {
                bounds_.min = bounds_.max = vertex;
                bounds_.valid = true;
            }
            bounds_.min.x = std::min(bounds_.min.x, vertex.x);
            bounds_.min.y = std::min(bounds_.min.y, vertex.y);
            bounds_.min.z = std::min(bounds_.min.z, vertex.z);
            bounds_.max.x = std::max(bounds_.max.x, vertex.x);
            bounds_.max.y = std::max(bounds_.max.y, vertex.y);
            bounds_.max.z = std::max(bounds_.max.z, vertex.z);
        }
    }
    return true;
}

nav::Vec3 Map::toCore(nav::Vec3 value) const {
    return {value.x - originOffset_, value.y - originOffset_, value.z};
}

nav::Vec3 Map::toServer(nav::Vec3 value) const {
    return {value.x + originOffset_, value.y + originOffset_, value.z};
}

bool Map::canStandAt(float x, float y, const nav::Agent &agent, nav::Vec3 *grounded, float referenceZ) const {
    if (!loaded() || !std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }

    const auto config = agent.config();
    movement::State candidate;
    const bool hasReference = std::isfinite(referenceZ);
    candidate.position = toCore({x, y, hasReference ? referenceZ : bounds_.max.z + config.halfHeight});
    candidate.position.z += config.stepHeight + config.skin;
    const float distance = hasReference ? config.stepHeight * 2.f + config.floorSnap
                                        : bounds_.max.z - bounds_.min.z + config.halfHeight * 2.f;

    if (!movement::findFloor(candidate, config, collision_, distance)) {
        return false;
    }

    const auto overlap = collision_.sweep(candidate.position, {}, config.radius, config.halfHeight);
    if (overlap.penetration > config.skin * 2.f) {
        return false;
    }

    if (grounded) {
        *grounded = toServer(candidate.position);
    }
    return true;
}

bool Map::nearestStandable(float x, float y, float radius, const nav::Agent &agent, nav::Vec3 &result,
                           float referenceZ) const {
    if (canStandAt(x, y, agent, &result, referenceZ)) {
        return true;
    }

    const float spacing = std::max(agent.radius, 20.f);
    for (float ring = spacing; ring <= radius; ring += spacing) {
        const int samples = std::max(8, static_cast<int>(ring * 6.2831853f / spacing));
        for (int index = 0; index < samples; ++index) {
            const float angle = 6.2831853f * index / samples;
            if (canStandAt(x + std::cos(angle) * ring, y + std::sin(angle) * ring, agent, &result,
                           referenceZ)) {
                return true;
            }
        }
    }
    return false;
}

bool Map::blockedAlong(const nav::Vec3 &from, const nav::Vec3 &to, const nav::Agent &agent) const {
    if (!loaded() || !movement::finite(from) || !movement::finite(to)) {
        return true;
    }

    const auto config = agent.config();
    movement::State probe;
    probe.position = toCore(from);
    probe.mode = movement::Mode::Grounded;
    const auto goal = toCore(to);
    const auto segment = goal - probe.position;

    // Clear segments with continuous floor support need no acceleration simulation.
    // The core's capsule and slope rules still decide whether the route is usable.
    const auto obstruction = collision_.sweep(probe.position, segment, config.radius, config.halfHeight);
    if (!obstruction.blocking) {
        const float spacing = std::max(config.radius * .5f, 1.f);
        const int samples = static_cast<int>(std::ceil(movement::length(segment) / spacing));
        bool supported = samples <= 4096;
        for (int sample = 0; supported && sample <= samples; ++sample) {
            movement::State floor;
            floor.position = probe.position + segment * (static_cast<float>(sample) / std::max(samples, 1));
            const float expectedZ = floor.position.z;
            supported = movement::findFloor(floor, config, collision_, config.floorSnap) &&
                        std::abs(floor.position.z - expectedZ) <= config.skin * 2.f;
        }
        if (supported) {
            return false;
        }
    }

    // Stairs and changing slopes need the full walking simulation.
    const int steps = std::min(1800, static_cast<int>(movement::length(goal - probe.position) /
                                                      (config.walkSpeed * movement::FixedDt)) +
                                         60);
    for (int index = 0; index < steps; ++index) {
        const auto delta = goal - probe.position;
        const float horizontal = std::hypot(delta.x, delta.y);
        if (horizontal < 5.f && std::abs(delta.z) <= config.stepHeight) {
            return false;
        }

        const auto direction = movement::normalized({delta.x, delta.y, 0});
        movement::Input input;
        input.x = direction.x;
        input.y = direction.y;
        const auto previous = probe.position;
        movement::simulate(probe, input, config, collision_);
        if (probe.mode != movement::Mode::Grounded ||
            (index > 4 && movement::length(probe.position - previous) < .01f)) {
            return true;
        }
    }
    return true;
}

void Map::advance(movement::State &state, float &accumulator, float dt, nav::Vec3 target, bool moving,
                  float speed) const {
    auto config = agent_.config();
    config.walkSpeed = std::max(speed, 1.f);
    config.runSpeed = config.walkSpeed;
    accumulator = std::min(accumulator + std::max(dt, 0.f), .25f);
    const auto goal = toCore(target);

    while (accumulator + 1e-7f >= movement::FixedDt) {
        const auto delta = goal - state.position;
        const float distance = std::hypot(delta.x, delta.y);
        movement::Input input;
        if (moving && distance > 3.f) {
            // Approach short waypoints slowly enough to turn without carrying momentum
            // across the edge of the supporting floor.
            const float arrivalSpeed = std::sqrt(2.f * config.braking * std::max(distance - 3.f, 0.f)) * .5f;
            const float strength = std::min(1.f, arrivalSpeed / config.walkSpeed);
            input.x = delta.x / distance * strength;
            input.y = delta.y / distance * strength;
        }
        movement::simulate(state, input, config, collision_);
        accumulator -= movement::FixedDt;
    }
}
} // namespace heaven
