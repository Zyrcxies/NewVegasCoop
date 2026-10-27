#pragma once

// Companions are the party's (DESIGN). They live in the host's world; on the host's game this keeps track of who
// each follows: the host as in the base game, or a friend who told it to follow them (or hired it), through a copy
// of the game's own follow package aimed at that friend's character. Everyone hears who follows whom.

#include "coop/coop.h"

#include <cstdint>
#include <map>
#include <set>

class TESForm;

namespace game
{
    class StandIns;

    class Companions
    {
    public:
        // Host, every frame (looks once a second): friends' orders, keeping each following its player, and telling
        // everyone. Anyone else: lets go of what it had.
        void Update(coop::Coop& coop, const StandIns& standIns, uint32_t now);
        // Host: a friend's conversation line just ran here (one that hires a companion: it follows them).
        void FriendLineRan(uint8_t playerId, uint32_t now);
        void ReleaseAll();  // back to following the host (co-op over)

    private:
        void Follow(uint32_t ref, uint8_t playerId, coop::Coop& coop, const StandIns& standIns);
        void Release(uint32_t ref, const char* why);
        std::map<uint32_t, uint8_t> m_assigned;     // companion -> the friend it follows
        struct Package { TESForm* form = nullptr; uint32_t target = 0; };
        std::map<uint8_t, Package> m_packages;      // friend -> our follow package aimed at their character
        std::set<uint32_t> m_known;                 // companions at the last look
        uint8_t m_lineFrom = 0;
        uint32_t m_lineAt = 0, m_nextLook = 0;
    };
}
