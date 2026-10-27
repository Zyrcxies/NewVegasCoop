#pragma once

// Text chat (Alex, October 2026: lines float above the player's head like subtitles). What a player types goes to
// everyone (a friend's through the host's game); each game shows it above the sender's character.
// Game-independent: tested in tests/.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace coop
{
    constexpr size_t kMaxChatLength = 160;      // characters in one line
    constexpr int kChatLinesPerWindow = 5;      // a player's lines in kChatWindowMs at most (more are dropped)
    constexpr uint32_t kChatWindowMs = 10000;

    // A line as shown: printable characters only (the game's text tiles read '&' and '<' as markup: those become
    // '+' and '('), runs of spaces as one, no spaces at the ends, cut to kMaxChatLength. Empty: nothing to say.
    std::string CleanChat(std::string_view text);

    // The line being typed, a character at a time (Windows' typed characters: Enter, Esc and Backspace among them).
    class ChatDraft
    {
    public:
        enum class Done { No, Send, Cancel };
        Done Type(unsigned char c);  // Enter: Send (the text is taken with Take); Esc: Cancel (the text is gone)
        const std::string& Text() const { return m_text; }
        std::string Take();

    private:
        std::string m_text;
    };

    // Text-to-speech, heard from where the speaker is (Alex: gmod-style): full volume up close, fading out by
    // kSpeechSilentRange; panned to the side they're on.
    constexpr float kSpeechFullRange = 400;     // game units (about 6 m)
    constexpr float kSpeechSilentRange = 3000;  // (about 43 m)
    struct SpeechMix
    {
        float volume = 1;  // 0 to 1
        float pan = 0;     // -1 left to 1 right
    };
    // dx, dy, dz: from the listener to the speaker; heading: the listener's (radians, 0 north, clockwise).
    SpeechMix MixSpeech(float dx, float dy, float dz, float heading);

    // How a player's lines are read out, as they chose (Alex: voice options): sent with their lines.
    enum class VoiceEffect : uint8_t { None, Radio, Robot, Echo, Megaphone, Count };
    struct VoiceStyle
    {
        uint8_t voice = 0;    // 0 male, 1 female (a Windows voice of that kind)
        uint8_t variant = 0;  // which of this PC's voices of that kind (0 to 3; fewer there: counted round)
        int8_t pitch = 0;     // -10 to 10
        int8_t speed = 0;     // -10 to 10
        uint8_t effect = 0;   // a VoiceEffect
        bool Valid() const;
    };
    // The effect on a said line (16-bit mono samples at `rate` per second); Echo makes it longer.
    void ApplyVoiceEffect(std::vector<int16_t>& samples, uint32_t rate, VoiceEffect effect);

    // Spam limit for one player's lines.
    class ChatLimiter
    {
    public:
        bool Allow(uint32_t now);  // counts the line when allowed

    private:
        uint32_t m_windowStart = 0;
        int m_count = 0;
    };
}
