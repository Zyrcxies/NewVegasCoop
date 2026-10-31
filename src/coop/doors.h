// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include "net/protocol.h"

#include <cstdint>
#include <map>
#include <vector>

namespace coop
{
    constexpr uint32_t kDoorUseWaitMs = 2000;
    constexpr size_t kMaxKnownDoors = 512;

    struct LocalDoor
    {
        net::DoorState state;
        bool nearMe = false;
    };

    class FriendDoors
    {
    public:
        void Heard(const net::DoorState& host);
        std::vector<net::DoorState> Observe(uint32_t now, const std::vector<LocalDoor>& local,
            std::vector<net::DoorState>& uses);
        void Clear() { m_doors.clear(); }
        size_t Known() const { return m_doors.size(); }

    private:
        struct Door
        {
            net::DoorState host, last;
            bool heard = false, seen = false;
            uint32_t waitUntil = 0;
        };
        std::map<uint32_t, Door> m_doors;
    };
}
