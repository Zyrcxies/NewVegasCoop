#pragma once

// Conversations (step 7.4): one at a time in the group (DESIGN). The host's game keeps the lock: the first player
// to start one may talk; anyone else starting one meanwhile is refused (their game closes it, with a message).
// Game-independent: tested in tests/.

#include <cstdint>

namespace coop
{
    class TalkLock
    {
    public:
        // A player's conversation started (npc: who with, maybe 0 if unknown) or ended (talking false).
        // Returns whether they may talk: nobody else is.
        bool Report(uint8_t player, bool talking, uint32_t npc)
        {
            if (!talking)
            {
                Left(player);
                return true;
            }
            if (m_busy && m_talker != player)
                return false;
            m_busy = true;
            m_talker = player;
            m_npc = npc;
            return true;
        }
        void Left(uint8_t player)  // they left the game (or their conversation ended)
        {
            if (m_busy && m_talker == player)
                m_busy = false, m_npc = 0;
        }
        void Clear() { m_busy = false, m_npc = 0; }
        bool Busy() const { return m_busy; }
        uint8_t Talker() const { return m_talker; }  // meaningful while Busy()
        uint32_t Npc() const { return m_busy ? m_npc : 0; }
        bool MayTalk(uint8_t player) const { return !m_busy || m_talker == player; }

    private:
        bool m_busy = false;
        uint8_t m_talker = 0;
        uint32_t m_npc = 0;
    };
}
