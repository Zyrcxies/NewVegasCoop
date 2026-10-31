// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include "coop/coop.h"
#include "coop/progress.h"

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

class TESObjectREFR;

namespace game
{
    class Progress
    {
    public:
        void Update(coop::Coop& coop, uint32_t now);
        void Forget() { m_watcher.Forget(); m_reputationWatcher.Forget(); m_travelWatcher.Forget(); }
        void Expect(int32_t xp, int32_t karma) { m_watcher.Expect(xp, karma); }

    private:
        void Reputations(coop::Coop& coop, uint32_t now, bool host);
        void Travel(coop::Coop& coop, uint32_t now, bool host);
        coop::ProgressWatcher m_watcher;
        coop::ReputationWatcher m_reputationWatcher;
        coop::TravelWatcher m_travelWatcher;
        std::vector<TESObjectREFR*> m_mapMarkers;
        uint32_t m_nextTravelLook = 0, m_travelJoins = 0;
        uint32_t m_nextLook = 0, m_nextReputationLook = 0, m_nextReputationResend = 0;
        std::map<uint32_t, std::pair<float, float>> m_sentReputations;
        uint32_t m_joins = 0;
        uint32_t m_quietUntil = 0;
        bool m_wasInGame = false;
        int32_t m_sharedXp = 0, m_gotXp = 0;
        uint32_t m_reputationsSet = 0, m_reputationsShared = 0, m_nextLog = 0;
    };
}
