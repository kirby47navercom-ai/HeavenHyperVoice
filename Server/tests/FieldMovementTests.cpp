#include "Map.h"
#include "Path.h"
#include "PartnerFollower.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct Geometry {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("hhv-core-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".hhvcollision");
    std::vector<hhv::movement::Triangle> triangles;

    ~Geometry() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    void rectangle(float x0, float y0, float x1, float y1, float base = 0.f, float slope = 0.f) {
        const auto point = [&](float x, float y) { return hhv::movement::Vec3{x, y, base + slope * x}; };
        triangles.push_back({point(x0, y0), point(x1, y0), point(x1, y1)});
        triangles.push_back({point(x0, y0), point(x1, y1), point(x0, y1)});
    }

    void load(heaven::Map &map) {
        hhv::movement::TriangleWorld world;
        world.build(triangles);
        std::ofstream output(path);
        require(world.save(output), "could not save test collision");
        output.close();
        std::string error;
        if (!map.loadFromFile(path.string(), error)) {
            throw std::runtime_error(error);
        }
    }
};

heaven::nav::Vec3 ground(const heaven::Map &map, float x, float y, float z = NAN) {
    heaven::nav::Vec3 p;
    require(map.canStandAt(x, y, map.agent(), &p, z), "standable point rejected");
    require(std::hypot(p.x - x, p.y - y) <= 0.5f, "ground query changed XY");
    return p;
}

void slopeAndLayers() {
    Geometry geometry;
    geometry.rectangle(0, 0, 2000, 2000, -12000, 0.6f);
    geometry.rectangle(0, 0, 2000, 2000, 1000);
    heaven::Map map(0.f);
    geometry.load(map);
    auto from = ground(map, 200, 500, -11790);
    for (int x = 205; x <= 1700; x += 5) {
        const auto to = ground(map, static_cast<float>(x), 500, from.z);
        const float expectedZ = -12000.f + 0.6f * x + 88.f;
        if (std::abs(to.z - expectedZ) >= 10.f) {
            throw std::runtime_error("incorrect slope/layer at x=" + std::to_string(x) +
                                     " z=" + std::to_string(to.z) + " expected=" + std::to_string(expectedZ));
        }
        if (map.blockedAlong(from, to, map.agent())) {
            throw std::runtime_error("continuous slope blocked x=" + std::to_string(from.x) + " -> " +
                                     std::to_string(to.x) + " z=" + std::to_string(from.z) + " -> " +
                                     std::to_string(to.z));
        }
        from = to;
    }
    const auto upper = ground(map, 1000, 500, 1100);
    require(upper.z > 1000, "upper layer not selected");
    require(map.blockedAlong(from, upper, map.agent()), "2D raycast allowed a layer change");
    heaven::nav::Vec3 near;
    require(!map.canStandAt(-100, 500, map.agent()), "standability silently snapped outside XY");
    require(map.nearestStandable(-100, 500, 250, map.agent(), near, -11900),
            "explicit nearest fallback failed");
    std::cout << "PASS slope, exact XY, stacked layers, explicit projection\n";
}

void cornersAndParallelPaths() {
    Geometry geometry;
    geometry.rectangle(0, 0, 1000, 3000);
    geometry.rectangle(1000, 2000, 2000, 3000);
    geometry.rectangle(2000, 0, 3000, 3000);
    heaven::Map map(0.f);
    geometry.load(map);
    const auto start = ground(map, 500, 500);
    const auto goal = ground(map, 2400, 650);
    require(map.blockedAlong(start, goal, map.agent()), "hole was traversable");
    const auto path = heaven::Pathfinder{}.find(map, start, goal, map.agent());
    require(path.found && path.points.size() > 2, "path did not route around the hole");

    std::vector<std::future<void>> workers;
    for (int worker = 0; worker < 2; ++worker) {
        workers.push_back(std::async(std::launch::async, [&] {
            for (int i = 0; i < 3; ++i) {
                const auto found = heaven::Pathfinder{}.find(map, start, goal, map.agent());
                require(found.found && found.points.size() == path.points.size(), "parallel path changed");
            }
        }));
    }
    for (auto &worker : workers) {
        worker.get();
    }

    using namespace heaven::fieldshared;
    PartnerOwnerState owner{ground(map, 2250, 500), {}, 0};
    PartnerState partner;
    require(PartnerFollower::initialize(owner, 20, partner, &map), "partner initialization failed");
    partner.location = start;
    partner.movement.position = map.toCore(start);
    partner.movement.mode = hhv::movement::Mode::Grounded;
    PartnerFollowConfig config;
    config.teleportDistance = 10000;
    for (int tick = 0; tick < 1000; ++tick) {
        const auto previous = partner.location;
        PartnerFollower::update(0.05f, owner, 20, partner, &map, config);
        if (partner.teleportedThisTick) {
            std::cerr << "Route:";
            for (const auto &point : path.points) {
                std::cerr << " (" << point.x << ',' << point.y << ')';
            }
            std::cerr << '\n';
            throw std::runtime_error("reachable route used stuck teleport at tick=" + std::to_string(tick) +
                                     " from=" + std::to_string(previous.x) + "," +
                                     std::to_string(previous.y));
        }
        if (map.blockedAlong(previous, partner.location, map.agent())) {
            throw std::runtime_error(
                "partner cut a corner at tick " + std::to_string(tick) + " from " +
                std::to_string(previous.x) + "," + std::to_string(previous.y) + "," +
                std::to_string(previous.z) + " to " + std::to_string(partner.location.x) + "," +
                std::to_string(partner.location.y) + "," + std::to_string(partner.location.z));
        }
    }
    require(std::hypot(partner.location.x - goal.x, partner.location.y - goal.y) <= 46,
            "partner failed to finish its route");
    std::cout << "PASS concurrent core paths and partner corner following\n";
}

void partnerRecovery() {
    Geometry geometry;
    geometry.rectangle(0, 0, 1000, 2000);
    geometry.rectangle(1200, 0, 2400, 2000);
    heaven::Map map(0.f);
    geometry.load(map);
    using namespace heaven::fieldshared;
    PartnerOwnerState owner{ground(map, 1350, 1000), {}, 90};
    PartnerState partner;
    require(PartnerFollower::initialize(owner, 20, partner, &map), "initial partner missing");
    partner.location = ground(map, 900, 1000);
    partner.movement.position = map.toCore(partner.location);
    partner.movement.mode = hhv::movement::Mode::Grounded;
    bool recovered = false;
    for (int tick = 0; tick < 60; ++tick) {
        PartnerFollower::update(0.05f, owner, 20, partner, &map);
        recovered |= partner.teleportedThisTick;
    }
    require(recovered, "disconnected partner did not recover");
    require(map.canStandAt(partner.location.x, partner.location.y, map.agent()), "recovery outside navmesh");
    std::cout << "PASS bounded partner recovery\n";
}

void exportedMap(const std::filesystem::path &file, float origin) {
    heaven::Map map(origin);
    std::string error;
    if (!map.loadFromFile(file.string(), error)) {
        throw std::runtime_error(error);
    }
    const auto start = ground(map, origin, origin);
    using namespace heaven::fieldshared;
    PartnerOwnerState owner{start, {}, 0};
    PartnerState partner;
    require(PartnerFollower::initialize(owner, 20, partner, &map),
            "exported map partner initialization failed");
    for (int tick = 0; tick < 200; ++tick) {
        const auto next = ground(map, start.x + (tick + 1) * 5.f, start.y, owner.location.z);
        require(!map.blockedAlong(owner.location, next, map.agent()), "exported map origin walk rejected");
        owner.location = next;
        owner.velocity = {100, 0, 0};
        PartnerFollower::update(0.05f, owner, 20, partner, &map);
        require(!partner.teleportedThisTick, "exported map partner stalled on nearby slope");
    }
    require(partner.location.x > start.x + 800, "exported map partner did not follow");
    std::cout << "PASS " << file.filename().string()
              << " 10m player path and partner follow; triangles=" << map.triangleCount() << '\n';
}

} // namespace

int main(int argc, char **argv) {
    try {
        slopeAndLayers();
        cornersAndParallelPaths();
        partnerRecovery();
        if (argc > 1) {
            exportedMap(argv[1], 153600.f);
            const auto collisionRoot = std::filesystem::path(argv[1]).parent_path().parent_path();
            const auto goldenrod = collisionRoot / "Environments/Goldenrod_R03/Maps/L_Goldenrod.hhvcollision";
            if (std::filesystem::exists(goldenrod)) {
                exportedMap(goldenrod, 25600.f);
            }
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
