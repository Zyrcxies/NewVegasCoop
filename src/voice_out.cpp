#include "voice_out.h"

#include "log.h"

#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

namespace
{
    constexpr size_t kBlockFrames = coop::kVoiceRate / 50;  // 20 ms
    constexpr int kBlocks = 4;                              // queued with Windows at once (80 ms)
}

VoiceOut::~VoiceOut()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    if (m_thread.joinable())
        m_thread.detach();  // (the game is closing: its threads may be gone already)
}

void VoiceOut::Push(uint8_t who, const std::vector<int16_t>& samples)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_thread.joinable() && !m_stop)
        m_thread = std::thread(&VoiceOut::Run, this);
    m_mixer.Push(who, samples.data(), samples.size());
}

void VoiceOut::SetMix(uint8_t who, float volume, float pan)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mixer.SetMix(who, volume, pan);
}

bool VoiceOut::Talking(uint8_t who)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_mixer.Talking(who);
}

float VoiceOut::Level(uint8_t who)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_mixer.Level(who);
}

void VoiceOut::Forget(uint8_t who)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mixer.Forget(who);
}

void VoiceOut::Run()
{
    HANDLE done = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    WAVEFORMATEX format = {WAVE_FORMAT_PCM, 2, coop::kVoiceRate, coop::kVoiceRate * 4, 4, 16, 0};
    HWAVEOUT out = nullptr;
    if (!done || waveOutOpen(&out, WAVE_MAPPER, &format, reinterpret_cast<DWORD_PTR>(done), 0, CALLBACK_EVENT) !=
        MMSYSERR_NOERROR)
    {
        LOG_ERROR("Voice: Windows' sound out didn't open; voices aren't heard");
        if (done)
            CloseHandle(done);
        return;
    }
    LOG_INFO("Voice: playing others' voices");
    std::vector<int16_t> buffers[kBlocks];
    WAVEHDR headers[kBlocks] = {};
    for (int i = 0; i < kBlocks; ++i)
    {
        buffers[i].assign(kBlockFrames * 2, 0);
        headers[i].lpData = reinterpret_cast<LPSTR>(buffers[i].data());
        headers[i].dwBufferLength = static_cast<DWORD>(kBlockFrames * 4);
        waveOutPrepareHeader(out, &headers[i], sizeof(WAVEHDR));
        headers[i].dwFlags |= WHDR_DONE;  // (free to fill)
    }
    for (;;)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stop)
                break;
            for (int i = 0; i < kBlocks; ++i)
                if (headers[i].dwFlags & WHDR_DONE)
                {
                    m_mixer.Mix(buffers[i].data(), kBlockFrames);
                    headers[i].dwFlags &= ~WHDR_DONE;
                    waveOutWrite(out, &headers[i], sizeof(WAVEHDR));
                }
        }
        WaitForSingleObject(done, 50);
    }
    waveOutReset(out);
    for (auto& header : headers)
        waveOutUnprepareHeader(out, &header, sizeof(WAVEHDR));
    waveOutClose(out);
    CloseHandle(done);
}
