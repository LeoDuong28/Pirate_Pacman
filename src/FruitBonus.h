#pragma once
#include <SDL2/SDL.h>
#include "Vec2.h"
#include "SpriteSheet.h"
#include "Constants.h"

class FruitBonus {
public:
    // Spawn position: below the ghost house, accessible from both sides
    static constexpr int   FRUIT_COL    = 13;
    static constexpr int   FRUIT_ROW    = 17;
    // Dots eaten thresholds that trigger each appearance
    static constexpr int   APPEAR1_DOTS = 70;
    static constexpr int   APPEAR2_DOTS = 170;
    static constexpr float LIFETIME     = 9.f;
    static constexpr float BLINK_AT     = 3.f;   // start blinking when this many seconds remain

    FruitBonus();
    void loadSprites(SDL_Renderer* ren);

    // Call each tick during PLAYING; dotsEaten = totalDots - remainingDots
    void checkSpawn(int dotsEaten);

    // Returns point value if player collected it, 0 otherwise
    int  tryCollect(Vec2i playerTile, int level);

    void update(float dt);
    void render(SDL_Renderer* ren) const;
    void reset();   // call on new game / new level

    bool  isActive() const { return m_active; }
    Vec2i tile()     const { return {FRUIT_COL, FRUIT_ROW}; }

private:
    bool        m_active    = false;
    float       m_timer     = 0.f;      // time remaining
    bool        m_appeared1 = false;
    bool        m_appeared2 = false;
    float       m_blinkAccum= 0.f;
    bool        m_blinkOn   = true;
    float       m_animAccum = 0.f;
    int         m_animFrame = 0;
    SpriteSheet m_sheet;

    void  spawn();
    static int pointsForLevel(int level);
};
