#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <array>
#include <string>
#include <unordered_map>
#include "Map.h"
#include "Player.h"
#include "Ghost.h"
#include "InfluenceMap.h"
#include "Particle.h"
#include "ScorePopup.h"
#include "FruitBonus.h"
#include "Constants.h"

enum class GamePhase {
    MENU,
    COUNTDOWN,   // brief "ready!" pause
    PLAYING,
    DEATH_PAUSE, // freeze before respawn
    LEVEL_CLEAR,
    GAME_OVER,
};

class Game {
public:
    Game();
    ~Game();

    bool init();
    void run();

private:
    // SDL
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    TTF_Font*     m_fontLg   = nullptr;
    TTF_Font*     m_fontSm   = nullptr;

    // Game objects
    Map         m_map;
    Player      m_player;
    std::array<Ghost, 4> m_ghosts;   // initialised in Game.cpp ctor
    InfluenceMap     m_influence;
    ParticleSystem   m_particles;
    ScorePopupSystem m_popups;
    FruitBonus       m_fruit;

    // State
    GamePhase m_phase     = GamePhase::MENU;
    int       m_level     = 1;
    int       m_highScore = 0;
    float     m_phaseTimer = 0.f;
    int       m_ghostKillCombo = 0; // ghosts eaten in current power mode

    bool m_quit = false;

    // Fixed-timestep
    Uint64 m_lastTime  = 0;
    float  m_accumulator = 0.f;

    // Pellet blink
    float m_pelletBlink   = 0.f;
    bool  m_pelletVisible = true;

    // Level-clear maze flash (white overlay fading in/out 3 times)
    float m_flashAlpha = 0.f;
    float m_flashTimer = 0.f;
    int   m_flashCount = 0;
    void  tickFlash(float dt);

    // Pause
    bool m_paused = false;

    // Death flash (red overlay, fades to 0 over ~0.5s)
    float m_deathFlashAlpha = 0.f;

    // Extra life
    int   m_extraLifeThreshold = C::EXTRA_LIFE_SCORE;
    float m_extraLifeFlash     = 0.f;   // > 0 → show "1UP!" banner

    // Shared sprite sheet for all ghosts in frightened state (one GPU texture)
    SpriteSheet m_frightSheet;

    // Waka-waka alternation: two dot-eat tones
    int m_dotEatPhase = 0;

    // Kill freeze: brief full-game pause after eating a ghost
    float m_killFreezeTimer = 0.f;

    // Sound (generated procedurally at init)
    Mix_Chunk* m_sfxEat[2]  = {nullptr, nullptr};
    Mix_Chunk* m_sfxPower   = nullptr;
    Mix_Chunk* m_sfxDeath   = nullptr;
    Mix_Chunk* m_sfxGhost[4]= {nullptr,nullptr,nullptr,nullptr};
    Mix_Chunk* m_sfxLevel   = nullptr;
    Mix_Chunk* m_sfxFruit   = nullptr;

    // Background siren — dedicated reserved channel, pitch tier 0-4
    static constexpr int SIREN_CHANNEL = 0;
    Mix_Chunk* m_sfxSiren[5] = {nullptr,nullptr,nullptr,nullptr,nullptr};
    int        m_sirenTier   = -1;

    void handleEvents();
    void update(float dt);
    void render();
    void renderMap();
    void renderPellets();
    void renderHUD();
    void renderText(const std::string& text, int x, int y,
                    SDL_Color color, TTF_Font* font);
    void renderCenteredText(const std::string& text, int y,
                             SDL_Color color, TTF_Font* font);

    void checkCollisions();
    void startLevel();
    void nextLevel();
    void resetEntities();
    void drawWallTile(int col, int row);

    // Timing helpers
    float frightDuration() const;
    float playerSpeed()    const;
    void  updateSiren();

    // High score persistence
    void loadHighScore();
    void saveHighScore();

    // Text rendering cache: key = text+RGB, value = {texture, w, h}
    // Capped at TEXT_CACHE_MAX entries — evicts half when full (prevents
    // unbounded growth from changing score strings).
    static constexpr size_t TEXT_CACHE_MAX = 96;
    struct TextEntry { SDL_Texture* tex; int w, h; };
    std::unordered_map<std::string, TextEntry> m_textCache;
    void clearTextCache();
    void evictTextCache();   // drop half the cache when at capacity
    std::string textCacheKey(const std::string& text, SDL_Color c) const;
};
