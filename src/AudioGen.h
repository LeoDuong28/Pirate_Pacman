#pragma once
#include <SDL2/SDL_mixer.h>
#include <vector>
#include <cstdint>

// Generates Mix_Chunk objects from synthesised PCM — no WAV files required.
// All chunks are stereo 16-bit signed @ 44100 Hz to match Mix_OpenAudio defaults.
class AudioGen {
public:
    static constexpr int RATE = 44100;

    struct Tone {
        int   freqHz;      // 0 = silence
        int   durationMs;
        float amplitude;   // 0..1
        bool  square = false; // true = square wave, false = sine
    };

    // Build a chunk from a sequence of tones concatenated end-to-end.
    static Mix_Chunk* makeSequence(const std::vector<Tone>& tones);

    // ── Pre-built game sounds ────────────────────────────────────────────────
    static Mix_Chunk* dotEat();           // short blip, alternates two pitches
    static Mix_Chunk* powerEat();         // descending cascade
    static Mix_Chunk* ghostEat(int combo);// rising pitch per combo (1-4)
    static Mix_Chunk* playerDeath();      // chromatic descent
    static Mix_Chunk* levelClear();       // ascending fanfare
    static Mix_Chunk* fruitEat();         // coin jingle
    static Mix_Chunk* siren(int tier);   // tier 0-4: background siren, pitch rises with tier

private:
    static Mix_Chunk* buildChunk(const std::vector<int16_t>& mono);
    static void       appendSine  (std::vector<int16_t>& buf, int freq, int samples, float amp);
    static void       appendSquare(std::vector<int16_t>& buf, int freq, int samples, float amp);
    static void       appendSilence(std::vector<int16_t>& buf, int samples);
    static int        ms2samples(int ms) { return RATE * ms / 1000; }
};
