#include "AStar.h"
#include <queue>
#include <array>
#include <algorithm>

static constexpr Vec2i DIRS[4] = {{1,0},{-1,0},{0,1},{0,-1}};

int AStar::heuristic(Vec2i a, Vec2i b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

std::vector<Vec2i> AStar::findPath(const Map& map, Vec2i from, Vec2i to,
                                    bool ghostMode) {
    if (from == to) return {};

    // Open set: min-heap on f
    using PQ = std::priority_queue<Node, std::vector<Node>, std::greater<Node>>;
    PQ open;

    // Flat arrays for visited grid (28×31 = 868 cells — stack-friendly)
    std::array<int,   C::COLS * C::ROWS> gCost;
    std::array<Vec2i, C::COLS * C::ROWS> parent;
    std::array<bool,  C::COLS * C::ROWS> visited;
    gCost.fill(999999);
    parent.fill({-1,-1});
    visited.fill(false);

    auto idx = [](Vec2i v){ return v.y * C::COLS + v.x; };

    gCost[idx(from)] = 0;
    open.push({ from, 0, heuristic(from, to), {-1,-1} });

    while (!open.empty()) {
        Node cur = open.top(); open.pop();

        if (visited[idx(cur.pos)]) continue;
        visited[idx(cur.pos)] = true;
        parent[idx(cur.pos)] = cur.parent;

        if (cur.pos == to) {
            // Reconstruct path
            std::vector<Vec2i> path;
            Vec2i p = to;
            while (p != from && p != Vec2i{-1,-1}) {
                path.push_back(p);
                p = parent[idx(p)];
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (auto& d : DIRS) {
            Vec2i nb = map.wrap({cur.pos.x + d.x, cur.pos.y + d.y});

            bool walkable = ghostMode ? map.isWalkableForGhost(nb)
                                      : map.isWalkable(nb);
            if (!walkable) continue;
            if (visited[idx(nb)]) continue;

            int ng = cur.g + 1;
            if (ng < gCost[idx(nb)]) {
                gCost[idx(nb)] = ng;
                open.push({ nb, ng, ng + heuristic(nb, to), cur.pos });
            }
        }
    }
    return {}; // no path
}

Vec2i AStar::nextStep(const Map& map, Vec2i from, Vec2i to, bool ghostMode) {
    if (from == to) return from;

    using PQ = std::priority_queue<Node, std::vector<Node>, std::greater<Node>>;
    PQ open;

    std::array<int,   C::COLS * C::ROWS> gCost;
    std::array<Vec2i, C::COLS * C::ROWS> parent;
    std::array<bool,  C::COLS * C::ROWS> visited;
    gCost.fill(999999);
    parent.fill({-1,-1});
    visited.fill(false);

    auto idx = [](Vec2i v){ return v.y * C::COLS + v.x; };

    gCost[idx(from)] = 0;
    open.push({ from, 0, heuristic(from, to), {-1,-1} });

    while (!open.empty()) {
        Node cur = open.top(); open.pop();

        if (visited[idx(cur.pos)]) continue;
        visited[idx(cur.pos)] = true;
        parent[idx(cur.pos)] = cur.parent;

        if (cur.pos == to) {
            // Trace parent chain backward from `to` to find the step after `from`.
            // No path vector needed — just walk until parent == from.
            Vec2i p = to;
            while (true) {
                Vec2i prev = parent[idx(p)];
                if (prev == from || prev == Vec2i{-1,-1}) return p;
                p = prev;
            }
        }

        for (auto& d : DIRS) {
            Vec2i nb = map.wrap({cur.pos.x + d.x, cur.pos.y + d.y});
            bool walkable = ghostMode ? map.isWalkableForGhost(nb) : map.isWalkable(nb);
            if (!walkable || visited[idx(nb)]) continue;
            int ng = cur.g + 1;
            if (ng < gCost[idx(nb)]) {
                gCost[idx(nb)] = ng;
                open.push({ nb, ng, ng + heuristic(nb, to), cur.pos });
            }
        }
    }
    return from; // no path
}
