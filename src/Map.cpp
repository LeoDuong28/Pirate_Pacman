#include "Map.h"
#include <cassert>
#include <cstring>

// Classic 28×31 Pac-Man maze.
// '#' wall  '.' dot  'o' power  ' ' empty  '-' ghost door  'T' tunnel
static const char* MAP_DATA[C::ROWS] = {
    "############################",  //  0
    "#............##............#",  //  1
    "#.####.#####.##.#####.####.#",  //  2
    "#o####.#####.##.#####.####o#",  //  3
    "#.####.#####.##.#####.####.#",  //  4
    "#..........................#",  //  5
    "#.####.##.########.##.####.#",  //  6
    "#.####.##.########.##.####.#",  //  7
    "#......##....##....##......#",  //  8
    "######.#####.##.#####.######",  //  9
    "######.#####.##.#####.######",  // 10
    "######.##          ##.######",  // 11
    "######.## ###--### ##.######",  // 12
    "######.## #      # ##.######",  // 13
    "T      .  #      #  .      T",  // 14  ← tunnel row
    "######.## #      # ##.######",  // 15
    "######.## ######## ##.######",  // 16
    "######.##          ##.######",  // 17
    "######.## ######## ##.######",  // 18
    "######.## ######## ##.######",  // 19
    "#............##............#",  // 20
    "#.####.#####.##.#####.####.#",  // 21
    "#.####.#####.##.#####.####.#",  // 22
    "#o..##................##..o#",  // 23
    "###.##.##.########.##.##.###",  // 24
    "###.##.##.########.##.##.###",  // 25
    "#......##....##....##......#",  // 26
    "#.##########.##.##########.#",  // 27
    "#.##########.##.##########.#",  // 28
    "#..........................#",  // 29
    "############################",  // 30
};

Map::Map() { load(); }

void Map::load() {
    m_totalDots = 0;
    m_remaining = 0;

    for (int r = 0; r < C::ROWS; ++r) {
        const char* row    = MAP_DATA[r];
        int         rowLen = (int)strlen(row);   // compute once, not per column
        for (int c = 0; c < C::COLS; ++c) {
            char ch = (c < rowLen) ? row[c] : ' ';
            Tile t = Tile::EMPTY;
            switch (ch) {
                case '#': t = Tile::WALL;       break;
                case '.': t = Tile::DOT;        ++m_totalDots; break;
                case 'o': t = Tile::POWER;      ++m_totalDots; break;
                case '-': t = Tile::GHOST_DOOR; break;
                case 'T': t = Tile::TUNNEL;     break;
                default:  t = Tile::EMPTY;      break;
            }
            m_grid[idx(c, r)]    = t;
            m_initial[idx(c, r)] = t;
        }
    }
    m_remaining = m_totalDots;

    // Clear the player's starting tile — dot there can never be entered
    // from a new-tile transition, so it would be immortal otherwise.
    int startIdx = idx(C::PLAYER_START_COL, C::PLAYER_START_ROW);
    if (m_grid[startIdx] == Tile::DOT || m_grid[startIdx] == Tile::POWER) {
        m_grid[startIdx]   = Tile::EMPTY;
        m_initial[startIdx]= Tile::EMPTY;
        --m_totalDots;
        --m_remaining;
    }

    buildJunctions();
}

void Map::reset() {
    m_grid      = m_initial;
    m_remaining = m_totalDots;
}

void Map::buildJunctions() {
    const Vec2i dirs[4] = {{1,0},{-1,0},{0,1},{0,-1}};
    for (int r = 0; r < C::ROWS; ++r) {
        for (int c = 0; c < C::COLS; ++c) {
            if (isWall(c, r)) { m_junction[idx(c,r)] = false; continue; }
            int open = 0;
            for (auto& d : dirs) {
                Vec2i nb = wrap({c + d.x, r + d.y});
                if (!isWall(nb)) ++open;
            }
            m_junction[idx(c,r)] = (open >= 3);
        }
    }
}

Tile Map::get(int col, int row) const {
    if (col < 0 || col >= C::COLS || row < 0 || row >= C::ROWS)
        return Tile::WALL;
    return m_grid[idx(col, row)];
}

void Map::set(int col, int row, Tile tile) {
    if (col < 0 || col >= C::COLS || row < 0 || row >= C::ROWS) return;
    m_grid[idx(col, row)] = tile;
}

bool Map::isWall(int col, int row) const {
    Tile t = get(col, row);
    return t == Tile::WALL;
}

bool Map::isWalkable(Vec2i t) const {
    if (t.x < 0 || t.x >= C::COLS || t.y < 0 || t.y >= C::ROWS) return false;
    Tile tile = get(t);
    return tile != Tile::WALL && tile != Tile::GHOST_DOOR;
}

bool Map::isWalkableForGhost(Vec2i t) const {
    if (t.x < 0 || t.x >= C::COLS || t.y < 0 || t.y >= C::ROWS) return false;
    return get(t) != Tile::WALL;
}

bool Map::collectDot(Vec2i t) {
    if (get(t) == Tile::DOT) {
        set(t, Tile::EMPTY);
        --m_remaining;
        return true;
    }
    return false;
}

bool Map::collectPower(Vec2i t) {
    if (get(t) == Tile::POWER) {
        set(t, Tile::EMPTY);
        --m_remaining;
        return true;
    }
    return false;
}

Vec2i Map::wrap(Vec2i t) const {
    if (t.x < 0)       t.x = C::COLS - 1;
    if (t.x >= C::COLS) t.x = 0;
    return t;
}

bool Map::isJunction(Vec2i t) const {
    if (t.x < 0 || t.x >= C::COLS || t.y < 0 || t.y >= C::ROWS) return false;
    return m_junction[idx(t.x, t.y)];
}
