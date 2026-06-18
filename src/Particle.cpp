#include "Particle.h"
#include <cmath>
#include <cstdlib>

static float frand() { return (float)std::rand() / RAND_MAX; }
static float frandRange(float lo, float hi) { return lo + frand() * (hi - lo); }

void ParticleSystem::emit(float x, float y, float vx, float vy,
                           float life, float size, SDL_Color color, float gravity) {
    // Ring-buffer: overwrite oldest when full
    Particle& p  = m_pool[m_next];
    m_next       = (m_next + 1) % MAX;
    p.pos        = {x, y};
    p.vel        = {vx, vy};
    p.life       = life;
    p.maxLife    = life;
    p.size       = size;
    p.gravity    = gravity;
    p.color      = color;
    p.active     = true;
}

void ParticleSystem::spawnDotPickup(float cx, float cy) {
    // 5 small gold sparks
    for (int i = 0; i < 5; ++i) {
        float angle = frand() * 6.28318f;
        float speed = frandRange(20.f, 60.f);
        emit(cx, cy,
             std::cos(angle) * speed, std::sin(angle) * speed,
             frandRange(0.25f, 0.45f),
             frandRange(2.f, 4.f),
             {255, 200, 40, 255});
    }
}

void ParticleSystem::spawnPowerPickup(float cx, float cy) {
    // 14 bright yellow bursts, slight gravity
    for (int i = 0; i < 14; ++i) {
        float angle = (float)i / 14.f * 6.28318f + frand() * 0.5f;
        float speed = frandRange(50.f, 110.f);
        emit(cx, cy,
             std::cos(angle) * speed, std::sin(angle) * speed,
             frandRange(0.5f, 0.9f),
             frandRange(3.f, 6.f),
             {255, 230, 30, 255},
             80.f);
    }
}

void ParticleSystem::spawnGhostKill(float cx, float cy, SDL_Color ghostColor) {
    // 10 particles in the ghost's colour + white accent
    for (int i = 0; i < 10; ++i) {
        float angle = frand() * 6.28318f;
        float speed = frandRange(40.f, 120.f);
        SDL_Color col = (i < 7) ? ghostColor : SDL_Color{255, 255, 255, 255};
        emit(cx, cy,
             std::cos(angle) * speed, std::sin(angle) * speed,
             frandRange(0.4f, 0.8f),
             frandRange(3.f, 7.f),
             col, 60.f);
    }
}

void ParticleSystem::update(float dt) {
    for (auto& p : m_pool) {
        if (!p.active) continue;
        p.life -= dt;
        if (p.life <= 0.f) { p.active = false; continue; }
        p.vel.y += p.gravity * dt;
        p.pos   += p.vel * dt;
    }
}

void ParticleSystem::render(SDL_Renderer* ren) const {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    for (const auto& p : m_pool) {
        if (!p.active) continue;
        float t   = p.life / p.maxLife;           // 1→0 as particle dies
        Uint8 a   = (Uint8)(t * p.color.a);
        SDL_SetRenderDrawColor(ren, p.color.r, p.color.g, p.color.b, a);
        int   s   = std::max(1, (int)(p.size * t));
        SDL_Rect r = { (int)(p.pos.x - s/2), (int)(p.pos.y - s/2), s, s };
        SDL_RenderFillRect(ren, &r);
    }
}

void ParticleSystem::clear() {
    for (auto& p : m_pool) p.active = false;
}
