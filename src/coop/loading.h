#pragma once

// Loading (step 8.4, DESIGN): after a door, fast travel or the host loading a save, everyone who is already in
// the world (NPCs, enemies and players) is frozen until everyone has finished loading, so a slower PC isn't
// attacked first. The host's game keeps the list of who is still on their way. Game-independent: tested in tests/.

#include <cstdint>
#include <vector>

namespace coop
{
    class LoadGate
    {
    public:
        static constexpr uint32_t kMaxWaitMs = 60000;  // nobody waits longer than this for one player

        // The group is on its way: these players still have to arrive (the leader is there already).
        void Expect(const std::vector<uint8_t>& players, uint32_t now)
        {
            m_waiting = players;
            m_since = now;
        }
        void Arrived(uint8_t player) { Forget(player); }  // they finished loading (or didn't need to)
        void Left(uint8_t player) { Forget(player); }

        // Whether the world waits now. After kMaxWaitMs the rest are given up on (timedOut: who, for the log).
        bool Waiting(uint32_t now, std::vector<uint8_t>* timedOut = nullptr)
        {
            if (!m_waiting.empty() && now - m_since >= kMaxWaitMs)
            {
                if (timedOut)
                    *timedOut = m_waiting;
                m_waiting.clear();
            }
            return !m_waiting.empty();
        }
        uint8_t WaitingFor() const { return m_waiting.empty() ? 0 : m_waiting.front(); }  // one of them

    private:
        void Forget(uint8_t player)
        {
            for (size_t i = 0; i < m_waiting.size(); ++i)
                if (m_waiting[i] == player)
                {
                    m_waiting.erase(m_waiting.begin() + i);
                    return;
                }
        }

        std::vector<uint8_t> m_waiting;
        uint32_t m_since = 0;
    };
}
