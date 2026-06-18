#include "ScorePopup.h"
#include <string>
#include <algorithm>

void ScorePopupSystem::spawn(float x, float y, int value) {
    m_popups.push_back({ x, y, -RISE_SPD, 1.f, 1.f / LIFETIME, value });
}

void ScorePopupSystem::update(float dt) {
    for (auto& p : m_popups) {
        p.y    += p.vy * dt;
        p.life -= p.decay * dt;
        if (p.life <= 0.f && p.tex) {
            SDL_DestroyTexture(p.tex);
            p.tex = nullptr;
        }
    }
    m_popups.erase(
        std::remove_if(m_popups.begin(), m_popups.end(),
                       [](const ScorePopup& p){ return p.life <= 0.f; }),
        m_popups.end());
}

void ScorePopupSystem::render(SDL_Renderer* ren, TTF_Font* font) {
    if (!font || m_popups.empty()) return;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    for (auto& p : m_popups) {
        // Lazy texture creation — once per popup, not once per frame
        if (!p.tex) {
            std::string txt = std::to_string(p.value);
            SDL_Color col = {255, 220, 30, 255};
            SDL_Surface* surf = TTF_RenderText_Solid(font, txt.c_str(), col);
            if (!surf) continue;
            p.tex = SDL_CreateTextureFromSurface(ren, surf);
            if (p.tex) SDL_SetTextureBlendMode(p.tex, SDL_BLENDMODE_BLEND);
            p.texW = surf->w;
            p.texH = surf->h;
            SDL_FreeSurface(surf);
        }
        if (!p.tex) continue;

        // Fade alpha based on remaining life
        SDL_SetTextureAlphaMod(p.tex, (Uint8)(p.life * 255.f));
        SDL_Rect dst = {(int)p.x - p.texW / 2, (int)p.y - p.texH / 2,
                        p.texW, p.texH};
        SDL_RenderCopy(ren, p.tex, nullptr, &dst);
    }
}

void ScorePopupSystem::clear() {
    for (auto& p : m_popups)
        if (p.tex) SDL_DestroyTexture(p.tex);
    m_popups.clear();
}
