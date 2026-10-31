// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

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
    void Say(const std::string& text, uint8_t who, const coop::VoiceStyle& style, float volume, float pan);
    float Level(uint8_t who);

private:
    struct Line { std::string text; uint8_t who; coop::VoiceStyle style; float volume, pan; };
    struct Sounding { uint8_t who; std::vector<int16_t> samples; unsigned long start; };
    std::vector<Sounding> m_sounding;
    void Run();

    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::deque<Line> m_lines;
    bool m_stop = false;
    std::thread m_thread;
};
