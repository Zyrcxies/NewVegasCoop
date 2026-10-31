// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include "net/fake_lag.h"
#include "net/protocol.h"
#include "net/transport.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct _ENetHost;

namespace net
{
    bool ParseHostAddress(const std::string& text, std::string& host, uint16_t& port);
    std::vector<std::string> LocalAddresses();

    class Session : public Transport
    {
    public:
        Session();
        ~Session();
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        bool StartHost(uint16_t port, size_t maxPeers);
        bool StartClient(const std::string& address, uint16_t port);
        void SetFakeLag(const FakeLag& lag) { m_lag = lag; }
        void Stop() override;
        bool Running() const override { return m_thread.joinable(); }

        void Send(PeerId peer, Bytes data, bool reliable = true) override;
        void Disconnect(PeerId peer) override;
        bool Poll(Event& out) override;
        uint32_t RoundTripMs(PeerId peer) const override;

    private:
        struct Command
        {
            enum class Kind { Send, Disconnect } kind;
            PeerId peer;
            Bytes data;
            bool reliable;
        };

        void Run();
        void Push(Event e);

        _ENetHost* m_host = nullptr;
        bool m_isHost = false;
        FakeLag m_lag;
        std::thread m_thread;
        std::atomic<bool> m_stop{false};

        std::mutex m_commandsMutex;
        std::deque<Command> m_commands;

        std::mutex m_eventsMutex;
        std::deque<Event> m_events;
        size_t m_eventBytes = 0;
        bool m_warnedFull = false;

        mutable std::mutex m_rttMutex;
        std::unordered_map<PeerId, uint32_t> m_rtt;
    };
}
