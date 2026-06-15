#pragma once
#include <SDL2/SDL.h>
#include "Vec2.h"
#include "Map.h"
#include "AStar.h"
#include "InfluenceMap.h"
#include "SpriteSheet.h"
#include "Animation.h"
#include "Constants.h"

enum class GhostType  { CHASER=0, AMBUSHER=1, FLANKER=2, ERRATIC=3 };
enum class GhostState { HOUSE, CHASE, SCATTER, FRIGHTENED, DEAD };

struct GhostSnapshot { Vec2i tile; Vec2i dir; };

class Ghost {
public:
    explicit Ghost(GhostType type);

    // frightSheet is shared across all ghosts; caller owns it.
    void loadSprites(SDL_Renderer* ren, SpriteSheet* sharedFrightSheet = nullptr);
    void reset(int level);

    void update(float dt, const Map& map,
                const GhostSnapshot& player,
                const GhostSnapshot& chaser,
                const InfluenceMap&  influence,
                int dotsRemaining, int level);

    void render(SDL_Renderer* ren) const;

    Vec2i      tile()      const { return m_tile; }
    Vec2i      dir()       const { return m_dir; }
    GhostState state()     const { return m_state; }
    GhostType  type()      const { return m_type; }
    bool       isEdible()  const { return m_state == GhostState::FRIGHTENED; }
    bool       isDead()    const { return m_state == GhostState::DEAD; }

    void frighten(float duration);
    void kill();

    static void advanceWave(float dt, int level);
    static GhostState currentWave() { return s_wave; }
    static void       resetWave()   { s_waveIdx = 0; s_waveTimer = 0.f;
                                      s_wave = GhostState::SCATTER; }

private:
    GhostType  m_type;
    GhostState m_state;
    Vec2i      m_tile;        // current tile (origin of move in progress)
    Vec2i      m_cachedNext;  // A*-computed next tile (cached until tile commits)
    Vec2i      m_cachedTarget;
    bool       m_pathDirty;   // recompute A* on next moveToward call
    Vec2i      m_dir;
    float      m_moveProgress;// 0..1 from m_tile toward m_cachedNext
    float      m_frighTimer;
    float      m_flashTimer;
    bool       m_flashOn;
    float      m_houseTimer;
    float      m_bobAccum;    // oscillation phase when inside ghost house

    SpriteSheet  m_sheet;
    SpriteSheet* m_frighSheet = nullptr;  // shared; not owned by this ghost
    bool         m_ownsFright = false;    // true only when no shared sheet provided
    SpriteSheet  m_frighSheetOwned;       // storage when owned
    Animation    m_anim;

    Vec2i computeTarget(const Map& map,
                        const GhostSnapshot& player,
                        const GhostSnapshot& chaser,
                        const InfluenceMap&  influence) const;
    Vec2i scatterCorner() const;
    Vec2i houseExit()     const;
    float speed(int level, int dotsRemaining) const;
    bool  inTunnel()      const;

    void moveToward(Vec2i target, const Map& map, float dt, float spd);
    void reverseDir();

    static GhostState s_wave;
    static int        s_waveIdx;
    static float      s_waveTimer;
};
