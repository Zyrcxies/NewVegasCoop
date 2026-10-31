// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#include "game/steam.h"

#include "game/addresses.h"
#include "log.h"
#include "net/protocol.h"

#include <cstring>
#include <deque>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <windows.h>

namespace game
{
    namespace slot = steamslots;

    namespace
    {
        void* g_client = nullptr;
        void* g_user = nullptr;
        void* g_voiceUser = nullptr;
        void* g_friends = nullptr;
        void* g_networking = nullptr;
        bool g_tried = false;
        bool g_ready = false;

        template <typename Fn>
        Fn Slot(void* iface, int index)
        {
            return reinterpret_cast<Fn>((*reinterpret_cast<void***>(iface))[index]);
        }

        class CallbackBase
        {
        public:
            virtual void Run(void* param) = 0;
            virtual void Run(void* param, bool ioFailure, uint64_t call) = 0;
            virtual int GetCallbackSizeBytes() = 0;

        protected:
            uint8_t m_flags = 0;
            int m_id = 0;
        };

        std::mutex g_queueMutex;
        std::deque<net::SteamId> g_requests;
        std::deque<std::pair<net::SteamId, uint8_t>> g_fails;

        class SessionRequest : public CallbackBase
        {
        public:
            void Run(void* param) override
            {
                net::SteamId who;
                std::memcpy(&who, param, sizeof(who));
                std::lock_guard lock(g_queueMutex);
                if (g_requests.size() < 64)
                    g_requests.push_back(who);
            }
            void Run(void* param, bool, uint64_t) override { Run(param); }
            int GetCallbackSizeBytes() override { return 8; }
        } g_sessionRequest;

        class ConnectFail : public CallbackBase
        {
        public:
            void Run(void* param) override
            {
                net::SteamId who;
                std::memcpy(&who, param, sizeof(who));
                uint8_t reason = static_cast<const uint8_t*>(param)[8];
                std::lock_guard lock(g_queueMutex);
                if (g_fails.size() < 64)
                    g_fails.push_back({who, reason});
            }
            void Run(void* param, bool, uint64_t) override { Run(param); }
            int GetCallbackSizeBytes() override { return 16; }
        } g_connectFail;

        class GameSteam : public net::SteamApi
        {
        public:
            net::SteamId Me() override
            {
                net::SteamId id = 0;
                Slot<void*(__thiscall*)(void*, net::SteamId*)>(g_user, slot::kUserSteamId)(g_user, &id);
                return id;
            }

            bool Send(net::SteamId to, const uint8_t* data, uint32_t size, bool reliable, int channel) override
            {
                return Slot<bool(__thiscall*)(void*, net::SteamId, const void*, uint32_t, int, int)>(g_networking,
                    slot::kSend)(g_networking, to, data, size, reliable ? 2 : 0, channel);
            }

            bool Read(net::Bytes& out, net::SteamId& from, int channel) override
            {
                uint32_t size = 0;
                if (!Slot<bool(__thiscall*)(void*, uint32_t*, int)>(g_networking, slot::kAvailable)(g_networking,
                    &size, channel))
                    return false;
                out.resize(size ? (size < 1024 * 1024 ? size : 1024 * 1024) : 1);
                uint32_t got = 0;
                if (!Slot<bool(__thiscall*)(void*, void*, uint32_t, uint32_t*, net::SteamId*, int)>(g_networking,
                    slot::kRead)(g_networking, out.data(), static_cast<uint32_t>(out.size()), &got, &from, channel))
                    return false;
                out.resize(got < out.size() ? got : out.size());
                return true;
            }

            void Accept(net::SteamId who) override
            {
                Slot<bool(__thiscall*)(void*, net::SteamId)>(g_networking, slot::kAccept)(g_networking, who);
            }

            bool IsFriend(net::SteamId who) override
            {
                return Slot<int(__thiscall*)(void*, net::SteamId)>(g_friends, slot::kRelationship)(g_friends, who) == 3;
            }

            bool TakeSessionRequest(net::SteamId& who) override
            {
                std::lock_guard lock(g_queueMutex);
                if (g_requests.empty())
                    return false;
                who = g_requests.front();
                g_requests.pop_front();
                return true;
            }

            bool TakeConnectFail(net::SteamId& who, uint8_t& reason) override
            {
                std::lock_guard lock(g_queueMutex);
                if (g_fails.empty())
                    return false;
                who = g_fails.front().first;
                reason = g_fails.front().second;
                g_fails.pop_front();
                return true;
            }
        } g_steam;

        const char* FriendName(net::SteamId id)
        {
            return Slot<const char*(__thiscall*)(void*, net::SteamId)>(g_friends, slot::kFriendName)(g_friends, id);
        }
    }

    bool SteamReady()
    {
        if (g_tried)
            return g_ready;
        g_tried = true;
        HMODULE dll = GetModuleHandleA("steam_api.dll");
        auto steamClient = dll ? reinterpret_cast<void*(__cdecl*)()>(GetProcAddress(dll, "SteamClient")) : nullptr;
        auto steamUser = dll ? reinterpret_cast<int(__cdecl*)()>(GetProcAddress(dll, "SteamAPI_GetHSteamUser")) : nullptr;
        auto steamPipe = dll ? reinterpret_cast<int(__cdecl*)()>(GetProcAddress(dll, "SteamAPI_GetHSteamPipe")) : nullptr;
        auto registerCallback = dll ? reinterpret_cast<void(__cdecl*)(void*, int)>(GetProcAddress(dll,
            "SteamAPI_RegisterCallback")) : nullptr;
        if (!steamClient || !steamUser || !steamPipe || !registerCallback)
        {
            LOG_WARN("Steam: the game's steam_api.dll isn't there (or isn't Steam's); Steam joining is off");
            return false;
        }
        g_client = steamClient();
        int user = steamUser(), pipe = steamPipe();
        if (!g_client || !user || !pipe)
        {
            LOG_WARN("Steam: not running or not logged in; Steam joining is off");
            return false;
        }
        using GetInterface = void*(__thiscall*)(void*, int, int, const char*);
        g_user = Slot<GetInterface>(g_client, slot::kClientUser)(g_client, user, pipe, slot::kUserVersion);
        g_friends = Slot<GetInterface>(g_client, slot::kClientFriends)(g_client, user, pipe, slot::kFriendsVersion);
        g_networking = Slot<GetInterface>(g_client, slot::kClientNetworking)(g_client, user, pipe,
            slot::kNetworkingVersion);
        if (!g_user || !g_friends || !g_networking)
        {
            LOG_WARN("Steam: the Steam client didn't hand out %s, %s or %s; Steam joining is off", slot::kUserVersion,
                slot::kFriendsVersion, slot::kNetworkingVersion);
            return false;
        }
        g_voiceUser = Slot<GetInterface>(g_client, slot::kClientUser)(g_client, user, pipe, slot::kVoiceUserVersion);
        if (!g_voiceUser)
            LOG_WARN("Steam: the Steam client didn't hand out %s; no voice chat", slot::kVoiceUserVersion);
        Slot<bool(__thiscall*)(void*, bool)>(g_networking, slot::kAllowRelay)(g_networking, true);
        registerCallback(&g_sessionRequest, slot::kSessionRequest);
        registerCallback(&g_connectFail, slot::kConnectFail);
        g_ready = true;
        int friends = Slot<int(__thiscall*)(void*, int)>(g_friends, slot::kFriendCount)(g_friends, 4);
        LOG_INFO("Steam: ready as \"%s\" (%s; %d friends)", SteamName().c_str(),
            net::SteamSession::Name(g_steam.Me()).c_str(), friends);
        return true;
    }

    net::SteamApi& Steam() { return g_steam; }

    namespace
    {
        struct VoiceRecorder
        {
            std::mutex mutex;
            std::condition_variable wake;
            bool want = false;
            bool quit = false;
            std::deque<std::vector<uint8_t>> recorded;
            bool running = false;
        } g_recorder;
        constexpr size_t kMaxRecorded = 16;
        constexpr uint32_t kTailMs = 1000;
        constexpr uint32_t kRecordLookMs = 20;

        bool TakeRecorded(std::vector<uint8_t>& out)
        {
            uint32_t available = 0;
            if (Slot<int(__thiscall*)(void*, uint32_t*, uint32_t*, uint32_t)>(g_voiceUser, slot::kVoiceAvailable)(
                    g_voiceUser, &available, nullptr, 0) != 0 || available == 0)
                return false;
            out.resize(available < 8192 ? available : 8192);
            uint32_t written = 0;
            int result = Slot<int(__thiscall*)(void*, bool, void*, uint32_t, uint32_t*, bool, void*, uint32_t,
                uint32_t*, uint32_t)>(g_voiceUser, slot::kVoiceGet)(g_voiceUser, true, out.data(),
                static_cast<uint32_t>(out.size()), &written, false, nullptr, 0, nullptr, 0);
            if (result != 0 || written == 0 || written > out.size())
                return false;
            out.resize(written);
            return true;
        }

        void RecordVoice()
        {
            bool on = false;
            uint32_t tailUntil = 0;
            std::vector<uint8_t> chunk;
            for (;;)
            {
                bool tail = tailUntil && static_cast<int32_t>(tailUntil - GetTickCount()) > 0;
                bool want;
                {
                    std::unique_lock<std::mutex> lock(g_recorder.mutex);
                    auto asked = [&] { return g_recorder.quit || g_recorder.want != on; };
                    if (on || tail)
                        g_recorder.wake.wait_for(lock, std::chrono::milliseconds(kRecordLookMs), asked);
                    else
                        g_recorder.wake.wait(lock, asked);
                    if (g_recorder.quit)
                        break;
                    want = g_recorder.want;
                }
                if (want != on)
                {
                    LARGE_INTEGER start, end, freq;
                    QueryPerformanceCounter(&start);
                    Slot<void(__thiscall*)(void*)>(g_voiceUser, want ? slot::kVoiceStart : slot::kVoiceStop)(
                        g_voiceUser);
                    QueryPerformanceCounter(&end);
                    QueryPerformanceFrequency(&freq);
                    LOG_DEBUG("Voice: Steam took %.1f ms to %s recording (on our recording thread)",
                        1000.0 * static_cast<double>(end.QuadPart - start.QuadPart) / static_cast<double>(freq.QuadPart),
                        want ? "start" : "stop");
                    on = want;
                    tailUntil = on ? 0 : GetTickCount() + kTailMs;
                }
                for (int i = 0; i < 4 && TakeRecorded(chunk); ++i)
                {
                    std::lock_guard<std::mutex> lock(g_recorder.mutex);
                    if (g_recorder.recorded.size() >= kMaxRecorded)
                        g_recorder.recorded.pop_front();
                    g_recorder.recorded.push_back(std::move(chunk));
                }
            }
            if (on)
                Slot<void(__thiscall*)(void*)>(g_voiceUser, slot::kVoiceStop)(g_voiceUser);
            std::lock_guard<std::mutex> lock(g_recorder.mutex);
            g_recorder.running = false;
            g_recorder.wake.notify_all();
        }
    }

    bool VoiceRecording(bool on)
    {
        if (!SteamReady() || !g_voiceUser)
            return false;
        std::lock_guard<std::mutex> lock(g_recorder.mutex);
        if (g_recorder.quit)
            return false;
        if (on && !g_recorder.running)
        {
            try
            {
                std::thread(RecordVoice).detach();
                g_recorder.running = true;
            }
            catch (...)
            {
                LOG_WARN("Voice: couldn't start recording (no thread)");
                return false;
            }
        }
        if (g_recorder.want != on)
        {
            g_recorder.want = on;
            g_recorder.wake.notify_all();
        }
        return true;
    }

    bool TakeVoice(std::vector<uint8_t>& out)
    {
        out.clear();
        std::lock_guard<std::mutex> lock(g_recorder.mutex);
        if (g_recorder.recorded.empty())
            return false;
        out = std::move(g_recorder.recorded.front());
        g_recorder.recorded.pop_front();
        return true;
    }

    void StopVoice()
    {
        std::unique_lock<std::mutex> lock(g_recorder.mutex);
        g_recorder.quit = true;
        g_recorder.wake.notify_all();
        g_recorder.wake.wait_for(lock, std::chrono::milliseconds(500), [] { return !g_recorder.running; });
    }

    bool DecompressVoice(const std::vector<uint8_t>& in, uint32_t rate, std::vector<int16_t>& out)
    {
        out.clear();
        if (!g_voiceUser || in.empty())
            return false;
        static std::vector<int16_t> s_buffer;
        s_buffer.resize(rate);
        std::vector<int16_t>& buffer = s_buffer;
        uint32_t written = 0;
        int result = Slot<int(__thiscall*)(void*, const void*, uint32_t, void*, uint32_t, uint32_t*, uint32_t)>(
            g_voiceUser, slot::kVoiceDecompress)(g_voiceUser, in.data(), static_cast<uint32_t>(in.size()),
            buffer.data(), static_cast<uint32_t>(buffer.size() * 2), &written, rate);
        if (result != 0 || written > buffer.size() * 2)
            return false;
        out.assign(buffer.begin(), buffer.begin() + written / 2);
        return !out.empty();
    }

    std::string SteamName()
    {
        if (!SteamReady())
            return "";
        const char* name = Slot<const char*(__thiscall*)(void*)>(g_friends, slot::kPersonaName)(g_friends);
        return name ? name : "";
    }

    void SetHostingOnSteam(bool hosting, const std::string& mode)
    {
        if (!SteamReady())
            return;
        if (hosting)
        {
            std::string value = "host " + std::to_string(net::kProtocolVersion);
            Slot<bool(__thiscall*)(void*, const char*, const char*)>(g_friends, slot::kSetPresence)(g_friends,
                "nvcoop", value.c_str());
            Slot<bool(__thiscall*)(void*, const char*, const char*)>(g_friends, slot::kSetPresence)(g_friends,
                "status", "Hosting New Vegas Co-op");
            Slot<bool(__thiscall*)(void*, const char*, const char*)>(g_friends, slot::kSetPresence)(g_friends,
                "nvcoopmode", mode.c_str());
        }
        else
            Slot<void(__thiscall*)(void*)>(g_friends, slot::kClearPresence)(g_friends);
        LOG_INFO("Steam: %s", hosting ? "friends' games can see we're hosting" : "not shown as hosting");
    }

    std::vector<SteamFriend> HostingFriends()
    {
        std::vector<SteamFriend> hosting;
        if (!SteamReady())
            return hosting;
        const std::string wanted = "host " + std::to_string(net::kProtocolVersion);
        int count = Slot<int(__thiscall*)(void*, int)>(g_friends, slot::kFriendCount)(g_friends, 4);
        int inGame = 0, otherVersion = 0;
        for (int i = 0; i < count && i < 2000; ++i)
        {
            net::SteamId id = 0;
            Slot<void*(__thiscall*)(void*, net::SteamId*, int, int)>(g_friends, slot::kFriendByIndex)(g_friends, &id,
                i, 4);
            uint8_t game[24] = {};
            if (!id || !Slot<bool(__thiscall*)(void*, net::SteamId, void*)>(g_friends, slot::kFriendGame)(g_friends,
                id, game))
                continue;
            ++inGame;
            const char* presence = Slot<const char*(__thiscall*)(void*, net::SteamId, const char*)>(g_friends,
                slot::kFriendPresence)(g_friends, id, "nvcoop");
            if (!presence || std::strncmp(presence, "host ", 5) != 0)
                continue;
            if (wanted != presence)
            {
                ++otherVersion;
                continue;
            }
            const char* name = FriendName(id);
            const char* mode = Slot<const char*(__thiscall*)(void*, net::SteamId, const char*)>(g_friends,
                slot::kFriendPresence)(g_friends, id, "nvcoopmode");
            hosting.push_back({id, name && *name ? name : net::SteamSession::Name(id),
                mode ? std::string(mode).substr(0, 64) : std::string()});
        }
        LOG_INFO("Steam: %d friends in a game, %zu hosting co-op (%d more on another version of the mod)", inGame,
            hosting.size(), otherVersion);
        return hosting;
    }
}
