#pragma once

// Voice chat's sound out: the others' voices (coop::VoiceMixer) played as one stereo stream through Windows' wave out,
// on a thread of its own, 20 ms at a time.

#include "coop/voice.h"

#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

class VoiceOut
{
public:
    ~VoiceOut();
    void Push(uint8_t who, const std::vector<int16_t>& samples);  // starts the stream on first use
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
