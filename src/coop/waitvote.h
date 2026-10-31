// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include "net/protocol.h"

#include <cstdint>
#include <vector>

namespace coop
{
    class WaitVote
    {
    public:
        static constexpr uint8_t kMaxHours = 24;
        static constexpr uint32_t kAnswerMs = 30000;

        enum class Outcome { Open, Passed, Refused };

        bool Start(uint8_t requester, uint8_t hours, bool sleep, const std::vector<uint8_t>& voters, uint32_t now)
        {
            if (m_open || hours < 1 || hours > kMaxHours)
                return false;
            m_open = true;
            m_requester = requester;
            m_hours = hours;
            m_sleep = sleep;
            m_waiting.clear();
            for (uint8_t v : voters)
                if (v != requester)
                    m_waiting.push_back(v);
            m_refusedBy = net::kNobody;
            m_endsAt = now + kAnswerMs;
            return true;
        }

        void Answer(uint8_t player, bool yes)
        {
            if (!m_open || !Asked(player))
                return;
            Forget(player);
            if (!yes && m_refusedBy == net::kNobody)
                m_refusedBy = player;
        }

        void Left(uint8_t player)
        {
            if (!m_open)
                return;
            if (player == m_requester)
            {
                m_open = false;
                return;
            }
            Forget(player);
        }

        void Cancel(uint8_t player)
        {
            if (m_open && player == m_requester)
                m_open = false;
        }

        Outcome Check(uint32_t now)
        {
            if (!m_open)
                return Outcome::Open;
            if (m_refusedBy != net::kNobody || (!m_waiting.empty() && static_cast<int32_t>(now - m_endsAt) >= 0))
            {
                m_open = false;
                return Outcome::Refused;
            }
            if (m_waiting.empty())
            {
                m_open = false;
                return Outcome::Passed;
            }
            return Outcome::Open;
        }

        bool IsOpen() const { return m_open; }
        bool Asked(uint8_t player) const
        {
            for (uint8_t v : m_waiting)
                if (v == player)
                    return true;
            return false;
        }
        uint8_t Requester() const { return m_requester; }
        uint8_t Hours() const { return m_hours; }
        bool Sleep() const { return m_sleep; }
        uint8_t RefusedBy() const { return m_refusedBy; }

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

        bool m_open = false;
        uint8_t m_requester = 0;
        uint8_t m_hours = 0;
        bool m_sleep = false;
        uint8_t m_refusedBy = net::kNobody;
        uint32_t m_endsAt = 0;
        std::vector<uint8_t> m_waiting;
    };
}
