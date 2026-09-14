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
    loaded_ = false;
    error.clear();
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
    loaded_ = navigation_.build(collision_, agent_.config(), error);
    return loaded_;
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
    // 이 NavMesh는 맵의 기본 캡슐 규격으로 만들어졌다. 다른 크기를 조용히 통과시키지 않는다.
    if (!loaded() || agent.radius != agent_.radius || agent.halfHeight != agent_.halfHeight ||
        agent.maxStepHeight != agent_.maxStepHeight || agent.maxSlopeAngleDegrees != agent_.maxSlopeAngleDegrees) {
        return true;
    }
    return !navigation_.clearSegment(toCore(from), toCore(to));
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
