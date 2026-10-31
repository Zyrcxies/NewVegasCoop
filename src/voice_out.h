// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include "coop/voice.h"

#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

class VoiceOut
{
public:
    ~VoiceOut();
    void Push(uint8_t who, const std::vector<int16_t>& samples);
    void SetMix(uint8_t who, float volume, float pan);
    bool Talking(uint8_t who);
    float Level(uint8_t who);
    void Forget(uint8_t who);

private:
    void Run();

    std::mutex m_mutex;
    coop::VoiceMixer m_mixer;
    bool m_stop = false;
    std::thread m_thread;
};
