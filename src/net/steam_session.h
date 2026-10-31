// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include "net/transport.h"

#include <cstdint>
#include <deque>
#include <map>
#include <string>

namespace net
{
    using SteamId = uint64_t;

    class SteamApi
    {
    public:
        virtual ~SteamApi() = default;
        virtual SteamId Me() = 0;
        virtual bool Send(SteamId to, const uint8_t* data, uint32_t size, bool reliable, int channel) = 0;
        virtual bool Read(Bytes& out, SteamId& from, int channel) = 0;
        virtual void Accept(SteamId who) = 0;
        virtual bool IsFriend(SteamId who) = 0;
        virtual bool TakeSessionRequest(SteamId& who) = 0;
        virtual bool TakeConnectFail(SteamId& who, uint8_t& reason) = 0;
    };

    class SteamSession : public Transport
    {
    public:
        static constexpr uint32_t kUnreliableMax = 1200;
        static constexpr int kMaxPacketsPerPoll = 256;

        explicit SteamSession(SteamApi& steam) : m_steam(steam) {}
        ~SteamSession() override { Stop(); }

        bool StartHost(size_t maxPeers);
        bool StartClient(SteamId host);
        void SetTimeouts(uint32_t silentMs, uint32_t connectMs) { m_silentMs = silentMs; m_connectMs = connectMs; }

        void Send(PeerId peer, Bytes data, bool reliable = true) override;
        void Disconnect(PeerId peer) override;
        bool Poll(Event& out) override;
        void Stop() override;
        bool Running() const override { return m_running; }
        uint32_t RoundTripMs(PeerId peer) const override;

        static std::string Name(SteamId id) { return "steam:" + std::to_string(id); }

    private:
        struct Peer
        {
            SteamId id = 0;
            uint32_t lastHeard = 0;
            uint32_t rtt = 0;
        };
        void Pump(uint32_t now);
        void Control(SteamId from, const Bytes& packet, uint32_t now);
        void SendControl(SteamId to, uint8_t type, uint32_t value, bool reliable);
        void Drop(PeerId peer, bool tell);
        PeerId PeerOf(SteamId id) const;

        SteamApi& m_steam;
        bool m_running = false;
        bool m_isHost = false;
        size_t m_maxPeers = 0;
        SteamId m_host = 0;
        bool m_connected = false;
        uint32_t m_connectSince = 0, m_nextConnect = 0, m_nextKeepAlive = 0;
        PeerId m_nextPeer = 1;
        std::map<PeerId, Peer> m_peers;
        std::deque<Event> m_events;
        uint32_t m_silentMs = 60000, m_connectMs = 20000;
        uint32_t m_lastPump = 0;
        bool m_looked = false;
    };
}
