#include "FruitBonus.h"
#include <algorithm>

// Points per level: 200, 400, 800, 1600, 2000, 3000, 5000, 5000, ...
static const int FRUIT_PTS[] = {200, 400, 800, 1600, 2000, 3000, 5000};
static const int FRUIT_PTS_N = (int)(sizeof(FRUIT_PTS)/sizeof(FRUIT_PTS[0]));

int FruitBonus::pointsForLevel(int level) {
    int idx = std::clamp(level - 1, 0, FRUIT_PTS_N - 1);
    return FRUIT_PTS[idx];
}

FruitBonus::FruitBonus() {}

void FruitBonus::loadSprites(SDL_Renderer* ren) {
    // Uses the treasure chest sprite; falls back to a gold square
    m_sheet.load(ren, "assets/sprites/power_pellet.png",
                 130, 130, {220, 160, 20, 255});
}

void FruitBonus::reset() {
    m_active     = false;
    m_timer      = 0.f;
    m_appeared1  = false;
    m_appeared2  = false;
    m_blinkOn    = true;
    m_blinkAccum = 0.f;
    m_animAccum  = 0.f;
    m_animFrame  = 0;
}

void FruitBonus::spawn() {
    m_active     = true;
    m_timer      = LIFETIME;
    m_blinkOn    = true;
    m_blinkAccum = 0.f;
}

void FruitBonus::checkSpawn(int dotsEaten) {
    if (!m_appeared1 && dotsEaten >= APPEAR1_DOTS) {
        m_appeared1 = true;
        spawn();
        return;
    }
    if (!m_appeared2 && !m_active && dotsEaten >= APPEAR2_DOTS) {
        m_appeared2 = true;
        spawn();
    }
}

int FruitBonus::tryCollect(Vec2i playerTile, int level) {
    if (!m_active) return 0;
    if (playerTile.x != FRUIT_COL || playerTile.y != FRUIT_ROW) return 0;
    m_active = false;
    return pointsForLevel(level);
}

void FruitBonus::update(float dt) {
    if (!m_active) return;

    m_timer -= dt;
    if (m_timer <= 0.f) { m_active = false; return; }

    // Blink during final BLINK_AT seconds
    if (m_timer <= BLINK_AT) {
        m_blinkAccum += dt;
        if (m_blinkAccum >= 0.15f) { m_blinkAccum = 0.f; m_blinkOn = !m_blinkOn; }
    } else {
        m_blinkOn = true;
    }

    // Sprite animation
    m_animAccum += dt;
    if (m_animAccum >= 0.12f) {
        m_animAccum = 0.f;
        int frames = std::max(1, m_sheet.frameCount());
        m_animFrame = (m_animFrame + 1) % frames;
    }
}

void FruitBonus::render(SDL_Renderer* ren) const {
    if (!m_active || !m_blinkOn) return;
    int px = FRUIT_COL * C::TILE;
    int py = FRUIT_ROW * C::TILE + C::HUD_H;
    m_sheet.draw(ren, m_animFrame, px, py, C::TILE, C::TILE,
                 false, {255, 255, 255, 255});
}
