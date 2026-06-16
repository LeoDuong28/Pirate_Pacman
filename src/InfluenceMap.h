#pragma once
#include "Vec2.h"
#include "Constants.h"
#include "Map.h"
#include <array>

// Decaying heat-map of player positions.
// Updated each game tick: player cell += 1.0, all cells *= DECAY.
// Ghosts can query the highest-influence tile within radius to predict
// where the player will be, enabling pre-emptive interception.
class InfluenceMap {
public:
    static constexpr float DECAY = 0.94f;   // per game tick (~60 Hz)
    static constexpr float ADD   = 1.0f;

    InfluenceMap() { m_map.fill(0.f); }

    void update(Vec2i playerTile);
    void reset() { m_map.fill(0.f); }

    float get(Vec2i t) const;

    // Returns the highest-influence tile within Chebyshev radius `r`
    Vec2i hotspot(Vec2i origin, int radius) const;

    // Returns the LOWEST-influence walkable tile within radius — used for ghost flee.
    // ghostMode=true allows door tiles (ghosts can exit through the house door).
    Vec2i lowspot(Vec2i origin, int radius, const Map& map,
                  bool ghostMode = true) const;

private:
    std::array<float, C::COLS * C::ROWS> m_map{};
    int idx(Vec2i t) const { return t.y * C::COLS + t.x; }
    bool inBounds(Vec2i t) const {
        return t.x >= 0 && t.x < C::COLS && t.y >= 0 && t.y < C::ROWS;
    }
};
