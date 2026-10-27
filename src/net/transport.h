#pragma once

// What carries co-op's messages between games: the IP connection (ENet, net/session.h: labs, the fake friend and
// tests) or Steam's peer-to-peer networking (net/steam_session.h, step 8.5). Game-independent.

#include "net/protocol.h"

#include <cstdint>
#include <string>

namespace net
{
    using PeerId = uint32_t;

    struct Event
    {
        enum class Kind { Connected, Disconnected, Received } kind;
        PeerId peer = 0;
        Bytes data;        // Received only
        std::string from;  // Connected only: who (an IP address, or "steam:<id>")
    };

    class Transport
    {
    public:
        virtual ~Transport() = default;
        virtual void Send(PeerId peer, Bytes data, bool reliable = true) = 0;
        virtual void Disconnect(PeerId peer) = 0;  // after what was sent to them
        virtual bool Poll(Event& out) = 0;         // the next event, if any
        virtual void Stop() = 0;
        virtual bool Running() const = 0;
        virtual uint32_t RoundTripMs(PeerId peer) const = 0;
    };
}
