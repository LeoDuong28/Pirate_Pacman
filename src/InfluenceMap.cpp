#include "InfluenceMap.h"
#include <algorithm>

void InfluenceMap::update(Vec2i playerTile) {
    // Decay all
    for (auto& v : m_map) v *= DECAY;
    // Add influence at player position
    if (inBounds(playerTile))
        m_map[idx(playerTile)] += ADD;
}

float InfluenceMap::get(Vec2i t) const {
    if (!inBounds(t)) return 0.f;
    return m_map[idx(t)];
}

Vec2i InfluenceMap::hotspot(Vec2i origin, int radius) const {
    Vec2i best = origin;
    float bestVal = -1.f;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            Vec2i t = {origin.x + dx, origin.y + dy};
            if (!inBounds(t)) continue;
            float v = m_map[idx(t)];
            if (v > bestVal) { bestVal = v; best = t; }
        }
    }
    return best;
}

Vec2i InfluenceMap::lowspot(Vec2i origin, int radius, const Map& map,
                             bool ghostMode) const {
    Vec2i best   = origin;
    float bestVal = 1e30f;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            Vec2i t = {origin.x + dx, origin.y + dy};
            if (!inBounds(t)) continue;
            // Only flee toward tiles the ghost can actually navigate to
            bool passable = ghostMode ? map.isWalkableForGhost(t)
                                      : map.isWalkable(t);
            if (!passable) continue;
            float v = m_map[idx(t)];
            if (v < bestVal) { bestVal = v; best = t; }
        }
    }
    return best;
}
