#pragma once

// Quests (step 7.1): the host's game decides every quest's progress. The host shares what changed (and everything
// when a friend joins, and a few quests again each second); a friend's game is set to match directly, with every
// quest script detached while in co-op so none of them changes the friend's world.

#include "coop/coop.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace game
{
    class Quests
    {
    public:
        void Update(coop::Coop& coop, uint32_t now);  // every frame
        void Stop();  // co-op ended or the game is closing: quest scripts put back

    private:
        void Host(coop::Coop& coop, uint32_t now);
        void Friend(coop::Coop& coop, uint32_t now);
        std::vector<std::string> m_loadOrder;  // the mods' file names, in load order (for which quests are the game's)
        const std::vector<std::string>* m_questMods = nullptr;  // the host's quest mods (Coop's, set each Update)
        std::vector<std::pair<uint32_t, uint32_t>> m_prints;  // host: each quest's fingerprint at the last look
        uint32_t m_joins = 0;           // host: friends joined, as of the last full send
        size_t m_resendAt = 0;          // host: where the next few quests sent again start
        uint32_t m_nextLook = 0;
        void Enables(coop::Coop& coop, uint32_t now, bool host);
        std::map<uint32_t, bool> m_switched;  // host: refs our scripts switched on or off this session, as now
        uint32_t m_nextSwitchedResend = 0;
        uint32_t m_enablesSent = 0, m_enablesSet = 0;
        std::vector<std::pair<uint32_t, void*>> m_detached;  // friend: quest scripts taken off
        bool m_detachedAll = false;
        uint32_t m_quietUntil = 0;      // friend: no messages for the first sync after joining
        // Logged now and then (CLAUDE.md: measure).
        double m_lookMs = 0;
        uint32_t m_looks = 0, m_sent = 0, m_set = 0, m_nextLog = 0;
    };
}
