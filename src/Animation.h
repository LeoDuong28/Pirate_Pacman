#pragma once
#include <vector>
#include <string>

struct AnimClip {
    std::vector<int> frames;   // indices into SpriteSheet
    float            fps;
    bool             loop;
};

class Animation {
public:
    Animation() = default;

    void addClip(const std::string& name, AnimClip clip) {
        m_clips.push_back({name, clip});
    }

    void play(const std::string& name, bool restart = false) {
        for (int i = 0; i < (int)m_clips.size(); ++i) {
            if (m_clips[i].first == name) {
                if (m_current == i && !restart) return;
                m_current  = i;
                m_elapsed  = 0.f;
                m_frameIdx = 0;
                return;
            }
        }
    }

    void update(float dt) {
        if (m_current < 0) return;
        const AnimClip& clip = m_clips[m_current].second;
        if (clip.frames.empty()) return;

        m_elapsed += dt;
        float frameDur = 1.f / clip.fps;
        while (m_elapsed >= frameDur) {
            m_elapsed -= frameDur;
            ++m_frameIdx;
            if (m_frameIdx >= (int)clip.frames.size()) {
                m_frameIdx = clip.loop ? 0 : (int)clip.frames.size() - 1;
            }
        }
    }

    int currentFrame() const {
        if (m_current < 0) return 0;
        const AnimClip& clip = m_clips[m_current].second;
        if (clip.frames.empty()) return 0;
        return clip.frames[m_frameIdx];
    }

    bool isDone() const {
        if (m_current < 0) return true;
        const AnimClip& clip = m_clips[m_current].second;
        return !clip.loop && m_frameIdx == (int)clip.frames.size() - 1;
    }

    const std::string& currentClip() const {
        static std::string none;
        if (m_current < 0) return none;
        return m_clips[m_current].first;
    }

private:
    std::vector<std::pair<std::string, AnimClip>> m_clips;
    int   m_current  = -1;
    int   m_frameIdx = 0;
    float m_elapsed  = 0.f;
};
