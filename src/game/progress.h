#pragma once

// Shared XP, karma and faction reputation (step 7.2). XP and karma: what any player gains, everyone gains (the game's
// own way, with its sound and messages). Reputation is the host's world: friends' fame and infamy are set to it.

#include "coop/coop.h"
#include "coop/progress.h"

#include <cstdint>
#include <map>
#include <utility>

namespace game
{
    class Progress
    {
    public:
        void Update(coop::Coop& coop, uint32_t now);  // every frame
        void Forget() { m_watcher.Forget(); }         // a save is loading
        // Our player just gained this in a way that mustn't be shared as usual (a conversation's line: step 7.4).
        void Expect(int32_t xp, int32_t karma) { m_watcher.Expect(xp, karma); }

    private:
        void Reputations(coop::Coop& coop, uint32_t now, bool host);
        coop::ProgressWatcher m_watcher;
        uint32_t m_nextLook = 0, m_nextReputationLook = 0, m_nextReputationResend = 0;
        std::map<uint32_t, std::pair<float, float>> m_sentReputations;  // host: as last sent, by form
        uint32_t m_joins = 0;
        uint32_t m_quietUntil = 0;  // friend: no messages for the first reputations after joining
        bool m_wasInGame = false;
        // Logged now and then (CLAUDE.md: measure).
        int32_t m_sharedXp = 0, m_gotXp = 0;
        uint32_t m_reputationsSet = 0, m_nextLog = 0;
    };
}
