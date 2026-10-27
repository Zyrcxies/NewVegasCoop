#pragma once

// Text-to-speech for chat lines (Alex, October 2026: optional, gmod-style): Windows' own voices (SAPI) say a line
// into memory on a thread of its own, and it plays at the volume and on the side given (from where the speaker is:
// coop::MixSpeech). Each player gets a voice and pitch of their own. Lines play over each other, as people talk.

#include "coop/chat.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class Speech
{
public:
    ~Speech();
    // A line to say (printable text: coop::CleanChat's) as the speaker chose (their voice, pitch, speed, effect).
    // Started on first use. Too many waiting: the oldest is dropped.
    void Say(const std::string& text, uint8_t who, const coop::VoiceStyle& style, float volume, float pan);
    float Level(uint8_t who);  // how loud `who`'s line playing now is (0 to 32767; 0: none)

private:
    struct Line { std::string text; uint8_t who; coop::VoiceStyle style; float volume, pan; };
    struct Sounding { uint8_t who; std::vector<int16_t> samples; unsigned long start; };  // (start: GetTickCount)
    std::vector<Sounding> m_sounding;  // lines playing (under m_mutex)
    void Run();

    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::deque<Line> m_lines;
    bool m_stop = false;
    std::thread m_thread;
};
