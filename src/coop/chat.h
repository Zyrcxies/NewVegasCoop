// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace coop
{
    constexpr size_t kMaxChatLength = 160;
    constexpr int kChatLinesPerWindow = 5;
    constexpr uint32_t kChatWindowMs = 10000;

    std::string CleanChat(std::string_view text);

    class ChatDraft
    {
    public:
        enum class Done { No, Send, Cancel };
        Done Type(unsigned char c);
        const std::string& Text() const { return m_text; }
        std::string Take();

    private:
        std::string m_text;
    };

    constexpr float kSpeechFullRange = 400;
    constexpr float kSpeechSilentRange = 3000;
    struct SpeechMix
    {
        float volume = 1;
        float pan = 0;
    };
    SpeechMix MixSpeech(float dx, float dy, float dz, float heading);

    enum class VoiceEffect : uint8_t { None, Radio, Robot, Echo, Megaphone, Count };
    struct VoiceStyle
    {
        uint8_t voice = 0;
        uint8_t variant = 0;
        int8_t pitch = 0;
        int8_t speed = 0;
        uint8_t effect = 0;
        bool Valid() const;
    };
    void ApplyVoiceEffect(std::vector<int16_t>& samples, uint32_t rate, VoiceEffect effect);

    class ChatLimiter
    {
    public:
        bool Allow(uint32_t now);

    private:
        uint32_t m_windowStart = 0;
        int m_count = 0;
    };
}
