#pragma once

// Doors near the group (step 6.1): the host shares whether each is open, closed, locked or unlocked; a friend's
// game follows, and sends its own player's door uses to the host (coop::FriendDoors decides which is which).

#include "coop/coop.h"

#include <cstdint>
#include <vector>

namespace game
{
    class Doors
    {
    public:
        void Update(coop::Coop& coop, uint32_t now);  // every frame
        void Forget();  // a save is loading: its doors are going away

    private:
        std::vector<uint32_t> m_refs;  // the doors near us (ref ids), listed now and then
        const void* m_cell = nullptr;  // the cell we were in when listing them
        uint32_t m_nextList = 0, m_nextLook = 0;
        // Timing, logged now and then (CLAUDE.md: measure).
        double m_lookMs = 0, m_listMs = 0;
        uint32_t m_looks = 0, m_lists = 0, m_applied = 0, m_nextLog = 0;
    };
}
