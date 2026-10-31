// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#include "net/steam_session.h"

#include "log.h"

#include <windows.h>

namespace net
{
    namespace
    {
        constexpr uint32_t kMagic = 0x4F43564E;
        constexpr int kChannel = 0;
        constexpr uint8_t kMessage = 0, kConnect = 'C', kAccepted = 'A', kStillHere = 'K', kEcho = 'E', kBye = 'B';
        constexpr uint32_t kConnectEveryMs = 1000, kKeepAliveEveryMs = 1000;
        constexpr uint32_t kPausedMs = 2000;

        bool Due(uint32_t now, uint32_t at) { return static_cast<int32_t>(now - at) >= 0; }
    }

    bool SteamSession::StartHost(size_t maxPeers)
    {
        Stop();
        m_running = true;
        m_isHost = true;
        m_maxPeers = maxPeers;
        m_nextPeer = 1;
        LOG_INFO("Steam: hosting as %s", Name(m_steam.Me()).c_str());
        return true;
    }

    bool SteamSession::StartClient(SteamId host)
    {
        Stop();
        if (!host)
            return false;
        m_running = true;
        m_isHost = false;
        m_host = host;
        m_connected = false;
        m_connectSince = GetTickCount();
        m_nextConnect = m_connectSince;
        LOG_INFO("Steam: connecting to %s", Name(host).c_str());
        return true;
    }

    PeerId SteamSession::PeerOf(SteamId id) const
    {
        for (const auto& [peer, p] : m_peers)
            if (p.id == id)
                return peer;
        return 0;
    }

    void SteamSession::SendControl(SteamId to, uint8_t type, uint32_t value, bool reliable)
    {
        uint8_t packet[5] = {type, static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
            static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)};
        m_steam.Send(to, packet, sizeof(packet), reliable, kChannel);
    }

    void SteamSession::Send(PeerId peer, Bytes data, bool reliable)
    {
        auto it = m_peers.find(peer);
        if (it == m_peers.end() || data.empty() || data.size() > kMaxMessageSize)
            return;
        data.insert(data.begin(), kMessage);
        m_steam.Send(it->second.id, data.data(), static_cast<uint32_t>(data.size()),
            reliable || data.size() > kUnreliableMax, kChannel);
    }

    void SteamSession::Disconnect(PeerId peer)
    {
        auto it = m_peers.find(peer);
        if (it == m_peers.end())
            return;
        SendControl(it->second.id, kBye, 0, true);
        Drop(peer, true);
    }

    void SteamSession::Drop(PeerId peer, bool tell)
    {
        auto it = m_peers.find(peer);
        if (it == m_peers.end())
            return;
        m_peers.erase(it);
        if (tell)
            m_events.push_back({Event::Kind::Disconnected, peer, {}});
    }

    void SteamSession::Stop()
    {
        if (!m_running)
            return;
        for (const auto& [peer, p] : m_peers)
            SendControl(p.id, kBye, 0, true);
        m_peers.clear();
        m_events.clear();
        m_running = m_connected = false;
        m_host = 0;
        m_lastPump = 0;
        m_looked = false;
    }

    uint32_t SteamSession::RoundTripMs(PeerId peer) const
    {
        auto it = m_peers.find(peer);
        return it == m_peers.end() ? 0 : it->second.rtt;
    }

    void SteamSession::Control(SteamId from, const Bytes& packet, uint32_t now)
    {
        if (packet.size() != 5)
            return;
        uint8_t type = packet[0];
        uint32_t value = packet[1] | (packet[2] << 8) | (packet[3] << 16) | (uint32_t(packet[4]) << 24);
        PeerId peer = PeerOf(from);
        if (peer)
            m_peers[peer].lastHeard = now;
        switch (type)
        {
        case kConnect:
            if (!m_isHost || value != kMagic)
                return;
            if (!m_steam.IsFriend(from))
                return LOG_WARN("Steam: %s isn't a Steam friend; not let in", Name(from).c_str());
            if (!peer)
            {
                if (m_peers.size() >= m_maxPeers)
                    return SendControl(from, kBye, 0, true);
                peer = m_nextPeer++;
                m_peers[peer] = Peer{from, now};
                Event e{Event::Kind::Connected, peer, {}};
                e.from = Name(from);
                m_events.push_back(std::move(e));
                LOG_INFO("Steam: %s connected (connection %u)", Name(from).c_str(), peer);
            }
            SendControl(from, kAccepted, 0, true);
            return;
        case kAccepted:
            if (m_isHost || from != m_host || m_connected)
                return;
            m_connected = true;
            m_peers[1] = Peer{from, now};
            {
                Event e{Event::Kind::Connected, 1, {}};
                e.from = Name(from);
                m_events.push_back(std::move(e));
            }
            LOG_INFO("Steam: connected to %s", Name(from).c_str());
            return;
        case kStillHere:
            if (peer)
                SendControl(from, kEcho, value, false);
            return;
        case kEcho:
            if (peer)
                m_peers[peer].rtt = now - value;
            return;
        case kBye:
            if (peer)
            {
                LOG_INFO("Steam: %s left", Name(from).c_str());
                Drop(peer, true);
            }
            return;
        }
    }

    void SteamSession::Pump(uint32_t now)
    {
        if (m_lastPump && now - m_lastPump > kPausedMs)
        {
            for (auto& [peer, p] : m_peers)
                p.lastHeard = now;
            m_connectSince += now - m_lastPump;
        }
        m_lastPump = now;

        SteamId who;
        while (m_steam.TakeSessionRequest(who))
        {
            if (m_isHost ? m_steam.IsFriend(who) : who == m_host)
                m_steam.Accept(who);
            else
                LOG_WARN("Steam: %s tried to reach us; not a Steam friend (or not our host): ignored", Name(who).c_str());
        }
        uint8_t reason = 0;
        while (m_steam.TakeConnectFail(who, reason))
        {
            const char* why = reason == 2 ? "Steam says we don't own the game" : reason == 4 ?
                "they didn't answer" : "another reason";
            if (PeerId peer = PeerOf(who))
            {
                LOG_WARN("Steam: lost %s (%s, %u)", Name(who).c_str(), why, reason);
                Drop(peer, true);
            }
            else if (!m_isHost && who == m_host && !m_connected)
            {
                LOG_WARN("Steam: couldn't reach %s (%s, %u)", Name(who).c_str(), why, reason);
                m_events.push_back({Event::Kind::Disconnected, 1, {}});
                m_host = 0;
            }
            else
                LOG_INFO("Steam: %s isn't reachable (%s, %u)", Name(who).c_str(), why, reason);
        }
        Bytes packet;
        for (int packets = 0; packets < kMaxPacketsPerPoll && m_steam.Read(packet, who, kChannel); ++packets)
        {
            if (packet.empty())
                continue;
            if (packet[0] != kMessage)
            {
                Control(who, packet, now);
                continue;
            }
            PeerId peer = PeerOf(who);
            if (!peer || packet.size() < 2 || packet.size() > kMaxMessageSize + 1)
                continue;
            m_peers[peer].lastHeard = now;
            packet.erase(packet.begin());
            m_events.push_back({Event::Kind::Received, peer, std::move(packet)});
            packet = Bytes();
        }

        if (!m_isHost && m_host && !m_connected)
        {
            if (now - m_connectSince > m_connectMs)
            {
                LOG_WARN("Steam: %s didn't answer", Name(m_host).c_str());
                m_events.push_back({Event::Kind::Disconnected, 1, {}});
                m_host = 0;
            }
            else if (Due(now, m_nextConnect))
            {
                SendControl(m_host, kConnect, kMagic, true);
                m_nextConnect = now + kConnectEveryMs;
            }
        }

        bool keepAlive = Due(now, m_nextKeepAlive);
        if (keepAlive)
            m_nextKeepAlive = now + kKeepAliveEveryMs;
        std::vector<PeerId> silent;
        for (auto& [peer, p] : m_peers)
        {
            if (now - p.lastHeard > m_silentMs)
                silent.push_back(peer);
            else if (keepAlive)
                SendControl(p.id, kStillHere, now, false);
        }
        for (PeerId peer : silent)
        {
            LOG_WARN("Steam: nothing from %s for %u s", Name(m_peers[peer].id).c_str(), m_silentMs / 1000);
            Drop(peer, true);
        }
    }

    bool SteamSession::Poll(Event& out)
    {
        if (!m_running)
            return false;
        if (m_events.empty())
        {
            if (m_looked)
                return m_looked = false;
            Pump(GetTickCount());
            m_looked = true;
        }
        if (m_events.empty())
            return m_looked = false;
        out = std::move(m_events.front());
        m_events.pop_front();
        return true;
    }
}
