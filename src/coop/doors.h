#pragma once

// Doors on a friend's game (step 6.1): the host's game decides whether each door is open, closed, locked or
// unlocked. Our own player using a door is sent to the host; any other difference is put back to the host's state.
// Game-independent: tested in tests/.

#include "net/protocol.h"

#include <cstdint>
#include <map>
#include <vector>

namespace coop
{
    constexpr uint32_t kDoorUseWaitMs = 2000;  // after our own use: how long the host has to agree before we give in
    constexpr size_t kMaxKnownDoors = 512;     // doors kept track of at once; more are ignored

    struct LocalDoor
    {
        net::DoorState state;
        bool nearMe = false;  // close enough that a change could be our player's doing (not an NPC copy's)
    };

    class FriendDoors
    {
    public:
        // A door's state in the host's game.
        void Heard(const net::DoorState& host);
        // Our game's doors near us, as they are now; call a few times a second. Returns the doors to set to the
        // host's state; `uses` gets our player's own changes, to send to the host.
        std::vector<net::DoorState> Observe(uint32_t now, const std::vector<LocalDoor>& local,
            std::vector<net::DoorState>& uses);
        void Clear() { m_doors.clear(); }
        size_t Known() const { return m_doors.size(); }

    private:
        struct Door
        {
            net::DoorState host, last;
            bool heard = false, seen = false;
            uint32_t waitUntil = 0;  // our own use not agreed yet (0: none)
        };
        std::map<uint32_t, Door> m_doors;
    };
}
