#pragma once

// Voice chat (Alex, October 2026: directional, gmod-style). Each game mixes the others' voices into one stereo stream:
// a little waits before a voice starts (packets come unevenly over the network), too much waiting is dropped (we'd
// fall behind), and each voice has the volume and side of where its player is (coop::MixSpeech), changed smoothly.
// Game-independent: tested in tests/.

#include <cstdint>
#include <deque>
#include <map>

namespace coop
{
    constexpr uint32_t kVoiceRate = 24000;                          // samples a second (mono in, stereo out)
    constexpr size_t kVoiceStartSamples = kVoiceRate * 60 / 1000;  // waiting before a voice plays (60 ms)
    constexpr size_t kVoiceMaxSamples = kVoiceRate * 400 / 1000;   // waiting at most (the oldest is dropped)

    class VoiceMixer
    {
    public:
        void Push(uint8_t who, const int16_t* samples, size_t count);
        void SetMix(uint8_t who, float volume, float pan);  // volume 0 to 1, pan -1 left to 1 right
        void Mix(int16_t* stereo, size_t frames);           // the next `frames` (left, right pairs)
        bool Talking(uint8_t who) const;                    // their voice is waiting or playing
        float Level(uint8_t who) const;                     // how loud the last block of theirs was (0 to 32767)
        void Forget(uint8_t who) { m_voices.erase(who); }

    private:
        struct Voice
        {
            std::deque<int16_t> waiting;
            bool playing = false;
            float volume = 0, pan = 0;
            float left = 0, right = 0;  // the gains last used (moved towards volume and pan a block at a time)
            float level = 0;            // the last block's average loudness (before volume)
        };
        std::map<uint8_t, Voice> m_voices;
    };

    // The mouth's opening (0 to 1) for a voice this loud (gmod-style: shut when quiet, wide when loud).
    float MouthOpening(float level);

    // Spam limit for one player's voice packets (Steam's voice is a few KB a second).
    class VoiceLimiter
    {
    public:
        static constexpr uint32_t kPacketsPerSecond = 60;
        static constexpr uint32_t kBytesPerSecond = 16000;
        bool Allow(uint32_t now, size_t bytes);

    private:
        uint32_t m_second = 0, m_packets = 0, m_bytes = 0;
        bool m_started = false;
    };
}
