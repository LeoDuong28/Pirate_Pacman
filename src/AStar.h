#pragma once
#include "Vec2.h"
#include "Map.h"
#include <vector>

class AStar {
public:
    // Returns the NEXT step from `from` toward `to`.
    // Returns `from` if already at target or no path found.
    static Vec2i nextStep(const Map& map, Vec2i from, Vec2i to,
                          bool ghostMode = false);

    // Full path (rarely needed — mostly for debug/visualization)
    static std::vector<Vec2i> findPath(const Map& map, Vec2i from, Vec2i to,
                                       bool ghostMode = false);

private:
    struct Node {
        Vec2i pos;
        int   g;       // cost from start
        int   f;       // g + h
        Vec2i parent;

        bool operator>(const Node& o) const {
            // Tie-break on h (= f - g) to prefer nodes closer to goal
            if (f != o.f) return f > o.f;
            return (f - g) > (o.f - o.g);
        }
    };

    static int heuristic(Vec2i a, Vec2i b);
};
