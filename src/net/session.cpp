#include "net/session.h"

#include "log.h"

#include <enet/enet.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cstdlib>

namespace net
{
    bool ParseHostAddress(const std::string& text, std::string& host, uint16_t& port)
    {
        size_t first = text.find_first_not_of(" \t"), last = text.find_last_not_of(" \t");
        if (first == std::string::npos)
            return false;
        std::string t = text.substr(first, last - first + 1);
        std::string name = t;
        uint16_t given = port;
        if (size_t colon = t.find(':'); colon != std::string::npos)
        {
            name = t.substr(0, colon);
            std::string digits = t.substr(colon + 1);
            if (digits.empty() || digits.size() > 5 ||
                digits.find_first_not_of("0123456789") != std::string::npos)
                return false;
            long number = std::strtol(digits.c_str(), nullptr, 10);
            if (number < 1 || number > 65535)
                return false;
            given = static_cast<uint16_t>(number);
        }
        if (name.empty() || name.size() > 253 ||
            name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-") !=
                std::string::npos)
            return false;
        host = name;
        port = given;
        return true;
    }

    std::vector<std::string> LocalAddresses()
    {
        std::vector<std::string> out;
        char name[256] = {};
        if (gethostname(name, sizeof(name)) != 0)
            return out;
        addrinfo hints{};
        hints.ai_family = AF_INET;
        addrinfo* found = nullptr;
        if (getaddrinfo(name, nullptr, &hints, &found) != 0)
            return out;
        for (addrinfo* a = found; a; a = a->ai_next)
        {
            char text[INET_ADDRSTRLEN] = {};
            auto* in = reinterpret_cast<sockaddr_in*>(a->ai_addr);
            if (inet_ntop(AF_INET, &in->sin_addr, text, sizeof(text)) && std::string(text).rfind("127.", 0) != 0 &&
                std::find(out.begin(), out.end(), text) == out.end())
                out.push_back(text);
        }
        freeaddrinfo(found);
        return out;
    }

    namespace
    {
        enum Channel : uint8_t { kReliable = 0, kUnreliable = 1, kChannelCount = 2 };
        // Messages waiting for the game (it can stop taking them for a while: a loading screen). Past this,
        // received ones are dropped, so a flood can't eat memory (CLAUDE.md security).
        constexpr size_t kMaxWaitingBytes = 8 * 1024 * 1024;
        constexpr size_t kMaxWaitingEvents = 20000;

        // ENet's own limits (its defaults allow 32 MB packets and 32 MB waiting per peer): ours are far smaller.
        void LimitHost(ENetHost* host)
        {
            host->maximumPacketSize = kMaxMessageSize;
            host->maximumWaitingData = 1024 * 1024;
        }

        std::mutex g_enetMutex;
        int g_enetUsers = 0;

        bool EnetAcquire()
        {
            std::lock_guard lock(g_enetMutex);
            if (g_enetUsers == 0 && enet_initialize() != 0)
                return false;
            ++g_enetUsers;
            return true;
        }

        void EnetRelease()
        {
            std::lock_guard lock(g_enetMutex);
            if (--g_enetUsers == 0)
                enet_deinitialize();
        }

        PeerId IdOf(ENetPeer* p) { return static_cast<PeerId>(reinterpret_cast<uintptr_t>(p->data)); }
    }

    Session::Session() = default;
    Session::~Session() { Stop(); }

    bool Session::StartHost(uint16_t port, size_t maxPeers)
    {
        if (Running() || !EnetAcquire())
            return false;
        ENetAddress address{};
        address.host = ENET_HOST_ANY;
        address.port = port;
        m_host = enet_host_create(&address, maxPeers, kChannelCount, 0, 0);
        if (!m_host)
        {
            EnetRelease();
            return false;
        }
        LimitHost(m_host);
        m_isHost = true;
        m_stop = false;
        m_thread = std::thread(&Session::Run, this);
        return true;
    }

    bool Session::StartClient(const std::string& host, uint16_t port)
    {
        if (Running() || !EnetAcquire())
            return false;
        ENetAddress address{};
        address.port = port;
        if (enet_address_set_host(&address, host.c_str()) != 0)
        {
            LOG_ERROR("Couldn't find the host's address (not an IP address or a known name)");
            EnetRelease();
            return false;
        }
        m_host = enet_host_create(nullptr, 1, kChannelCount, 0, 0);
        if (m_host)
            LimitHost(m_host);
        ENetPeer* peer = m_host ? enet_host_connect(m_host, &address, kChannelCount, 0) : nullptr;
        if (!peer)
        {
            if (m_host)
                enet_host_destroy(m_host);
            m_host = nullptr;
            EnetRelease();
            return false;
        }
        peer->data = reinterpret_cast<void*>(uintptr_t{1});  // a client's only peer is the host: id 1
        m_isHost = false;
        m_stop = false;
        m_thread = std::thread(&Session::Run, this);
        return true;
    }

    void Session::Stop()
    {
        if (!Running())
            return;
        m_stop = true;
        m_thread.join();
        enet_host_destroy(m_host);
        m_host = nullptr;
        EnetRelease();
        m_commands.clear();
        m_events.clear();
        m_eventBytes = 0;
        m_warnedFull = false;
        m_rtt.clear();
    }

    void Session::Send(PeerId peer, Bytes data, bool reliable)
    {
        std::lock_guard lock(m_commandsMutex);
        m_commands.push_back({Command::Kind::Send, peer, std::move(data), reliable});
    }

    void Session::Disconnect(PeerId peer)
    {
        std::lock_guard lock(m_commandsMutex);
        m_commands.push_back({Command::Kind::Disconnect, peer, {}, true});
    }

    bool Session::Poll(Event& out)
    {
        std::lock_guard lock(m_eventsMutex);
        if (m_events.empty())
            return false;
        out = std::move(m_events.front());
        m_events.pop_front();
        m_eventBytes -= out.data.size();
        return true;
    }

    uint32_t Session::RoundTripMs(PeerId peer) const
    {
        std::lock_guard lock(m_rttMutex);
        auto it = m_rtt.find(peer);
        return it == m_rtt.end() ? 0 : it->second + m_lag.pingMs;
    }

    void Session::Push(Event e)
    {
        std::lock_guard lock(m_eventsMutex);
        // (Connects and disconnects always get through: they carry no data.)
        if (e.kind == Event::Kind::Received &&
            (m_eventBytes + e.data.size() > kMaxWaitingBytes || m_events.size() >= kMaxWaitingEvents))
        {
            if (!m_warnedFull)
                LOG_WARN("Too many messages waiting for the game (%zu, %zu bytes); dropping new ones",
                    m_events.size(), m_eventBytes);
            m_warnedFull = true;
            return;
        }
        m_eventBytes += e.data.size();
        m_events.push_back(std::move(e));
    }

    // Network thread: the only place ENet is used after Start.
    void Session::Run()
    {
        std::unordered_map<PeerId, ENetPeer*> peers;
        PeerId nextId = 1;
        DWORD lastRtt = 0;

        // With fake lag on, messages both ways wait in these first.
        using OutQueue = LagQueue<Command>;
        using InQueue = LagQueue<Event>;
        OutQueue outQueue(m_lag, GetTickCount());
        InQueue inQueue(m_lag, GetTickCount() + 1);
        if (m_lag.On())
            LOG_WARN("Fake lag on: ping %u ms, jitter %u ms, %u%% lost", m_lag.pingMs, m_lag.jitterMs,
                m_lag.lossPercent);
        auto send = [&](Command& c)
        {
            auto it = peers.find(c.peer);
            if (it == peers.end())
                return;
            if (c.kind == Command::Kind::Disconnect)
            {
                enet_peer_disconnect_later(it->second, 0);
                return;
            }
            ENetPacket* packet = enet_packet_create(c.data.data(), c.data.size(),
                c.reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
            if (enet_peer_send(it->second, c.reliable ? kReliable : kUnreliable, packet) != 0)
                enet_packet_destroy(packet);
        };
        auto push = [&](Event e, InQueue::Kind kind)
        {
            if (m_lag.On())
                inQueue.Push(GetTickCount64(), kind, std::move(e));
            else
                Push(std::move(e));
        };

        while (!m_stop)
        {
            std::deque<Command> commands;
            {
                std::lock_guard lock(m_commandsMutex);
                commands.swap(m_commands);
            }
            for (auto& c : commands)
            {
                if (!m_lag.On())
                    send(c);
                else
                    outQueue.Push(GetTickCount64(), c.kind == Command::Kind::Disconnect ? OutQueue::Kind::Barrier
                        : c.reliable ? OutQueue::Kind::Reliable : OutQueue::Kind::Unreliable, std::move(c));
            }
            for (Command c; outQueue.Pop(GetTickCount64(), c);)
                send(c);

            ENetEvent ev;
            int wait = 2;
            while (enet_host_service(m_host, &ev, wait) > 0)
            {
                wait = 0;
                switch (ev.type)
                {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    PeerId id = m_isHost ? ++nextId : IdOf(ev.peer);
                    ev.peer->data = reinterpret_cast<void*>(uintptr_t{id});
                    enet_peer_timeout(ev.peer, 0, 5000, 10000);
                    peers[id] = ev.peer;
                    Event connected{Event::Kind::Connected, id, {}};
                    char ip[64] = {};
                    enet_address_get_host_ip(&ev.peer->address, ip, sizeof(ip));
                    connected.from = ip;
                    push(std::move(connected), InQueue::Kind::Barrier);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    PeerId id = IdOf(ev.peer);
                    peers.erase(id);
                    {
                        std::lock_guard lock(m_rttMutex);
                        m_rtt.erase(id);
                    }
                    push({Event::Kind::Disconnected, id, {}}, InQueue::Kind::Barrier);
                    ev.peer->data = nullptr;
                    break;
                }
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    PeerId id = IdOf(ev.peer);
                    if (ev.packet->dataLength <= kMaxMessageSize && peers.contains(id))
                        push({Event::Kind::Received, id,
                              Bytes(ev.packet->data, ev.packet->data + ev.packet->dataLength)},
                             (ev.packet->flags & ENET_PACKET_FLAG_RELIABLE) ? InQueue::Kind::Reliable
                                                                            : InQueue::Kind::Unreliable);
                    enet_packet_destroy(ev.packet);
                    break;
                }
                default:
                    break;
                }
            }

            for (Event e; inQueue.Pop(GetTickCount64(), e);)
                Push(std::move(e));

            DWORD now = GetTickCount();
            if (now - lastRtt > 500)
            {
                lastRtt = now;
                std::lock_guard lock(m_rttMutex);
                for (auto& [id, p] : peers)
                    m_rtt[id] = p->roundTripTime;
            }
        }

        // Say goodbye so the other side notices right away instead of timing out.
        for (auto& [id, p] : peers)
            enet_peer_disconnect_now(p, 0);
        enet_host_flush(m_host);
    }
}
