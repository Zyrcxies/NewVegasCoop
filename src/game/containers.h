#pragma once

// Containers and bodies (step 6.2): what they hold is the host's. A friend's takes and puts are done in the host's
// world (or given back if someone got there first); the container a friend has open is kept to the host's.

#include "coop/coop.h"

#include <cstdint>

namespace game
{
    class Npcs;

    class Containers
    {
    public:
        void Update(coop::Coop& coop, uint32_t now, const Npcs& npcs);  // every frame

    private:
        void HostApply(coop::Coop& coop);
        void FriendLook(coop::Coop& coop, const Npcs& npcs);
        uint32_t m_nextLook = 0;
        // Logged now and then (CLAUDE.md: measure).
        double m_readMs = 0;
        uint32_t m_reads = 0, m_changes = 0, m_refunds = 0, m_fixes = 0, m_nextLog = 0;
    };
}
