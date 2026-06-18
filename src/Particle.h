#pragma once
#include <SDL2/SDL.h>
#include <array>
#include "Vec2.h"

struct Particle {
    Vec2f    pos;
    Vec2f    vel;
    float    life;       // seconds remaining
    float    maxLife;
    float    size;       // pixel radius
    float    gravity;    // px/s² downward  (0 = none)
    SDL_Color color;
    bool     active = false;
};

class ParticleSystem {
public:
    void spawnDotPickup  (float cx, float cy);
    void spawnPowerPickup(float cx, float cy);
    void spawnGhostKill  (float cx, float cy, SDL_Color ghostColor);

    void update(float dt);
    void render(SDL_Renderer* ren) const;
    void clear();

private:
    static constexpr int MAX = 256;
    std::array<Particle, MAX> m_pool{};
    int m_next = 0;   // ring-buffer cursor

    void emit(float x, float y, float vx, float vy,
              float life, float size, SDL_Color color, float gravity = 0.f);
};
