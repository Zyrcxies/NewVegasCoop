// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

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
        Bytes data;
        std::string from;
    };

    class Transport
    {
    public:
        virtual ~Transport() = default;
        virtual void Send(PeerId peer, Bytes data, bool reliable = true) = 0;
        virtual void Disconnect(PeerId peer) = 0;
        virtual bool Poll(Event& out) = 0;
        virtual void Stop() = 0;
        virtual bool Running() const = 0;
        virtual uint32_t RoundTripMs(PeerId peer) const = 0;
    };
}
