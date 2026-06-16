#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include "Vec2.h"
#include "Constants.h"

enum class Tile : uint8_t {
    EMPTY      = 0,
    WALL       = 1,
    DOT        = 2,
    POWER      = 3,
    GHOST_DOOR = 4,
    TUNNEL     = 5,   // horizontal wrap
};

class Map {
public:
    Map();

    Tile get(int col, int row)             const;
    Tile get(Vec2i t)                      const { return get(t.x, t.y); }
    void set(int col, int row, Tile tile);
    void set(Vec2i t, Tile tile)                  { set(t.x, t.y, tile); }

    bool isWall(int col, int row)          const;
    bool isWall(Vec2i t)                   const { return isWall(t.x, t.y); }
    bool isWalkable(Vec2i t)               const;   // non-wall, in-bounds
    bool isWalkableForGhost(Vec2i t)       const;   // ghost can also pass door

    bool collectDot(Vec2i t);   // returns true if dot was there
    bool collectPower(Vec2i t); // returns true if power pellet was there

    int  totalDots()     const { return m_totalDots; }
    int  remainingDots() const { return m_remaining; }

    // Wrap tunnel columns
    Vec2i wrap(Vec2i t) const;

    // Precomputed junction set (3+ open neighbors) — used by A*
    bool isJunction(Vec2i t) const;

    // Raw grid access for rendering
    const std::array<Tile, C::COLS * C::ROWS>& grid() const { return m_grid; }

    void reset(); // reload initial state

private:
    std::array<Tile, C::COLS * C::ROWS> m_grid{};
    std::array<Tile, C::COLS * C::ROWS> m_initial{};
    std::array<bool, C::COLS * C::ROWS> m_junction{};

    int m_totalDots   = 0;
    int m_remaining   = 0;

    void load();
    void buildJunctions();
    int  idx(int col, int row) const { return row * C::COLS + col; }
};
