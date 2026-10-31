// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstdint>

namespace coop
{
    class Reinforcements
    {
    public:
        static constexpr int kMaxAlive = 8;

        static float PerEnemy(int players) { return players <= 1 ? 0.0f : 0.5f * static_cast<float>(players - 1); }

        void EnemyJoined(int players) { m_owed += PerEnemy(players); }
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
        bool Owed() const { return m_owed >= 1; }

    private:
        float m_owed = 0;
    };
}
