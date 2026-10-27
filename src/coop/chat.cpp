#include "coop/chat.h"

#include <cmath>

namespace coop
{
    std::string CleanChat(std::string_view text)
    {
        std::string out;
        for (char c : text)
        {
            auto u = static_cast<unsigned char>(c);
            if (u == '\t')
                u = ' ';
            if (u < 0x20 || u > 0x7E)
                continue;
            if (u == '&')
                u = '+';
            else if (u == '<')
                u = '(';
            else if (u == '>')
                u = ')';
            if (u == ' ' && (out.empty() || out.back() == ' '))
                continue;
            if (out.size() >= kMaxChatLength)
                break;
            out.push_back(static_cast<char>(u));
        }
        while (!out.empty() && out.back() == ' ')
            out.pop_back();
        return out;
    }

    SpeechMix MixSpeech(float dx, float dy, float dz, float heading)
    {
        SpeechMix mix;
        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance >= kSpeechSilentRange)
            return SpeechMix{0, 0};
        if (distance > kSpeechFullRange)
            mix.volume = 1 - (distance - kSpeechFullRange) / (kSpeechSilentRange - kSpeechFullRange);
        float flat = std::sqrt(dx * dx + dy * dy);
        if (flat > 1)
            mix.pan = std::sin(std::atan2(dx, dy) - heading);  // (right of where the listener looks: positive)
        return mix;
    }

    bool VoiceStyle::Valid() const
    {
        return voice <= 1 && variant <= 3 && pitch >= -10 && pitch <= 10 && speed >= -10 && speed <= 10 &&
            effect < static_cast<uint8_t>(VoiceEffect::Count);
    }

    namespace
    {
        // One-pole filters: a low-pass (what's under `hz` stays) and a high-pass (what's over it stays).
        struct LowPass
        {
            float a, y = 0;
            LowPass(float hz, float rate) : a(1 - std::exp(-6.2831853f * hz / rate)) {}
            float operator()(float x) { return y += a * (x - y); }
        };
        struct HighPass
        {
            LowPass low;
            HighPass(float hz, float rate) : low(hz, rate) {}
            float operator()(float x) { return x - low(x); }
        };

        int16_t Clamp(float x)
        {
            return static_cast<int16_t>(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
        }
    }

    void ApplyVoiceEffect(std::vector<int16_t>& samples, uint32_t rate, VoiceEffect effect)
    {
        if (samples.empty() || rate == 0)
            return;
        float r = static_cast<float>(rate);
        switch (effect)
        {
        case VoiceEffect::Radio:
        {
            // A walkie-talkie: thin (300 to 3000 Hz), driven a little, with a hiss.
            HighPass high(300, r);
            LowPass low1(3000, r), low2(3000, r);
            uint32_t noise = 12345;
            for (int16_t& s : samples)
            {
                float x = low2(low1(high(static_cast<float>(s)))) * 2.2f;
                x = 26000 * std::tanh(x / 26000);
                noise = noise * 1664525u + 1013904223u;
                x += static_cast<float>(static_cast<int32_t>(noise >> 16) - 32768) * 0.03f;
                s = Clamp(x);
            }
            break;
        }
        case VoiceEffect::Robot:
        {
            // Metallic: ring modulation at 55 Hz, then a short ringing comb (6 ms).
            size_t delay = static_cast<size_t>(r * 0.006f);
            std::vector<float> out(samples.size());
            for (size_t i = 0; i < samples.size(); ++i)
            {
                float t = static_cast<float>(i) / r;
                float x = static_cast<float>(samples[i]) * (0.35f + 0.65f * std::sin(6.2831853f * 55 * t));
                if (i >= delay)
                    x += out[i - delay] * 0.55f;
                out[i] = x;
            }
            for (size_t i = 0; i < samples.size(); ++i)
                samples[i] = Clamp(out[i] * 0.8f);
            break;
        }
        case VoiceEffect::Echo:
        {
            // Two echoes a fifth of a second apart, each fainter.
            size_t delay = static_cast<size_t>(r * 0.22f);
            size_t length = samples.size();
            std::vector<float> out(length + delay * 2, 0.0f);
            for (size_t i = 0; i < length; ++i)
            {
                float x = static_cast<float>(samples[i]);
                out[i] += x * 0.8f;
                out[i + delay] += x * 0.4f;
                out[i + delay * 2] += x * 0.18f;
            }
            samples.resize(out.size());
            for (size_t i = 0; i < out.size(); ++i)
                samples[i] = Clamp(out[i]);
            break;
        }
        case VoiceEffect::Megaphone:
        {
            // Loud and harsh: 500 to 3500 Hz, clipped hard.
            HighPass high(500, r);
            LowPass low(3500, r);
            for (int16_t& s : samples)
            {
                float x = low(high(static_cast<float>(s))) * 5;
                s = Clamp(24000 * std::tanh(x / 12000));
            }
            break;
        }
        default:
            break;
        }
    }

    ChatDraft::Done ChatDraft::Type(unsigned char c)
    {
        if (c == '\r' || c == '\n')
            return Done::Send;
        if (c == 0x1B)
        {
            m_text.clear();
            return Done::Cancel;
        }
        if (c == '\b')
        {
            if (!m_text.empty())
                m_text.pop_back();
        }
        else if (c >= 0x20 && c <= 0x7E && m_text.size() < kMaxChatLength)
            m_text.push_back(static_cast<char>(c));
        return Done::No;
    }

    std::string ChatDraft::Take()
    {
        std::string text = std::move(m_text);
        m_text.clear();
        return text;
    }

    bool ChatLimiter::Allow(uint32_t now)
    {
        if (m_count == 0 || now - m_windowStart >= kChatWindowMs)
        {
            m_windowStart = now;
            m_count = 0;
        }
        if (m_count >= kChatLinesPerWindow)
            return false;
        ++m_count;
        return true;
    }
}
