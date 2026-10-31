// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstdint>

namespace coop
{
    class ShotCounter
    {
    public:
        uint32_t Look(bool attackStarted, bool inAttack, uint32_t roundsGone, bool reloadStarted)
        {
            uint32_t shots = 0;
            if (attackStarted)
            {
                ++shots;
                ++m_ahead;
            }
            uint32_t matched = roundsGone < m_ahead ? roundsGone : m_ahead;
            m_ahead -= matched;
            shots += roundsGone - matched;
            if (!inAttack || reloadStarted)
                m_ahead = 0;
            return shots;
        }

    private:
        uint32_t m_ahead = 0;
    };
}
