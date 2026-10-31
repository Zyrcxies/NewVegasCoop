// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstdint>

namespace coop
{
    class TalkLock
    {
    public:
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
        void Left(uint8_t player)
        {
            if (m_busy && m_talker == player)
                m_busy = false, m_npc = 0;
        }
        void Clear() { m_busy = false, m_npc = 0; }
        bool Busy() const { return m_busy; }
        uint8_t Talker() const { return m_talker; }
        uint32_t Npc() const { return m_busy ? m_npc : 0; }
        bool MayTalk(uint8_t player) const { return !m_busy || m_talker == player; }

    private:
        bool m_busy = false;
        uint8_t m_talker = 0;
        uint32_t m_npc = 0;
    };
}
