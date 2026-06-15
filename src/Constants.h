#pragma once
#include <SDL2/SDL.h>

namespace C {

// Grid
constexpr int COLS       = 28;
constexpr int ROWS       = 31;
constexpr int TILE       = 28;   // pixels per tile

// Window
constexpr int HUD_H      = 56;
constexpr int WIN_W      = COLS * TILE;                 // 784
constexpr int WIN_H      = ROWS * TILE + HUD_H;        // 924

// Frame rate
constexpr float FIXED_DT = 1.f / 60.f;
constexpr int   TARGET_FPS = 60;

// Speeds (pixels/second)
constexpr float PLAYER_SPEED       = 4.0f * TILE;   // 4 tiles/sec = 112 px/s
constexpr float GHOST_SPEED_CHASE  = 3.6f * TILE;
constexpr float GHOST_SPEED_FRIGHT = 2.0f * TILE;
constexpr float GHOST_SPEED_DEAD   = 7.0f * TILE;
constexpr float GHOST_SPEED_TUNNEL = 2.5f * TILE;

// Ghost state durations (seconds)
constexpr float SCATTER_1 = 7.f;
constexpr float CHASE_1   = 20.f;
constexpr float SCATTER_2 = 7.f;
constexpr float CHASE_2   = 20.f;
constexpr float SCATTER_3 = 5.f;
constexpr float CHASE_3   = 20.f;
constexpr float SCATTER_4 = 5.f;

constexpr float FRIGHT_BASE      = 8.f;   // reduced each level
constexpr float FRIGHT_FLASH_AT  = 2.f;   // flash warning

// Elroy thresholds (dots remaining)
constexpr int ELROY1_DOTS = 20;
constexpr int ELROY2_DOTS = 10;

// Points
constexpr int PT_DOT        = 10;
constexpr int PT_POWER      = 50;
constexpr int PT_GHOST_BASE = 200;   // doubles per ghost in one fright
constexpr int PT_FRUIT      = 200;
constexpr int EXTRA_LIFE_SCORE = 10000;  // award an extra life every N points

// Ghost house tile
constexpr int HOUSE_DOOR_COL = 13;
constexpr int HOUSE_DOOR_ROW = 12;
constexpr int HOUSE_CENTER_COL = 14;
constexpr int HOUSE_CENTER_ROW = 14;

// Scatter corners (col, row) for each ghost
constexpr int SCATTER_COL[4] = { 25, 2,  27,  0 };
constexpr int SCATTER_ROW[4] = {  0, 0,  30, 30 };

// Player start
constexpr int PLAYER_START_COL = 14;
constexpr int PLAYER_START_ROW = 23;

// Ghost release timers (seconds in ghost house)
constexpr float GHOST_RELEASE[4] = { 0.f, 0.f, 3.f, 6.f };

// Colors (RGBA)
namespace Color {
    constexpr SDL_Color WALL       = { 25,  50, 140, 255 };   // deep ocean blue
    constexpr SDL_Color WALL_INNER = { 10,  25,  80, 255 };
    constexpr SDL_Color DOT        = {220, 180,  40, 255 };   // gold coin
    constexpr SDL_Color POWER_DOT  = {255, 215,   0, 255 };   // bright gold
    constexpr SDL_Color BG         = {  5,   5,  20, 255 };   // night ocean
    constexpr SDL_Color DOOR       = {255, 182, 193, 255 };   // pink door
    constexpr SDL_Color TEXT_WHITE = {255, 255, 255, 255 };
    constexpr SDL_Color TEXT_GOLD  = {255, 215,   0, 255 };
    constexpr SDL_Color FRIGHT     = { 30,  30, 200, 255 };
    constexpr SDL_Color FRIGHT_FL  = {255, 255, 255, 255 };
}

} // namespace C
