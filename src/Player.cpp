#include "Player.h"
#include <cmath>

static constexpr int START_LIVES = 3;

Player::Player() {
    placeAt({ C::PLAYER_START_COL, C::PLAYER_START_ROW });
    m_dir          = { 0, 0 };
    m_nextDir      = { -1, 0 };
    m_moveProgress = 0.f;
    m_powerTimer   = 0.f;
    m_alive        = true;
    m_lives        = START_LIVES;
    m_score        = 0;
    m_justCollected = false;
    m_justPowered   = false;
}

void Player::placeAt(Vec2i tile) {
    m_tile       = tile;
    m_targetTile = tile;
    m_pos        = { (float)(tile.x * C::TILE), (float)(tile.y * C::TILE) };
    m_moveProgress = 0.f;
}

void Player::loadSprites(SDL_Renderer* ren) {
    m_walkSheet.load(ren, "assets/sprites/player_walk.png",
                     130, 130, {200, 100, 30, 255});
    m_idleSheet.load(ren, "assets/sprites/player_idle.png",
                     130, 130, {180, 80, 20, 255});
    setupAnimations();
}

void Player::setupAnimations() {
    std::vector<int> wf, idf;
    for (int i = 0; i < m_walkSheet.frameCount(); ++i) wf.push_back(i);
    for (int i = 0; i < m_idleSheet.frameCount(); ++i) idf.push_back(i);
    m_anim.addClip("walk",  { wf,  10.f, true  });
    m_anim.addClip("idle",  { idf,  8.f, true  });
    m_anim.addClip("death", { {0},  4.f, false });
    m_anim.play("idle");
}

void Player::reset() {
    placeAt({ C::PLAYER_START_COL, C::PLAYER_START_ROW });
    m_dir          = { 0, 0 };
    m_nextDir      = { -1, 0 };
    m_powerTimer   = 0.f;
    m_alive        = true;
    m_lives        = START_LIVES;
    m_score        = 0;
    m_justCollected = false;
    m_justPowered   = false;
}

void Player::revive() {
    placeAt({ C::PLAYER_START_COL, C::PLAYER_START_ROW });
    m_dir     = { 0, 0 };
    m_nextDir = { -1, 0 };
    m_powerTimer = 0.f;
    m_alive      = true;
    m_anim.play("idle");
}

void Player::die() {
    m_alive = false;
    --m_lives;
    m_anim.play("death", true);
}

void Player::handleEvent(const SDL_Event& e) {
    if (e.type != SDL_KEYDOWN) return;
    switch (e.key.keysym.sym) {
        case SDLK_UP:    case SDLK_w: m_nextDir = { 0,-1}; break;
        case SDLK_DOWN:  case SDLK_s: m_nextDir = { 0, 1}; break;
        case SDLK_LEFT:  case SDLK_a: m_nextDir = {-1, 0}; break;
        case SDLK_RIGHT: case SDLK_d: m_nextDir = { 1, 0}; break;
        default: break;
    }
}

// Try to begin moving in buffered direction, fall back to current direction.
void Player::tryStartMove(Map& map) {
    // Prefer buffered direction (allows turning at corners)
    if (m_nextDir != Vec2i{0,0}) {
        Vec2i next = map.wrap({ m_tile.x + m_nextDir.x, m_tile.y + m_nextDir.y });
        if (map.isWalkable(next)) {
            m_dir        = m_nextDir;
            m_targetTile = next;
            return;
        }
    }
    // Continue in current direction
    if (m_dir != Vec2i{0,0}) {
        Vec2i next = map.wrap({ m_tile.x + m_dir.x, m_tile.y + m_dir.y });
        if (map.isWalkable(next)) {
            m_targetTile = next;
            return;
        }
    }
    // Wall ahead in every valid direction — stay put
    m_targetTile = m_tile;
}

void Player::collectAt(Vec2i t, Map& map) {
    Tile tile = map.get(t);
    if (tile == Tile::DOT) {
        map.collectDot(t);
        m_score += C::PT_DOT;
        m_justCollected = true;
    } else if (tile == Tile::POWER) {
        map.collectPower(t);
        m_score += C::PT_POWER;
        m_justPowered   = true;
        m_powerTimer    = C::FRIGHT_BASE;
    }
}

void Player::update(float dt, Map& map) {
    m_justCollected = false;
    m_justPowered   = false;

    if (!m_alive) {
        m_anim.update(dt);
        return;
    }

    if (m_powerTimer > 0.f) m_powerTimer -= dt;

    bool moving = (m_targetTile != m_tile);

    if (moving) {
        // Advance position toward target tile
        m_moveProgress += m_speed / C::TILE * dt;

        if (m_moveProgress >= 1.f) {
            // Commit to target tile
            m_tile         = map.wrap(m_targetTile);
            m_targetTile   = m_tile;
            m_moveProgress = 0.f;
            collectAt(m_tile, map);
            tryStartMove(map);
        }
    } else {
        tryStartMove(map);
    }

    // --- Smooth pixel position ---
    // Tunnel transitions (|dx|>1) look fine because SDL clips negative coords.
    if (m_targetTile != m_tile) {
        m_pos.x = (m_tile.x + m_dir.x * m_moveProgress) * C::TILE;
        m_pos.y = (m_tile.y + m_dir.y * m_moveProgress) * C::TILE;
    } else {
        m_pos.x = (float)(m_tile.x * C::TILE);
        m_pos.y = (float)(m_tile.y * C::TILE);
    }

    if (m_targetTile != m_tile) m_anim.play("walk");
    else                        m_anim.play("idle");
    m_anim.update(dt);
}

void Player::render(SDL_Renderer* ren) const {
    int px = (int)m_pos.x;
    int py = (int)m_pos.y + C::HUD_H;

    bool flipH = (m_dir.x > 0);
    SDL_Color tint = {255, 255, 255, 255};

    const std::string& clip = m_anim.currentClip();
    const SpriteSheet* sheet = (clip == "idle" || clip == "death")
                               ? &m_idleSheet : &m_walkSheet;

    sheet->draw(ren, m_anim.currentFrame(),
                px, py, C::TILE, C::TILE, flipH, tint);
}
