#include "speech.h"

#include "log.h"

#include <windows.h>
#include <mmsystem.h>
#include <sapi.h>

#include <cmath>
#include <memory>
#include <vector>

#pragma comment(lib, "sapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winmm.lib")

namespace
{
    constexpr size_t kMaxWaiting = 6;     // lines waiting to be said
    constexpr size_t kMaxPlaying = 8;     // lines playing at once
    constexpr DWORD kRate = 22050;        // what the voices say into: 22 kHz, 16-bit, mono

    template <typename T>
    struct Com  // (releases a COM object)
    {
        T* p = nullptr;
        ~Com() { if (p) p->Release(); }
        T* operator->() const { return p; }
        T** operator&() { return &p; }
    };

    // A line playing: its own wave out (Windows mixes them), stereo for the side it's on.
    struct Playing
    {
        HWAVEOUT out = nullptr;
        WAVEHDR header = {};
        std::vector<int16_t> samples;
    };

    // Says `text` into memory as 16-bit mono samples.
    bool Render(ISpVoice* voice, ISpObjectToken* token, int pitch, int speed, const std::string& text,
        std::vector<int16_t>& out)
    {
        Com<IStream> memory;
        Com<ISpStream> stream;
        if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &memory)) ||
            FAILED(CoCreateInstance(CLSID_SpStream, nullptr, CLSCTX_ALL, IID_ISpStream, reinterpret_cast<void**>(&stream))))
            return false;
        WAVEFORMATEX format = {WAVE_FORMAT_PCM, 1, kRate, kRate * 2, 2, 16, 0};
        if (FAILED(stream->SetBaseStream(memory.p, SPDFID_WaveFormatEx, &format)) || FAILED(voice->SetOutput(stream.p, TRUE)))
            return false;
        if (token)
            voice->SetVoice(token);
        voice->SetRate(speed);
        // (The text is printable with no '<', '>' or '&' (coop::CleanChat): nothing in it reads as SAPI's markup.)
        std::wstring xml = L"<pitch absmiddle=\"" + std::to_wstring(pitch) + L"\">";
        for (char c : text)
            if (c != '<' && c != '>' && c != '&')
                xml.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
        xml += L"</pitch>";
        HRESULT spoke = voice->Speak(xml.c_str(), SPF_IS_XML, nullptr);
        voice->SetOutput(nullptr, TRUE);
        if (FAILED(spoke))
            return false;
        HGLOBAL global = nullptr;
        STATSTG stat = {};
        if (FAILED(GetHGlobalFromStream(memory.p, &global)) || FAILED(memory->Stat(&stat, STATFLAG_NONAME)))
            return false;
        size_t bytes = static_cast<size_t>(stat.cbSize.QuadPart);
        if (bytes > 30u * kRate * 2)
            bytes = 30u * kRate * 2;  // (30 s at most)
        const auto* data = static_cast<const int16_t*>(GlobalLock(global));
        if (!data)
            return false;
        out.assign(data, data + bytes / 2);
        GlobalUnlock(global);
        return !out.empty();
    }

    bool Play(const std::vector<int16_t>& mono, float volume, float pan, Playing& p)
    {
        // Equal-power panning.
        float angle = (pan + 1) * 0.785398f;
        float left = volume * std::cos(angle), right = volume * std::sin(angle);
        p.samples.resize(mono.size() * 2);
        for (size_t i = 0; i < mono.size(); ++i)
        {
            p.samples[i * 2] = static_cast<int16_t>(mono[i] * left);
            p.samples[i * 2 + 1] = static_cast<int16_t>(mono[i] * right);
        }
        WAVEFORMATEX format = {WAVE_FORMAT_PCM, 2, kRate, kRate * 4, 4, 16, 0};
        if (waveOutOpen(&p.out, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
            return false;
        p.header.lpData = reinterpret_cast<LPSTR>(p.samples.data());
        p.header.dwBufferLength = static_cast<DWORD>(p.samples.size() * 2);
        if (waveOutPrepareHeader(p.out, &p.header, sizeof(p.header)) != MMSYSERR_NOERROR ||
            waveOutWrite(p.out, &p.header, sizeof(p.header)) != MMSYSERR_NOERROR)
        {
            waveOutClose(p.out);
            p.out = nullptr;
            return false;
        }
        return true;
    }

    void Finish(Playing& p)
    {
        waveOutReset(p.out);
        waveOutUnprepareHeader(p.out, &p.header, sizeof(p.header));
        waveOutClose(p.out);
        p.out = nullptr;
    }
}

Speech::~Speech()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_wake.notify_all();
    if (m_thread.joinable())
        m_thread.detach();  // (the game is closing: its threads may be gone already)
}

void Speech::Say(const std::string& text, uint8_t who, const coop::VoiceStyle& style, float volume, float pan)
{
    if (text.empty() || volume <= 0)
        return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_thread.joinable())
            m_thread = std::thread(&Speech::Run, this);
        if (m_lines.size() >= kMaxWaiting)
            m_lines.pop_front();
        m_lines.push_back({text, who, style.Valid() ? style : coop::VoiceStyle{}, volume > 1 ? 1 : volume,
            pan < -1 ? -1 : pan > 1 ? 1 : pan});
    }
    m_wake.notify_one();
}

float Speech::Level(uint8_t who)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const Sounding& s : m_sounding)
    {
        if (s.who != who)
            continue;
        size_t at = static_cast<size_t>(GetTickCount() - s.start) * kRate / 1000;
        size_t window = kRate / 25;  // (40 ms)
        if (at >= s.samples.size())
            return 0;
        size_t end = at + window < s.samples.size() ? at + window : s.samples.size();
        float sum = 0;
        for (size_t i = at; i < end; ++i)
            sum += std::fabs(static_cast<float>(s.samples[i]));
        return sum / static_cast<float>(end - at);
    }
    return 0;
}

void Speech::Run()
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
        return LOG_ERROR("Speech: Windows' COM didn't start; chat lines aren't read out loud");
    {
        Com<ISpVoice> voice;
        Com<ISpObjectTokenCategory> category;
        std::vector<ISpObjectToken*> voices[2];  // male, female (Windows' voices say which they are)
        if (FAILED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, reinterpret_cast<void**>(&voice))))
        {
            LOG_ERROR("Speech: Windows' text-to-speech isn't there; chat lines aren't read out loud");
            CoUninitialize();
            return;
        }
        if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_ISpObjectTokenCategory,
                reinterpret_cast<void**>(&category))) &&
            SUCCEEDED(category->SetId(SPCAT_VOICES, FALSE)))
            for (int kind = 0; kind < 2; ++kind)
            {
                Com<IEnumSpObjectTokens> tokens;
                ULONG count = 0;
                if (SUCCEEDED(category->EnumTokens(kind ? L"Gender=Female" : L"Gender=Male", nullptr, &tokens)) &&
                    SUCCEEDED(tokens->GetCount(&count)))
                    for (ULONG i = 0; i < count && i < 16; ++i)
                    {
                        ISpObjectToken* token = nullptr;
                        if (SUCCEEDED(tokens->Item(i, &token)) && token)
                            voices[kind].push_back(token);
                    }
            }
        LOG_INFO("Speech: ready (Windows voices: %zu male, %zu female)", voices[0].size(), voices[1].size());

        std::vector<std::unique_ptr<Playing>> playing;
        for (;;)
        {
            Line line;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_wake.wait_for(lock, std::chrono::milliseconds(250), [&] { return m_stop || !m_lines.empty(); });
                if (m_stop)
                    break;
                bool have = !m_lines.empty();
                if (have)
                {
                    line = std::move(m_lines.front());
                    m_lines.pop_front();
                }
                lock.unlock();
                // Lines done playing are let go.
                for (size_t i = 0; i < playing.size();)
                    if (playing[i]->header.dwFlags & WHDR_DONE)
                    {
                        Finish(*playing[i]);
                        playing.erase(playing.begin() + i);
                    }
                    else
                        ++i;
                if (!have)
                    continue;
            }
            if (playing.size() >= kMaxPlaying)
                continue;  // (too much at once: this one isn't said)
            std::vector<int16_t> mono;
            // Their kind of voice (or the other kind, if this PC has none of theirs).
            const coop::VoiceStyle& style = line.style;
            const std::vector<ISpObjectToken*>& kind = !voices[style.voice].empty() ? voices[style.voice] : voices[!style.voice];
            ISpObjectToken* token = kind.empty() ? nullptr : kind[style.variant % kind.size()];
            auto p = std::make_unique<Playing>();
            bool said = Render(voice.p, token, style.pitch, style.speed, line.text, mono);
            if (said)
                coop::ApplyVoiceEffect(mono, kRate, static_cast<coop::VoiceEffect>(style.effect));
            if (said && Play(mono, line.volume, line.pan, *p))
            {
                playing.push_back(std::move(p));
                std::lock_guard<std::mutex> lock(m_mutex);
                // (Lines done a while ago are let go; one per speaker kept: the newest.)
                DWORD now = GetTickCount();
                for (size_t i = 0; i < m_sounding.size();)
                    if (m_sounding[i].who == line.who ||
                        now - m_sounding[i].start > m_sounding[i].samples.size() * 1000 / kRate + 500)
                        m_sounding.erase(m_sounding.begin() + i);
                    else
                        ++i;
                m_sounding.push_back({line.who, std::move(mono), now});
            }
            else
                LOG_WARN("Speech: a chat line couldn't be said");
        }
        for (auto& p : playing)
            Finish(*p);
        for (auto& kind : voices)
            for (ISpObjectToken* token : kind)
                token->Release();
    }
    CoUninitialize();
}
