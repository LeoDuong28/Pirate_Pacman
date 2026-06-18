#pragma once
#include <SDL2/SDL.h>
#include "Vec2.h"
#include "Map.h"
#include "SpriteSheet.h"
#include "Animation.h"
#include "Constants.h"

class Player {
public:
    Player();

    void loadSprites(SDL_Renderer* ren);
    void reset();          // full reset (new game)
    void revive();         // respawn after death

    void handleEvent(const SDL_Event& e);
    void update(float dt, Map& map);
    void render(SDL_Renderer* ren) const;

    // Accessors
    Vec2i tile()       const { return m_tile; }
    Vec2f pixelPos()   const { return m_pos; }
    Vec2i dir()        const { return m_dir; }
    bool  isPowered()  const { return m_powerTimer > 0.f; }
    float powerTimer() const { return m_powerTimer; }
    bool  isAlive()    const { return m_alive; }
    int   lives()      const { return m_lives; }
    int   score()      const { return m_score; }

    void  addScore(int pts)  { m_score += pts; }
    void  die();
    void  setLives(int l)    { m_lives = l; }
    void  setSpeed(float s)  { m_speed = s; }
    bool  justCollected()    const { return m_justCollected; }
    bool  justPowered()      const { return m_justPowered; }

private:
    Vec2f  m_pos;           // smooth pixel position for rendering
    Vec2i  m_tile;          // committed grid tile (origin of current move)
    Vec2i  m_targetTile;    // tile we are moving toward (== m_tile when idle)
    Vec2i  m_dir;           // current movement direction
    Vec2i  m_nextDir;       // buffered input
    float  m_moveProgress;  // 0..1 from m_tile toward m_targetTile

    float  m_powerTimer;
    float  m_speed = C::PLAYER_SPEED;
    bool   m_alive;
    int    m_lives;
    int    m_score;
    bool   m_justCollected;
    bool   m_justPowered;

    SpriteSheet m_walkSheet;
    SpriteSheet m_idleSheet;
    Animation   m_anim;

    void tryStartMove(Map& map);
    void collectAt(Vec2i t, Map& map);
    void setupAnimations();
    void placeAt(Vec2i tile);
};
