#pragma once

#include "MovementPrediction.h"
#include "TriangleWorld.h"
#include <limits>
#include <string>

namespace heaven {
namespace nav {
using Vec3 = hhv::movement::Vec3;

struct Aabb {
    Vec3 min;
    Vec3 max;
    bool valid = false;
};

struct Agent {
    float radius = 34.f;
    float halfHeight = 88.f;
    float maxStepHeight = 45.f;
    float maxSlopeAngleDegrees = 44.f;
    int maxSearchNodes = 4096;

    hhv::movement::Config config() const;
};
} // namespace nav

// The world offset belongs to persistence and visibility, never to core simulation.
class Map {
  public:
    explicit Map(float originOffset = 25600.f) : originOffset_(originOffset) {}
    bool loadFromFile(const std::string &path, std::string &error);
    bool loaded() const {
        return collision_.size() != 0;
    }
    std::size_t triangleCount() const {
        return collision_.size();
    }
    const nav::Aabb &bounds() const {
        return bounds_;
    }
    const nav::Agent &agent() const {
        return agent_;
    }
    const hhv::movement::TriangleWorld &collision() const {
        return collision_;
    }

    nav::Vec3 toCore(nav::Vec3 value) const;
    nav::Vec3 toServer(nav::Vec3 value) const;

    bool canStandAt(float x, float y, const nav::Agent &agent, nav::Vec3 *grounded = nullptr,
                    float referenceZ = std::numeric_limits<float>::quiet_NaN()) const;
    bool nearestStandable(float x, float y, float radius, const nav::Agent &agent, nav::Vec3 &result,
                          float referenceZ = std::numeric_limits<float>::quiet_NaN()) const;
    bool blockedAlong(const nav::Vec3 &from, const nav::Vec3 &to, const nav::Agent &agent) const;

    // AI selects an input direction; the same fixed-step core resolves the actual motion.
    void advance(hhv::movement::State &state, float &accumulator, float dt, nav::Vec3 target, bool moving,
                 float speed) const;

  private:
    hhv::movement::TriangleWorld collision_;
    nav::Agent agent_;
    nav::Aabb bounds_;
    float originOffset_;
};
} // namespace heaven
