#include "AudioGen.h"
#include <cmath>
#include <algorithm>
#include <cstring>

static constexpr float PI2 = 6.28318530718f;

void AudioGen::appendSine(std::vector<int16_t>& buf, int freq, int samples, float amp) {
    if (freq <= 0 || samples <= 0) { appendSilence(buf, samples); return; }
    float phase = 0.f;
    float inc   = PI2 * freq / RATE;
    int   peak  = (int)(amp * 32767.f);
    for (int i = 0; i < samples; ++i) {
        buf.push_back((int16_t)(std::sin(phase) * peak));
        phase += inc;
        if (phase > PI2) phase -= PI2;
    }
}

void AudioGen::appendSquare(std::vector<int16_t>& buf, int freq, int samples, float amp) {
    if (freq <= 0 || samples <= 0) { appendSilence(buf, samples); return; }
    int period = RATE / freq;
    int peak   = (int)(amp * 32767.f);
    for (int i = 0; i < samples; ++i)
        buf.push_back((int16_t)((i % period < period / 2) ? peak : -peak));
}

void AudioGen::appendSilence(std::vector<int16_t>& buf, int samples) {
    buf.resize(buf.size() + samples, 0);
}

Mix_Chunk* AudioGen::buildChunk(const std::vector<int16_t>& mono) {
    // Convert mono → stereo (L = R = same sample)
    int stereoBytes = (int)mono.size() * 2 * (int)sizeof(int16_t);
    Uint8* raw = (Uint8*)SDL_malloc(stereoBytes);
    if (!raw) return nullptr;
    int16_t* out = (int16_t*)raw;
    for (size_t i = 0; i < mono.size(); ++i) {
        out[i * 2]     = mono[i];
        out[i * 2 + 1] = mono[i];
    }
    Mix_Chunk* c = (Mix_Chunk*)SDL_malloc(sizeof(Mix_Chunk));
    if (!c) { SDL_free(raw); return nullptr; }
    c->allocated = 1;
    c->abuf      = raw;
    c->alen      = (Uint32)stereoBytes;
    c->volume    = MIX_MAX_VOLUME;
    return c;
}

Mix_Chunk* AudioGen::makeSequence(const std::vector<Tone>& tones) {
    std::vector<int16_t> buf;
    for (auto& t : tones) {
        int s = ms2samples(t.durationMs);
        if (t.square) appendSquare(buf, t.freqHz, s, t.amplitude);
        else          appendSine  (buf, t.freqHz, s, t.amplitude);
    }
    return buildChunk(buf);
}

// ── Pre-built sounds ──────────────────────────────────────────────────────────

Mix_Chunk* AudioGen::dotEat() {
    // Two alternating blips — caller cycles between them to create the
    // classic waka-waka effect.  We just return one blip here.
    return makeSequence({ {940, 25, 0.25f, true}, {0, 10, 0} });
}

Mix_Chunk* AudioGen::powerEat() {
    return makeSequence({
        {880, 40, 0.4f, false},
        {660, 40, 0.4f, false},
        {440, 60, 0.4f, false},
        {220, 80, 0.3f, false},
        {  0, 20, 0   },
    });
}

Mix_Chunk* AudioGen::ghostEat(int combo) {
    // combo 0→200 pts, 1→400, 2→800, 3→1600
    static const int freqs[4] = {400, 560, 800, 1120};
    int f = freqs[std::clamp(combo, 0, 3)];
    return makeSequence({
        {f,     30, 0.5f, false},
        {f*2,   30, 0.5f, false},
        {  0,   20, 0   },
    });
}

Mix_Chunk* AudioGen::playerDeath() {
    // Chromatic descending glide
    std::vector<Tone> seq;
    int freqs[] = {494,466,440,415,392,370,349,330,311,294,277,261,247,0};
    for (int f : freqs)
        seq.push_back({f, 60, 0.45f, false});
    seq.push_back({0, 100, 0});
    return makeSequence(seq);
}

Mix_Chunk* AudioGen::levelClear() {
    return makeSequence({
        {523, 80,  0.4f, false},
        {659, 80,  0.4f, false},
        {784, 80,  0.4f, false},
        {1047,120, 0.5f, false},
        {0,   60,  0   },
        {784, 60,  0.35f,false},
        {1047,180, 0.5f, false},
        {0,   80,  0   },
    });
}

Mix_Chunk* AudioGen::fruitEat() {
    return makeSequence({
        {1047, 40, 0.5f, false},
        {1319, 40, 0.5f, false},
        {1568, 60, 0.5f, false},
        {0,    30, 0   },
    });
}

Mix_Chunk* AudioGen::siren(int tier) {
    // Two alternating sine tones — pitch and tempo both rise with tier.
    // Chunk loops seamlessly in SDL_mixer (played with -1 loops flag).
    static const int LO[5] = {160, 175, 195, 215, 240};
    static const int HI[5] = {195, 215, 240, 270, 305};
    int lo = LO[std::clamp(tier, 0, 4)];
    int hi = HI[std::clamp(tier, 0, 4)];
    int ms = 170 - tier * 20;          // 170/150/130/110/90 ms per note
    std::vector<int16_t> buf;
    appendSine(buf, lo, ms2samples(ms), 0.07f);
    appendSine(buf, hi, ms2samples(ms), 0.07f);
    return buildChunk(buf);
}
