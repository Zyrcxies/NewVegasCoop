// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstdint>
#include <deque>
#include <map>

namespace coop
{
    constexpr uint32_t kVoiceRate = 24000;
    constexpr size_t kVoiceStartSamples = kVoiceRate * 60 / 1000;
    constexpr size_t kVoiceMaxSamples = kVoiceRate * 400 / 1000;

    class VoiceMixer
    {
    public:
        void Push(uint8_t who, const int16_t* samples, size_t count);
        void SetMix(uint8_t who, float volume, float pan);
        void Mix(int16_t* stereo, size_t frames);
        bool Talking(uint8_t who) const;
        float Level(uint8_t who) const;
        void Forget(uint8_t who) { m_voices.erase(who); }

    private:
        struct Voice
        {
            std::deque<int16_t> waiting;
            bool playing = false;
            float volume = 0, pan = 0;
            float left = 0, right = 0;
            float level = 0;
        };
        std::map<uint8_t, Voice> m_voices;
    };

    float MouthOpening(float level);

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
