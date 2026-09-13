#include "Path.h"
#include <map>
#include <queue>
#include <tuple>

namespace heaven {
PathResult Pathfinder::find(const Map &map, const nav::Vec3 &start, const nav::Vec3 &goal,
                            const nav::Agent &agent) const {
    if (!map.blockedAlong(start, goal, agent)) {
        return {true, {start, goal}};
    }

    // A* edges are checked by actually walking a capsule through the common core.
    // Height is part of the key so bridges and the ground below remain separate.
    const float spacing = std::max(64.f, agent.radius * 2.f);
    using Key = std::tuple<int, int, int>;
    const auto keyOf = [&](nav::Vec3 point) {
        return Key{static_cast<int>(std::round((point.x - start.x) / spacing)),
                   static_cast<int>(std::round((point.y - start.y) / spacing)),
                   static_cast<int>(std::round(point.z / 20.f))};
    };

    struct Node {
        nav::Vec3 point;
        float cost;
        int parent;
    };
    using Candidate = std::pair<float, int>;
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>> open;
    std::vector<Node> nodes{{start, 0, -1}};
    std::map<Key, float> costs{{keyOf(start), 0.f}};
    open.push({hhv::movement::length(goal - start), 0});

    int expanded = 0;
    while (!open.empty() && expanded++ < agent.maxSearchNodes) {
        const int index = open.top().second;
        open.pop();
        const Node current = nodes[index];
        if (current.cost > costs[keyOf(current.point)] + .01f) {
            continue;
        }

        if (hhv::movement::length(goal - current.point) < spacing * 2.f &&
            !map.blockedAlong(current.point, goal, agent)) {
            PathResult result{true, {goal}};
            for (int parent = index; parent >= 0; parent = nodes[parent].parent) {
                result.points.push_back(nodes[parent].point);
            }
            std::reverse(result.points.begin(), result.points.end());
            return result;
        }

        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                nav::Vec3 next;
                if (!map.canStandAt(current.point.x + dx * spacing, current.point.y + dy * spacing, agent,
                                    &next, current.point.z)) {
                    continue;
                }
                const float cost = current.cost + hhv::movement::length(next - current.point);
                const auto key = keyOf(next);
                const auto previous = costs.find(key);
                if (previous != costs.end() && previous->second <= cost) {
                    continue;
                }
                if (map.blockedAlong(current.point, next, agent)) {
                    continue;
                }
                costs[key] = cost;
                nodes.push_back({next, cost, index});
                open.push({cost + hhv::movement::length(goal - next), static_cast<int>(nodes.size()) - 1});
            }
        }
    }
    return {};
}
} // namespace heaven
