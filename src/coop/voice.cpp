#include "coop/voice.h"

#include <cmath>
#include <vector>

namespace coop
{
    void VoiceMixer::Push(uint8_t who, const int16_t* samples, size_t count)
    {
        if (!samples || count == 0 || (m_voices.size() >= 16 && !m_voices.count(who)))
            return;
        Voice& v = m_voices[who];
        v.waiting.insert(v.waiting.end(), samples, samples + count);
        if (v.waiting.size() > kVoiceMaxSamples)
            v.waiting.erase(v.waiting.begin(), v.waiting.begin() + (v.waiting.size() - kVoiceMaxSamples));
    }

    void VoiceMixer::SetMix(uint8_t who, float volume, float pan)
    {
        auto it = m_voices.find(who);
        if (it == m_voices.end())
            return;
        it->second.volume = volume < 0 ? 0 : volume > 1 ? 1 : volume;
        it->second.pan = pan < -1 ? -1 : pan > 1 ? 1 : pan;
    }

    float VoiceMixer::Level(uint8_t who) const
    {
        auto it = m_voices.find(who);
        return it == m_voices.end() ? 0 : it->second.level;
    }

    float MouthOpening(float level)
    {
        constexpr float kQuiet = 600, kLoud = 3600;
        if (level <= kQuiet)
            return 0;
        return level >= kLoud ? 1 : (level - kQuiet) / (kLoud - kQuiet);
    }

    bool VoiceMixer::Talking(uint8_t who) const
    {
        auto it = m_voices.find(who);
        return it != m_voices.end() && (it->second.playing || !it->second.waiting.empty());
    }

    void VoiceMixer::Mix(int16_t* stereo, size_t frames)
    {
        if (!stereo || frames == 0)
            return;
        std::vector<float> sum(frames * 2, 0.0f);
        for (auto& [who, v] : m_voices)
        {
            if (!v.playing && v.waiting.size() >= kVoiceStartSamples)
                v.playing = true;
            v.level = 0;
            if (!v.playing)
                continue;
            // Equal-power panning, moved to over this block (no clicks when they walk past).
            float angle = (v.pan + 1) * 0.785398f;
            float left = v.volume * std::cos(angle), right = v.volume * std::sin(angle);
            size_t n = frames < v.waiting.size() ? frames : v.waiting.size();
            float loudness = 0;
            for (size_t i = 0; i < n; ++i)
                loudness += std::fabs(static_cast<float>(v.waiting[i]));
            v.level = n ? loudness / static_cast<float>(n) : 0;
            for (size_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i + 1) / static_cast<float>(frames);
                float x = static_cast<float>(v.waiting[i]);
                sum[i * 2] += x * (v.left + (left - v.left) * t);
                sum[i * 2 + 1] += x * (v.right + (right - v.right) * t);
            }
            v.waiting.erase(v.waiting.begin(), v.waiting.begin() + n);
            v.left = left;
            v.right = right;
            if (v.waiting.empty())
                v.playing = false;  // (ran dry: it waits again before going on)
        }
        for (size_t i = 0; i < frames * 2; ++i)
            stereo[i] = static_cast<int16_t>(sum[i] > 32767 ? 32767 : sum[i] < -32768 ? -32768 : sum[i]);
    }

    bool VoiceLimiter::Allow(uint32_t now, size_t bytes)
    {
        if (!m_started || now - m_second >= 1000)
        {
            m_started = true;
            m_second = now;
            m_packets = 0;
            m_bytes = 0;
        }
        if (m_packets >= kPacketsPerSecond || m_bytes + bytes > kBytesPerSecond)
            return false;
        ++m_packets;
        m_bytes += static_cast<uint32_t>(bytes);
        return true;
    }
}
