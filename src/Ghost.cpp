#include "Ghost.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

static constexpr float PI2 = 6.28318530718f;

// ── Static wave state ────────────────────────────────────────────────────────
GhostState Ghost::s_wave      = GhostState::SCATTER;
int        Ghost::s_waveIdx   = 0;
float      Ghost::s_waveTimer = 0.f;

static const float WAVE_TABLE[][2] = {
    { C::SCATTER_1, C::CHASE_1 },
    { C::SCATTER_2, C::CHASE_2 },
    { C::SCATTER_3, C::CHASE_3 },
    { C::SCATTER_4, 99999.f    },
};
static constexpr int WAVE_COUNT = 4;

void Ghost::advanceWave(float dt, int level) {
    float scatterScale = std::max(0.f, 1.f - (level - 1) * 0.15f);
    s_waveTimer += dt;

    int   phase   = s_waveIdx / 2;
    bool  scatter = (s_waveIdx % 2 == 0);
    float dur     = (phase < WAVE_COUNT)
                    ? WAVE_TABLE[phase][scatter ? 0 : 1] * (scatter ? scatterScale : 1.f)
                    : 99999.f;

    if (level >= 5 && scatter && phase < WAVE_COUNT) dur = 0.5f;

    if (s_waveTimer >= dur) {
        s_waveTimer = 0.f;
        ++s_waveIdx;
        s_wave = (s_waveIdx % 2 == 0) ? GhostState::SCATTER : GhostState::CHASE;
    }
}

// ── Start positions and initial facing directions ────────────────────────────
static const Vec2i GHOST_START[4] = {
    { C::HOUSE_CENTER_COL,     C::HOUSE_CENTER_ROW - 3 }, // chaser: above door
    { C::HOUSE_CENTER_COL,     C::HOUSE_CENTER_ROW     }, // ambusher: center
    { C::HOUSE_CENTER_COL - 2, C::HOUSE_CENTER_ROW     }, // flanker: left
    { C::HOUSE_CENTER_COL + 2, C::HOUSE_CENTER_ROW     }, // erratic: right
};
// Initial facing: chaser/ambusher face left, flanker faces left, erratic faces right
static const Vec2i GHOST_INIT_DIR[4] = {
    {-1, 0}, // chaser
    {-1, 0}, // ambusher
    {-1, 0}, // flanker (already at left side, faces left toward exit)
    { 1, 0}, // erratic (at right side, faces right toward exit)
};

static constexpr Vec2i DIRS4[4] = {{1,0},{-1,0},{0,1},{0,-1}};

// ── Constructor ──────────────────────────────────────────────────────────────
Ghost::Ghost(GhostType type)
    : m_type(type)
    , m_state(GhostState::HOUSE)
    , m_tile(GHOST_START[(int)type])
    , m_cachedNext(GHOST_START[(int)type])
    , m_cachedTarget({-1,-1})
    , m_pathDirty(true)
    , m_dir(GHOST_INIT_DIR[(int)type])
    , m_moveProgress(0.f)
    , m_frighTimer(0.f)
    , m_flashTimer(0.f)
    , m_flashOn(false)
    , m_houseTimer(C::GHOST_RELEASE[(int)type])
    , m_bobAccum(0.f)
{}

void Ghost::loadSprites(SDL_Renderer* ren, SpriteSheet* sharedFrightSheet) {
    static const char* paths[4] = {
        "assets/sprites/ghost_chaser.png",
        "assets/sprites/ghost_ambusher.png",
        "assets/sprites/ghost_flanker.png",
        "assets/sprites/ghost_erratic.png",
    };
    static const SDL_Color fallbacks[4] = {
        {220,  30,  30, 255},
        {255, 180, 200, 255},
        { 30, 200, 220, 255},
        {230, 150,  30, 255},
    };
    int t = (int)m_type;
    m_sheet.load(ren, paths[t], 130, 130, fallbacks[t]);

    if (sharedFrightSheet) {
        m_frighSheet  = sharedFrightSheet;
        m_ownsFright  = false;
    } else {
        m_frighSheetOwned.load(ren, "assets/sprites/ghost_fright.png",
                               130, 130, {30, 30, 200, 255});
        m_frighSheet = &m_frighSheetOwned;
        m_ownsFright = true;
    }

    std::vector<int> wf;
    for (int i = 0; i < m_sheet.frameCount(); ++i) wf.push_back(i);
    m_anim.addClip("walk",   { wf,  8.f, true });
    m_anim.addClip("fright", { {0}, 6.f, true });
    m_anim.play("walk");
}

void Ghost::reset(int /*level*/) {
    m_tile         = GHOST_START[(int)m_type];
    m_cachedNext   = m_tile;
    m_cachedTarget = {-1,-1};
    m_pathDirty    = true;
    m_dir          = GHOST_INIT_DIR[(int)m_type];
    m_moveProgress = 0.f;
    m_frighTimer   = 0.f;
    m_flashTimer   = 0.f;
    m_flashOn      = false;
    m_houseTimer   = C::GHOST_RELEASE[(int)m_type];
    m_bobAccum     = 0.f;
    m_state        = GhostState::HOUSE;
    m_anim.play("walk", true);
}

void Ghost::frighten(float duration) {
    if (m_state == GhostState::DEAD || m_state == GhostState::HOUSE) return;
    m_state      = GhostState::FRIGHTENED;
    m_frighTimer = duration;
    m_flashTimer = 0.f;
    m_flashOn    = false;

    // Commit the reversed direction as the cached next step so moveToward
    // honours the reversal for the current tile segment instead of
    // immediately overwriting it with a new A* result.
    reverseDir();
    Vec2i reversedNext = { m_tile.x + m_dir.x, m_tile.y + m_dir.y };
    // Clamp — wrap is handled by moveToward when committing the tile
    reversedNext.x = std::clamp(reversedNext.x, 0, C::COLS - 1);
    reversedNext.y = std::clamp(reversedNext.y, 0, C::ROWS - 1);
    m_cachedNext   = reversedNext;
    m_cachedTarget = {-1,-1};   // force re-evaluation of flee target next tile
    m_pathDirty    = false;     // don't re-run A* until we commit this step

    m_anim.play("fright", true);
}

void Ghost::kill() {
    m_state      = GhostState::DEAD;
    m_frighTimer = 0.f;
    m_pathDirty  = true;
    m_anim.play("walk", true);
}

// ── Target computation ───────────────────────────────────────────────────────
Vec2i Ghost::scatterCorner() const {
    int t = (int)m_type;
    return { C::SCATTER_COL[t], C::SCATTER_ROW[t] };
}

Vec2i Ghost::houseExit() const {
    return { C::HOUSE_DOOR_COL, C::HOUSE_DOOR_ROW - 1 };
}

Vec2i Ghost::computeTarget(const Map& map,
                            const GhostSnapshot& player,
                            const GhostSnapshot& chaser,
                            const InfluenceMap&  influence) const {
    (void)influence;  // reserved for future per-ghost prediction use
    switch (m_type) {
        case GhostType::CHASER:
            return player.tile;

        case GhostType::AMBUSHER: {
            Vec2i ahead = map.wrap({
                player.tile.x + player.dir.x * 4,
                player.tile.y + player.dir.y * 4
            });
            return ahead;
        }

        case GhostType::FLANKER: {
            Vec2i pivot = map.wrap({
                player.tile.x + player.dir.x * 2,
                player.tile.y + player.dir.y * 2
            });
            Vec2i target = {
                pivot.x + (pivot.x - chaser.tile.x),
                pivot.y + (pivot.y - chaser.tile.y)
            };
            target.x = std::clamp(target.x, 0, C::COLS - 1);
            target.y = std::clamp(target.y, 0, C::ROWS - 1);
            return target;
        }

        case GhostType::ERRATIC: {
            int dist = m_tile.manhattan(player.tile);
            return (dist > 8) ? player.tile : scatterCorner();
        }
    }
    return player.tile;
}

// ── Speed ────────────────────────────────────────────────────────────────────
float Ghost::speed(int level, int dotsRemaining) const {
    // Elroy / level scaling applied only in non-special states
    float base     = C::GHOST_SPEED_CHASE;
    float levelMul = std::min(1.f + (level - 1) * 0.05f, 1.2f);
    base *= levelMul;

    if (m_state == GhostState::FRIGHTENED) return C::GHOST_SPEED_FRIGHT;
    if (m_state == GhostState::DEAD)       return C::GHOST_SPEED_DEAD;

    // Tunnel slows ghosts (classic rule)
    if (inTunnel()) return C::GHOST_SPEED_TUNNEL;

    // Elroy mode: Chaser gets a speed boost when dots are low
    if (m_type == GhostType::CHASER) {
        if (dotsRemaining <= C::ELROY2_DOTS) return base * 1.10f;
        if (dotsRemaining <= C::ELROY1_DOTS) return base * 1.05f;
    }
    return base;
}

bool Ghost::inTunnel() const {
    // Row 14 is the tunnel row; the open corridors are cols 0-9 (left) and 18-27 (right).
    // The house walls at cols 10 and 17 form the boundary.
    if (m_tile.y != 14) return false;
    return (m_tile.x <= 9 || m_tile.x >= 18);
}

void Ghost::reverseDir() {
    m_dir = { -m_dir.x, -m_dir.y };
}

// ── Movement ─────────────────────────────────────────────────────────────────
void Ghost::moveToward(Vec2i target, const Map& map, float dt, float spd) {
    // Recompute A* only when: target changed, or tile was just committed
    if (m_pathDirty || m_cachedTarget != target) {
        Vec2i next = AStar::nextStep(map, m_tile, target, /*ghostMode=*/true);

        // If A* can't find a path, fall back to any non-reverse open direction
        if (next == m_tile) {
            Vec2i rev = { -m_dir.x, -m_dir.y };
            for (auto& d : DIRS4) {
                if (d == rev) continue;
                Vec2i nb = map.wrap({ m_tile.x + d.x, m_tile.y + d.y });
                if (map.isWalkableForGhost(nb)) { next = nb; break; }
            }
            // Deadend — allow reverse
            if (next == m_tile)
                next = map.wrap({ m_tile.x + rev.x, m_tile.y + rev.y });
        }

        m_cachedNext   = next;
        m_cachedTarget = target;
        m_pathDirty    = false;
    }

    // Update facing direction (handle tunnel wrap: diff can be ±27)
    Vec2i rawDir = { m_cachedNext.x - m_tile.x, m_cachedNext.y - m_tile.y };
    if (rawDir.x >  1) rawDir.x = -1;
    if (rawDir.x < -1) rawDir.x =  1;
    m_dir = rawDir;

    // Advance
    m_moveProgress += spd / C::TILE * dt;
    if (m_moveProgress >= 1.f) {
        m_moveProgress -= 1.f;
        m_tile      = map.wrap(m_cachedNext);
        m_pathDirty = true;   // recompute from new tile next call
    }
}

// ── Update ───────────────────────────────────────────────────────────────────
void Ghost::update(float dt, const Map& map,
                    const GhostSnapshot& player,
                    const GhostSnapshot& chaser,
                    const InfluenceMap&  influence,
                    int dotsRemaining, int level) {
    m_anim.update(dt);

    // FRIGHTENED
    if (m_state == GhostState::FRIGHTENED) {
        m_frighTimer -= dt;
        if (m_frighTimer <= 0.f) {
            m_state     = s_wave;
            m_pathDirty = true;
            m_anim.play("walk", true);
        } else {
            if (m_frighTimer <= C::FRIGHT_FLASH_AT) {
                m_flashTimer += dt;
                if (m_flashTimer > 0.2f) { m_flashTimer = 0.f; m_flashOn = !m_flashOn; }
            }
            // Flee: head toward the grid cell with the LOWEST player influence
            // (= farthest from where the player has been)
            Vec2i fleeTarget = influence.lowspot(m_tile, 8, map, /*ghostMode=*/true);
            moveToward(fleeTarget, map, dt, speed(level, dotsRemaining));
        }
        return;
    }

    // DEAD: return to house
    if (m_state == GhostState::DEAD) {
        Vec2i home = { C::HOUSE_CENTER_COL, C::HOUSE_CENTER_ROW };
        moveToward(home, map, dt, speed(level, dotsRemaining));
        if (m_tile == home) {
            m_state      = GhostState::HOUSE;
            m_houseTimer = 2.f;
            m_pathDirty  = true;
        }
        return;
    }

    // HOUSE: wait then exit
    if (m_state == GhostState::HOUSE) {
        m_bobAccum   += dt;                         // drives bob animation
        m_houseTimer -= dt;
        if (m_houseTimer <= 0.f) {
            m_state    = s_wave;
            m_bobAccum = 0.f;
            moveToward(houseExit(), map, dt, C::GHOST_SPEED_CHASE * 0.5f);
        }
        return;
    }

    // SCATTER / CHASE
    m_state = s_wave;
    Vec2i target = (m_state == GhostState::SCATTER)
                   ? scatterCorner()
                   : computeTarget(map, player, chaser, influence);

    moveToward(target, map, dt, speed(level, dotsRemaining));
}

// ── Render ───────────────────────────────────────────────────────────────────
void Ghost::render(SDL_Renderer* ren) const {
    // Smooth interpolation between m_tile and m_cachedNext
    float rx, ry;
    int   dx = m_cachedNext.x - m_tile.x;
    int   dy = m_cachedNext.y - m_tile.y;

    if (std::abs(dx) > 1 || std::abs(dy) > 1) {
        rx = (float)(m_tile.x * C::TILE);
        ry = (float)(m_tile.y * C::TILE);
    } else {
        rx = (m_tile.x + dx * m_moveProgress) * C::TILE;
        ry = (m_tile.y + dy * m_moveProgress) * C::TILE;
    }

    // House bob: ±4px sine oscillation at 1.5 Hz
    if (m_state == GhostState::HOUSE) {
        ry += std::sin(m_bobAccum * PI2 * 1.5f) * 4.f;
    }

    int px = (int)rx;
    int py = (int)ry + C::HUD_H;

    SDL_Color tint = {255, 255, 255, 255};
    const SpriteSheet* sheet = &m_sheet;

    if (m_state == GhostState::FRIGHTENED) {
        if (m_frighSheet) sheet = m_frighSheet;
        tint  = m_flashOn ? C::Color::FRIGHT_FL : C::Color::FRIGHT;
    } else if (m_state == GhostState::DEAD) {
        tint = {200, 200, 255, 160};
    }

    bool flipH = (m_dir.x > 0);
    sheet->draw(ren, m_anim.currentFrame(),
                px, py, C::TILE, C::TILE, flipH, tint);

    // Dead ghosts: draw floating eyes over the dimmed body so the player
    // can see which ghosts are still returning to the house.
    if (m_state == GhostState::DEAD) {
        const int EW = C::TILE / 5;          // eye white square size
        const int PW = EW / 2 + 1;           // pupil size
        const int eyeY = py + C::TILE / 4;   // vertical position of eye whites

        // Left and right eye-white positions (centred at 1/3 and 2/3 of tile)
        const int lx = px + C::TILE / 3 - EW / 2;
        const int rx = px + 2 * C::TILE / 3 - EW / 2;

        // Eye whites
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 220);
        SDL_Rect lEye = {lx, eyeY, EW, EW};
        SDL_Rect rEye = {rx, eyeY, EW, EW};
        SDL_RenderFillRect(ren, &lEye);
        SDL_RenderFillRect(ren, &rEye);

        // Pupils shift in the direction of travel
        const int pdx = (m_dir.x > 0 ? 1 : m_dir.x < 0 ? -1 : 0);
        const int pdy = (m_dir.y > 0 ? 1 : m_dir.y < 0 ? -1 : 0);
        SDL_SetRenderDrawColor(ren, 30, 80, 220, 230);
        SDL_Rect lPupil = {lx + (EW - PW) / 2 + pdx, eyeY + (EW - PW) / 2 + pdy, PW, PW};
        SDL_Rect rPupil = {rx + (EW - PW) / 2 + pdx, eyeY + (EW - PW) / 2 + pdy, PW, PW};
        SDL_RenderFillRect(ren, &lPupil);
        SDL_RenderFillRect(ren, &rPupil);
    }
}
