#include "Game.h"
#include "AudioGen.h"
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <string>

Game::Game() : m_ghosts({{
    Ghost(GhostType::CHASER),
    Ghost(GhostType::AMBUSHER),
    Ghost(GhostType::FLANKER),
    Ghost(GhostType::ERRATIC),
}}) {}

Game::~Game() {
    saveHighScore();
    for (auto& c : m_sfxEat)   if (c) Mix_FreeChunk(c);
    for (auto& c : m_sfxGhost) if (c) Mix_FreeChunk(c);
    if (m_sfxPower) Mix_FreeChunk(m_sfxPower);
    if (m_sfxDeath) Mix_FreeChunk(m_sfxDeath);
    if (m_sfxLevel) Mix_FreeChunk(m_sfxLevel);
    if (m_sfxFruit) Mix_FreeChunk(m_sfxFruit);
    for (auto& c : m_sfxSiren) if (c) Mix_FreeChunk(c);
    Mix_HaltChannel(-1);   // halt all channels before closing
    Mix_CloseAudio();
    if (m_fontLg) TTF_CloseFont(m_fontLg);
    if (m_fontSm) TTF_CloseFont(m_fontSm);
    TTF_Quit();
    // Must destroy cached textures BEFORE the renderer
    clearTextCache();
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window)   SDL_DestroyWindow(m_window);
    IMG_Quit();
    SDL_Quit();
}

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL_Init: %s\n", SDL_GetError()); return false;
    }
    // Seed RNG for particle system variety
    std::srand((unsigned int)SDL_GetPerformanceCounter());
    if (IMG_Init(IMG_INIT_PNG) == 0) {
        printf("IMG_Init: %s\n", IMG_GetError()); return false;
    }
    if (TTF_Init() < 0) {
        printf("TTF_Init: %s\n", TTF_GetError()); return false;
    }
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 512);
    Mix_ReserveChannels(1);   // channel 0 dedicated to background siren

    m_window = SDL_CreateWindow("Pirate Pacman",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        C::WIN_W, C::WIN_H, SDL_WINDOW_SHOWN);
    if (!m_window) { printf("Window: %s\n", SDL_GetError()); return false; }

    m_renderer = SDL_CreateRenderer(m_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer) { printf("Renderer: %s\n", SDL_GetError()); return false; }

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderSetLogicalSize(m_renderer, C::WIN_W, C::WIN_H);

    // Fonts (fallback if file missing)
    m_fontLg = TTF_OpenFont("assets/fonts/PressStart2P.ttf", 14);
    m_fontSm = TTF_OpenFont("assets/fonts/PressStart2P.ttf", 10);
    if (!m_fontLg || !m_fontSm) {
        TTF_CloseFont(m_fontLg); TTF_CloseFont(m_fontSm);
        m_fontLg = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 16);
        m_fontSm = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 12);
    }

    // Load sprites — fright sheet shared across all ghosts (one GPU texture)
    m_frightSheet.load(m_renderer, "assets/sprites/ghost_fright.png",
                       130, 130, {30, 30, 200, 255});
    m_player.loadSprites(m_renderer);
    for (auto& g : m_ghosts) g.loadSprites(m_renderer, &m_frightSheet);

    // Procedural sound generation — no WAV files required
    m_sfxEat[0] = AudioGen::dotEat();
    // second blip at slightly lower pitch for waka-waka alternation
    m_sfxEat[1] = AudioGen::makeSequence({{780, 25, 0.25f, true}, {0, 10, 0}});
    m_sfxPower  = AudioGen::powerEat();
    m_sfxDeath  = AudioGen::playerDeath();
    m_sfxLevel  = AudioGen::levelClear();
    m_sfxFruit  = AudioGen::fruitEat();
    for (int i = 0; i < 4; ++i) m_sfxGhost[i] = AudioGen::ghostEat(i);
    for (int i = 0; i < 5; ++i) m_sfxSiren[i] = AudioGen::siren(i);

    // Fruit sprite
    m_fruit.loadSprites(m_renderer);

    loadHighScore();
    m_lastTime = SDL_GetPerformanceCounter();
    startLevel();
    return true;
}

void Game::startLevel() {
    m_map.reset();
    m_influence.reset();
    Ghost::resetWave();
    m_phase          = GamePhase::COUNTDOWN;
    m_phaseTimer     = 2.5f;
    m_ghostKillCombo = 0;
    m_flashAlpha      = 0.f;
    m_flashTimer      = 0.f;
    m_flashCount      = 0;
    m_deathFlashAlpha = 0.f;
    m_extraLifeFlash  = 0.f;
    m_killFreezeTimer = 0.f;
    Mix_HaltChannel(SIREN_CHANNEL);
    m_sirenTier       = -1;
    m_fruit.reset();
    m_particles.clear();
    m_popups.clear();
    resetEntities();
}

void Game::resetEntities() {
    m_ghostKillCombo  = 0;
    m_killFreezeTimer = 0.f;
    m_player.revive();
    for (auto& g : m_ghosts) g.reset(m_level);
}

void Game::nextLevel() {
    ++m_level;
    startLevel();
}

float Game::playerSpeed() const {
    // 80% speed at level 1, +5% per level, capped at 100%
    float pct = std::min(0.80f + (m_level - 1) * 0.05f, 1.0f);
    return C::PLAYER_SPEED * pct;
}

float Game::frightDuration() const {
    float d = C::FRIGHT_BASE - (m_level - 1) * 1.5f;
    return std::max(d, 0.5f);
}

void Game::updateSiren() {
    // Silence during fright, any non-playing phase, or when paused
    bool anyFrightened = std::any_of(m_ghosts.begin(), m_ghosts.end(),
        [](const Ghost& g){ return g.state() == GhostState::FRIGHTENED; });

    if (anyFrightened || m_phase != GamePhase::PLAYING || m_paused) {
        Mix_HaltChannel(SIREN_CHANNEL);
        m_sirenTier = -1;
        return;
    }

    // Pitch tier rises as more dots are eaten (0 = calm, 4 = urgent)
    int total     = m_map.totalDots();
    int remaining = m_map.remainingDots();
    int tier      = 0;
    if (total > 0) {
        float eaten = 1.f - (float)remaining / (float)total;
        tier = std::clamp((int)(eaten * 5.f), 0, 4);
    }

    if (tier != m_sirenTier) {
        Mix_HaltChannel(SIREN_CHANNEL);
        m_sirenTier = tier;
        if (m_sfxSiren[tier])
            Mix_PlayChannel(SIREN_CHANNEL, m_sfxSiren[tier], -1);
    }
}

void Game::run() {
    while (!m_quit) {
        Uint64 now = SDL_GetPerformanceCounter();
        float  dt  = (float)(now - m_lastTime) / (float)SDL_GetPerformanceFrequency();
        m_lastTime = now;
        dt = std::min(dt, 0.05f); // clamp spike

        m_accumulator += dt;

        handleEvents();
        if (m_quit) break;

        while (m_accumulator >= C::FIXED_DT) {
            update(C::FIXED_DT);
            m_accumulator -= C::FIXED_DT;
        }

        render();

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (keys[SDL_SCANCODE_ESCAPE]) m_quit = true;
    }
}

void Game::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { m_quit = true; return; }
        // Accept movement input during COUNTDOWN so the first move is already
        // buffered when gameplay begins (matches classic Pac-Man behaviour).
        if (m_phase == GamePhase::PLAYING || m_phase == GamePhase::COUNTDOWN) {
            m_player.handleEvent(e);
        }
        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_RETURN &&
                (m_phase == GamePhase::MENU || m_phase == GamePhase::GAME_OVER)) {
                m_level = 1;
                m_player.reset();
                m_extraLifeThreshold = C::EXTRA_LIFE_SCORE;
                startLevel();
            }
            if (e.key.keysym.sym == SDLK_p) {
                if (m_paused) {
                    m_paused = false;
                } else if (m_phase == GamePhase::PLAYING) {
                    m_paused = true;
                }
            }
        }
    }
}

void Game::update(float dt) {
    if (m_paused) return;

    // Timers that run regardless of game phase
    if (m_deathFlashAlpha > 0.f)
        m_deathFlashAlpha = std::max(0.f, m_deathFlashAlpha - 400.f * dt);
    if (m_extraLifeFlash > 0.f)
        m_extraLifeFlash -= dt;

    m_pelletBlink += dt;
    if (m_pelletBlink > 0.4f) { m_pelletBlink = 0.f; m_pelletVisible = !m_pelletVisible; }

    switch (m_phase) {
        case GamePhase::MENU:
        case GamePhase::GAME_OVER:
            return;

        case GamePhase::COUNTDOWN:
            m_phaseTimer -= dt;
            if (m_phaseTimer <= 0.f) m_phase = GamePhase::PLAYING;
            return;

        case GamePhase::DEATH_PAUSE:
            m_phaseTimer -= dt;
            if (m_phaseTimer <= 0.f) {
                if (m_player.lives() <= 0) {
                    m_highScore = std::max(m_highScore, m_player.score());
                    saveHighScore();
                    m_phase = GamePhase::GAME_OVER;
                } else {
                    m_phase      = GamePhase::COUNTDOWN;
                    m_phaseTimer = 2.0f;
                    resetEntities();
                }
            }
            return;

        case GamePhase::LEVEL_CLEAR:
            tickFlash(dt);
            m_phaseTimer -= dt;
            if (m_phaseTimer <= 0.f) nextLevel();
            return;

        case GamePhase::PLAYING:
            break;
    }

    // Kill freeze: pause everything briefly after eating a ghost
    if (m_killFreezeTimer > 0.f) {
        m_killFreezeTimer -= dt;
        return;
    }

    // Advance global ghost wave — freeze during FRIGHTENED
    bool anyFrightened = std::any_of(m_ghosts.begin(), m_ghosts.end(),
        [](const Ghost& g){ return g.state() == GhostState::FRIGHTENED; });
    if (!anyFrightened)
        Ghost::advanceWave(dt, m_level);

    // Update influence map
    m_influence.update(m_player.tile());

    // Update player (speed scales with level)
    m_player.setSpeed(playerSpeed());
    m_player.update(dt, m_map);

    if (m_player.justPowered()) {
        float fd = frightDuration();
        for (auto& g : m_ghosts) g.frighten(fd);
        m_ghostKillCombo = 0;
        if (m_sfxPower) Mix_PlayChannel(-1, m_sfxPower, 0);
        // Particle burst at power pellet position
        float px = m_player.pixelPos().x + C::TILE * 0.5f;
        float py = m_player.pixelPos().y + C::TILE * 0.5f + C::HUD_H;
        m_particles.spawnPowerPickup(px, py);
    }
    if (m_player.justCollected()) {
        // Alternating waka-waka tones
        Mix_Chunk* chunk = m_sfxEat[m_dotEatPhase % 2];
        m_dotEatPhase++;
        if (chunk) Mix_PlayChannel(-1, chunk, 0);
        float px = m_player.pixelPos().x + C::TILE * 0.5f;
        float py = m_player.pixelPos().y + C::TILE * 0.5f + C::HUD_H;
        m_particles.spawnDotPickup(px, py);
    }

    // Fruit bonus
    int dotsEaten = m_map.totalDots() - m_map.remainingDots();
    m_fruit.checkSpawn(dotsEaten);
    m_fruit.update(dt);
    {
        int fruitPts = m_fruit.tryCollect(m_player.tile(), m_level);
        if (fruitPts > 0) {
            m_player.addScore(fruitPts);
            if (m_sfxFruit) Mix_PlayChannel(-1, m_sfxFruit, 0);
            float fx = FruitBonus::FRUIT_COL * C::TILE + C::TILE * 0.5f;
            float fy = FruitBonus::FRUIT_ROW * C::TILE + C::TILE * 0.5f + C::HUD_H;
            m_popups.spawn(fx, fy, fruitPts);
            m_particles.spawnPowerPickup(fx, fy);
        }
    }

    m_particles.update(dt);
    m_popups.update(dt);

    // Build ghost snapshots
    GhostSnapshot playerSnap { m_player.tile(), m_player.dir() };
    GhostSnapshot chaserSnap { m_ghosts[0].tile(), m_ghosts[0].dir() };

    // Update ghosts
    for (auto& g : m_ghosts) {
        g.update(dt, m_map, playerSnap, chaserSnap,
                 m_influence, m_map.remainingDots(), m_level);
    }

    checkCollisions();

    // Level complete — checked before extra-life so a simultaneous
    // last-dot-and-death doesn't advance the level (DEATH_PAUSE wins).
    if (m_map.remainingDots() == 0 && m_phase == GamePhase::PLAYING) {
        m_phase      = GamePhase::LEVEL_CLEAR;
        m_phaseTimer = 2.5f;
        m_flashCount = 0;
        m_flashAlpha = 0.f;
        if (m_sfxLevel) Mix_PlayChannel(-1, m_sfxLevel, 0);
    }

    // Extra life milestone — skip if already transitioning out of PLAYING
    // to avoid double-playing the level fanfare.
    if (m_phase == GamePhase::PLAYING &&
        m_player.score() >= m_extraLifeThreshold) {
        m_player.setLives(m_player.lives() + 1);
        m_extraLifeThreshold += C::EXTRA_LIFE_SCORE;
        m_extraLifeFlash = 2.5f;
        if (m_sfxLevel) Mix_PlayChannel(-1, m_sfxLevel, 0);
    }

    updateSiren();
}

void Game::checkCollisions() {
    if (!m_player.isAlive()) return;

    for (auto& g : m_ghosts) {
        if (g.tile() != m_player.tile()) continue;

        if (g.isEdible()) {
            int pts = C::PT_GHOST_BASE * (1 << m_ghostKillCombo);
            int combo = m_ghostKillCombo;
            ++m_ghostKillCombo;
            m_player.addScore(pts);
            // Pixel position for effects (ghost's render position)
            float gx = g.tile().x * C::TILE + C::TILE * 0.5f;
            float gy = g.tile().y * C::TILE + C::TILE * 0.5f + C::HUD_H;
            // Score popup
            m_popups.spawn(gx, gy, pts);
            // Particle burst in ghost's colour
            static const SDL_Color GHOST_COLS[4] = {
                {220,30,30,255},{255,180,200,255},{30,200,220,255},{230,150,30,255}};
            m_particles.spawnGhostKill(gx, gy, GHOST_COLS[(int)g.type()]);
            g.kill();
            Mix_Chunk* sfx = m_sfxGhost[std::min(combo, 3)];
            if (sfx) Mix_PlayChannel(-1, sfx, 0);
            m_killFreezeTimer = 0.4f;  // brief classic freeze
        } else if (!g.isDead()) {
            m_player.die();
            m_phase           = GamePhase::DEATH_PAUSE;
            m_phaseTimer      = 2.0f;
            m_deathFlashAlpha = 200.f;
            if (m_sfxDeath) Mix_PlayChannel(-1, m_sfxDeath, 0);
            float dx = m_player.pixelPos().x + C::TILE * 0.5f;
            float dy = m_player.pixelPos().y + C::TILE * 0.5f + C::HUD_H;
            m_particles.spawnGhostKill(dx, dy, {255, 200, 50, 255});
            return;
        }
    }
}

// ─── Flash ───────────────────────────────────────────────────────────────────

void Game::tickFlash(float dt) {
    if (m_flashCount >= 6) return;   // 3 full in/out cycles
    m_flashTimer += dt;
    // Each half-cycle lasts 0.2s: fade in 0→255 then out 255→0
    float halfPeriod = 0.2f;
    float t = m_flashTimer / halfPeriod;
    if (t >= 1.f) { m_flashTimer -= halfPeriod; ++m_flashCount; t = 0.f; }
    // Odd counts = fading out, even = fading in
    m_flashAlpha = (m_flashCount % 2 == 0) ? t * 200.f : (1.f - t) * 200.f;
}

// ─── Rendering ────────────────────────────────────────────────────────────────

void Game::drawWallTile(int col, int row) {
    // Connectivity check: treat out-of-bounds and ghost-door tiles as solid
    const auto& grid = m_map.grid();
    auto isSolid = [&](int c, int r) -> bool {
        if (c < 0 || c >= C::COLS || r < 0 || r >= C::ROWS) return true;
        Tile t = grid[r * C::COLS + c];
        return t == Tile::WALL || t == Tile::GHOST_DOOR;
    };

    const bool N = isSolid(col, row - 1);
    const bool S = isSolid(col, row + 1);
    const bool E = isSolid(col + 1, row);
    const bool W = isSolid(col - 1, row);

    const int x0 = col * C::TILE;
    const int y0 = row * C::TILE + C::HUD_H;
    const int T  = C::TILE;
    const int B  = 3;   // face-border thickness (px)

    // ── 1. Fill tile body with deep interior color ──────────────────────────
    const auto& wci = C::Color::WALL_INNER;
    SDL_SetRenderDrawColor(m_renderer, wci.r, wci.g, wci.b, wci.a);
    SDL_Rect body = {x0, y0, T, T};
    SDL_RenderFillRect(m_renderer, &body);

    // ── 2. Draw face border on each exposed side (facing a corridor) ─────────
    // Adjacent wall tiles share the interior — their face borders are suppressed,
    // so the wall mass reads as one solid connected shape.
    const auto& wc = C::Color::WALL;
    SDL_SetRenderDrawColor(m_renderer, wc.r, wc.g, wc.b, wc.a);
    if (!N) { SDL_Rect r = {x0,     y0,     T, B}; SDL_RenderFillRect(m_renderer, &r); }
    if (!S) { SDL_Rect r = {x0,     y0+T-B, T, B}; SDL_RenderFillRect(m_renderer, &r); }
    if (!W) { SDL_Rect r = {x0,     y0,     B, T}; SDL_RenderFillRect(m_renderer, &r); }
    if (!E) { SDL_Rect r = {x0+T-B, y0,     B, T}; SDL_RenderFillRect(m_renderer, &r); }
}

void Game::renderMap() {
    const auto& grid = m_map.grid();
    for (int r = 0; r < C::ROWS; ++r) {
        for (int c = 0; c < C::COLS; ++c) {
            Tile t = grid[r * C::COLS + c];
            if (t == Tile::WALL) {
                drawWallTile(c, r);
            } else if (t == Tile::GHOST_DOOR) {
                SDL_Rect dr = { c*C::TILE, r*C::TILE + C::HUD_H, C::TILE, 4 };
                const auto& dc = C::Color::DOOR;
                SDL_SetRenderDrawColor(m_renderer, dc.r, dc.g, dc.b, dc.a);
                SDL_RenderFillRect(m_renderer, &dr);
            }
        }
    }
}

void Game::renderPellets() {
    const auto& grid = m_map.grid();
    for (int r = 0; r < C::ROWS; ++r) {
        for (int c = 0; c < C::COLS; ++c) {
            Tile t = grid[r * C::COLS + c];
            int cx = c * C::TILE + C::TILE / 2;
            int cy = r * C::TILE + C::TILE / 2 + C::HUD_H;

            if (t == Tile::DOT) {
                const auto& dc = C::Color::DOT;
                SDL_SetRenderDrawColor(m_renderer, dc.r, dc.g, dc.b, dc.a);
                SDL_Rect dot = { cx - 2, cy - 2, 4, 4 };
                SDL_RenderFillRect(m_renderer, &dot);
            } else if (t == Tile::POWER && m_pelletVisible) {
                const auto& pc = C::Color::POWER_DOT;
                SDL_SetRenderDrawColor(m_renderer, pc.r, pc.g, pc.b, pc.a);
                SDL_Rect pw = { cx - 6, cy - 6, 12, 12 };
                SDL_RenderFillRect(m_renderer, &pw);
                // Glow ring
                SDL_SetRenderDrawColor(m_renderer, pc.r, pc.g, pc.b, 80);
                SDL_Rect glow = { cx - 9, cy - 9, 18, 18 };
                SDL_RenderDrawRect(m_renderer, &glow);
            }
        }
    }
}

void Game::renderHUD() {
    // Background bar
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_Rect bar = {0, 0, C::WIN_W, C::HUD_H};
    SDL_RenderFillRect(m_renderer, &bar);

    // Score
    if (m_fontSm) {
        renderText("SCORE:" + std::to_string(m_player.score()),
                   8, 8, C::Color::TEXT_GOLD, m_fontSm);
        renderText("HI:" + std::to_string(std::max(m_highScore, m_player.score())),
                   8, 28, C::Color::TEXT_WHITE, m_fontSm);
        renderText("LVL:" + std::to_string(m_level),
                   C::WIN_W - 110, 8, C::Color::TEXT_WHITE, m_fontSm);
    }

    // Lives: gold doubloon icons (up to 5 shown)
    for (int i = 0; i < std::min(m_player.lives(), 5); ++i) {
        int lx = C::WIN_W - 24 - i * 20;
        int ly = 30;
        // Outer coin
        SDL_SetRenderDrawColor(m_renderer, 180, 120, 0, 255);
        SDL_Rect outer = {lx, ly, 14, 14};
        SDL_RenderFillRect(m_renderer, &outer);
        // Inner sheen
        SDL_SetRenderDrawColor(m_renderer, 255, 200, 50, 255);
        SDL_Rect inner = {lx+2, ly+2, 10, 10};
        SDL_RenderFillRect(m_renderer, &inner);
        // Highlight dot
        SDL_SetRenderDrawColor(m_renderer, 255, 240, 140, 255);
        SDL_Rect shine = {lx+3, ly+3, 4, 4};
        SDL_RenderFillRect(m_renderer, &shine);
    }
}

std::string Game::textCacheKey(const std::string& text, SDL_Color c) const {
    // Key encodes RGB only — alpha is applied at render time via SDL_SetTextureAlphaMod,
    // so the same texture can be reused at different opacity levels without cache thrash.
    char buf[4] = {(char)c.r, (char)c.g, (char)c.b, '\0'};
    return text + '\x01' + std::string(buf, 3);
}

void Game::clearTextCache() {
    for (auto& [key, entry] : m_textCache) {
        if (entry.tex) SDL_DestroyTexture(entry.tex);
    }
    m_textCache.clear();
}

void Game::evictTextCache() {
    // Drop half the entries when the cache is full.  unordered_map iteration
    // order is arbitrary, giving roughly equal eviction probability to all entries.
    size_t toEvict = m_textCache.size() / 2;
    auto it = m_textCache.begin();
    while (it != m_textCache.end() && toEvict > 0) {
        if (it->second.tex) SDL_DestroyTexture(it->second.tex);
        it = m_textCache.erase(it);
        --toEvict;
    }
}

void Game::loadHighScore() {
    const char* home = std::getenv("HOME");
    if (!home) return;
    std::string path = std::string(home) + "/.pirate_pacman_hs";
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return;
    std::fscanf(f, "%d", &m_highScore);
    std::fclose(f);
}

void Game::saveHighScore() {
    int hs = std::max(m_highScore, m_player.score());
    if (hs <= 0) return;
    const char* home = std::getenv("HOME");
    if (!home) return;
    std::string path = std::string(home) + "/.pirate_pacman_hs";
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return;
    std::fprintf(f, "%d\n", hs);
    std::fclose(f);
}

void Game::renderText(const std::string& text, int x, int y,
                       SDL_Color color, TTF_Font* font) {
    if (!font) return;

    const std::string key = textCacheKey(text, color);
    auto it = m_textCache.find(key);

    if (it == m_textCache.end()) {
        if (m_textCache.size() >= TEXT_CACHE_MAX)
            evictTextCache();

        SDL_Surface* surf = TTF_RenderText_Solid(font, text.c_str(), color);
        if (!surf) return;
        TextEntry entry;
        entry.tex = SDL_CreateTextureFromSurface(m_renderer, surf);
        entry.w   = surf->w;
        entry.h   = surf->h;
        SDL_FreeSurface(surf);
        if (!entry.tex) return;
        // Ensure transparent background renders correctly on any backdrop
        SDL_SetTextureBlendMode(entry.tex, SDL_BLENDMODE_BLEND);
        it = m_textCache.emplace(key, entry).first;
    }

    const TextEntry& e = it->second;
    // Apply per-call alpha (cache key excludes alpha, so one texture serves all opacities)
    SDL_SetTextureAlphaMod(e.tex, color.a);
    SDL_Rect dst = {x, y, e.w, e.h};
    SDL_RenderCopy(m_renderer, e.tex, nullptr, &dst);
}

void Game::renderCenteredText(const std::string& text, int y,
                               SDL_Color color, TTF_Font* font) {
    if (!font) return;
    const std::string key = textCacheKey(text, color);
    auto it = m_textCache.find(key);
    int tw = 0;
    if (it != m_textCache.end()) {
        tw = it->second.w;
    } else {
        int th = 0;
        TTF_SizeText(font, text.c_str(), &tw, &th);
    }
    renderText(text, (C::WIN_W - tw) / 2, y, color, font);
}

void Game::render() {
    const auto& bg = C::Color::BG;
    SDL_SetRenderDrawColor(m_renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderClear(m_renderer);

    renderMap();
    renderPellets();

    // Entities (only during active gameplay phases)
    if (m_phase == GamePhase::PLAYING ||
        m_phase == GamePhase::DEATH_PAUSE ||
        m_phase == GamePhase::COUNTDOWN) {
        m_fruit.render(m_renderer);
        for (auto& g : m_ghosts) g.render(m_renderer);
        m_player.render(m_renderer);
        m_particles.render(m_renderer);
        m_popups.render(m_renderer, m_fontSm);
    }

    // Level-clear flash overlay
    if (m_phase == GamePhase::LEVEL_CLEAR && m_flashAlpha > 0.f) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, (Uint8)m_flashAlpha);
        SDL_Rect maze = {0, C::HUD_H, C::WIN_W, C::WIN_H - C::HUD_H};
        SDL_RenderFillRect(m_renderer, &maze);
    }

    renderHUD();

    // Overlay messages
    if (m_phase == GamePhase::COUNTDOWN) {
        if (m_fontLg)
            renderCenteredText("READY!", C::WIN_H/2 - 10,
                                {255,255,80,255}, m_fontLg);
    } else if (m_phase == GamePhase::LEVEL_CLEAR) {
        if (m_fontLg)
            renderCenteredText("LEVEL CLEAR!", C::WIN_H/2 - 10,
                                {80,255,80,255}, m_fontLg);
    } else if (m_phase == GamePhase::MENU) {
        if (m_fontLg) {
            renderCenteredText("PIRATE PACMAN", C::WIN_H/2 - 40,
                                C::Color::TEXT_GOLD, m_fontLg);
            renderCenteredText("PRESS ENTER", C::WIN_H/2 + 10,
                                C::Color::TEXT_WHITE, m_fontSm);
        }
    } else if (m_phase == GamePhase::GAME_OVER) {
        if (m_fontLg)
            renderCenteredText("GAME OVER", C::WIN_H/2 - 60,
                                {220, 30, 30, 255}, m_fontLg);
        if (m_fontSm) {
            renderCenteredText("SCORE  " + std::to_string(m_player.score()),
                                C::WIN_H/2 - 14, C::Color::TEXT_GOLD, m_fontSm);
            renderCenteredText("LEVEL  " + std::to_string(m_level),
                                C::WIN_H/2 + 6,  C::Color::TEXT_WHITE, m_fontSm);
            renderCenteredText("BEST   " + std::to_string(m_highScore),
                                C::WIN_H/2 + 26, {100, 255, 100, 255}, m_fontSm);
            renderCenteredText("PRESS ENTER TO PLAY AGAIN",
                                C::WIN_H/2 + 60, C::Color::TEXT_WHITE, m_fontSm);
        }
    }

    // ── Death flash (red overlay, fades quickly) ─────────────────────────────
    if (m_deathFlashAlpha > 0.f) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 220, 20, 20, (Uint8)m_deathFlashAlpha);
        SDL_Rect full = {0, 0, C::WIN_W, C::WIN_H};
        SDL_RenderFillRect(m_renderer, &full);
    }

    // ── Pause overlay ────────────────────────────────────────────────────────
    if (m_paused) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 160);
        SDL_Rect full = {0, 0, C::WIN_W, C::WIN_H};
        SDL_RenderFillRect(m_renderer, &full);
        if (m_fontLg) {
            renderCenteredText("PAUSED", C::WIN_H/2 - 20,
                                C::Color::TEXT_GOLD, m_fontLg);
            renderCenteredText("P TO RESUME", C::WIN_H/2 + 14,
                                C::Color::TEXT_WHITE, m_fontSm);
        }
    }

    // ── 1UP banner ───────────────────────────────────────────────────────────
    if (m_extraLifeFlash > 0.f && m_fontLg) {
        Uint8 alpha = (m_extraLifeFlash > 0.5f)
                      ? 255
                      : (Uint8)(m_extraLifeFlash / 0.5f * 255.f);
        renderCenteredText("1 UP!", C::WIN_H/2 - 60,
                            {100, 255, 100, alpha}, m_fontLg);
    }

    SDL_RenderPresent(m_renderer);
}
