#pragma once

// More enemies for bigger groups (REBALANCE.md step 3; Co-op balanced, host): each generic enemy that starts fighting
// the group brings a share of an extra copy of itself, more with more players (3: a quarter, 4: a half), so a fight
// grows about that much; never more health.

#include <cstdint>

namespace coop
{
    class Reinforcements
    {
    public:
        static constexpr int kMaxAlive = 8;  // extras alive at once, at most

        // Extra enemies for each one in a fight, by the number of players in the session.
        static float PerEnemy(int players) { return players <= 2 ? 0.0f : 0.25f * static_cast<float>(players - 2); }

        // A new enemy started fighting the group.
        void EnemyJoined(int players) { m_owed += PerEnemy(players); }
        // Whether to add a copy now (at most one a call; none while kMaxAlive are alive, and then not all that was
        // owed: no rush of them once some die).
        bool Take(int alive)
        {
            if (alive >= kMaxAlive)
            {
                m_owed = m_owed > 1 ? 1 : m_owed;
                return false;
            }
            if (m_owed < 1)
                return false;
            m_owed -= 1;
            return true;
        }
        void Reset() { m_owed = 0; }

    private:
        float m_owed = 0;  // shares of a copy not spawned yet
    };
}
