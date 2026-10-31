// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include "coop/coop.h"

#include <cstdint>
#include <map>
#include <vector>

namespace game
{
    class StandIns;

    class Mines
    {
    public:
        void Update(coop::Coop& coop, const StandIns& standIns, uint32_t now);
        void Forget();
        bool SparedFrom(int playerId, uint32_t now) const { return playerId == m_spared && now - m_sparedAt < 2000; }
        bool BlewNear(float x, float y, float z, uint32_t now) const;

    private:
        bool Apply(uint32_t id, uint8_t state, bool fresh, uint8_t playerId, uint32_t now);
        struct Blast { float x = 0, y = 0, z = 0; uint32_t at = 0; };
        struct Pending { uint32_t id = 0, since = 0; };

        std::vector<uint32_t> m_refs;
        std::map<uint32_t, uint8_t> m_seen;
        std::map<uint32_t, uint8_t> m_known;
        const void* m_cell = nullptr;
        uint32_t m_nextList = 0, m_nextLook = 0;
        uint32_t m_reported = 0, m_applied = 0, m_nextLog = 0;
        int m_spared = -1;
        uint32_t m_sparedAt = 0;
        Blast m_blasts[4];
        size_t m_nextBlast = 0;
        std::vector<Pending> m_toBlow;
    };
}
