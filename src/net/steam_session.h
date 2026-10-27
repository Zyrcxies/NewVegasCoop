#pragma once

// Co-op over Steam's peer-to-peer networking (step 8.5): no open ports or IP addresses; Steam finds the way (or
// relays). Only the host's Steam friends get in. Built on SteamApi, so the plugin gives it the game's Steam and
// tests give it a pretend one. Game-independent: tested in tests/.
//
// Steam's P2P has no connections, only packets between Steam IDs, so this adds them: a friend says "connect" until
// the host answers "accepted"; both say "still here" every second (the answer gives the round trip); 60 s without
// a word (as long as the world waits for a load: our code doesn't run during loading screens, so neither do these),
// or "bye", ends it. At most kMaxPacketsPerPoll are read a frame, so a flood can't hold the game up. Everything goes on one channel, so a "bye" can't overtake the messages before it (it
// did on two: the friend's playthrough test, 2026-10-16, lost the host's "mods match"); the first byte says which
// (0: one of co-op's messages). Sessions aren't closed by hand: Steam ends idle ones (closing raced with the next
// connect). Call from one thread (the game's): Steam's networking is asked from Poll.

#include "net/transport.h"

#include <cstdint>
#include <deque>
#include <map>
#include <string>

namespace net
{
    using SteamId = uint64_t;

    // The parts of Steam this needs.
    class SteamApi
    {
    public:
        virtual ~SteamApi() = default;
        virtual SteamId Me() = 0;
        virtual bool Send(SteamId to, const uint8_t* data, uint32_t size, bool reliable, int channel) = 0;
        virtual bool Read(Bytes& out, SteamId& from, int channel) = 0;  // the next packet on a channel, if any
        virtual void Accept(SteamId who) = 0;   // let their packets through
        virtual bool IsFriend(SteamId who) = 0;
        virtual bool TakeSessionRequest(SteamId& who) = 0;  // someone we don't talk to yet wants to
        // Steam couldn't reach them; reason: Steam's (2: we don't own the game, 4: they didn't answer).
        virtual bool TakeConnectFail(SteamId& who, uint8_t& reason) = 0;
    };

    class SteamSession : public Transport
    {
    public:
        static constexpr uint32_t kUnreliableMax = 1200;  // bigger is sent reliably (Steam's limit)
        static constexpr int kMaxPacketsPerPoll = 256;    // the rest wait in Steam for the next frame

        explicit SteamSession(SteamApi& steam) : m_steam(steam) {}
        ~SteamSession() override { Stop(); }

        bool StartHost(size_t maxPeers);  // Steam friends who connect become peers 1, 2...
        bool StartClient(SteamId host);   // the host is peer 1
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
        SteamId m_host = 0;              // friend: who we join
        bool m_connected = false;        // friend: the host answered
        uint32_t m_connectSince = 0, m_nextConnect = 0, m_nextKeepAlive = 0;
        PeerId m_nextPeer = 1;
        std::map<PeerId, Peer> m_peers;
        std::deque<Event> m_events;
        uint32_t m_silentMs = 60000, m_connectMs = 20000;
        uint32_t m_lastPump = 0;
        bool m_looked = false;  // this round of polls has looked at Steam already
    };
}
