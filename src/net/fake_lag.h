#pragma once

// Pretend bad internet, for testing on one PC (where everything arrives in about 1 ms). Messages are held
// back half the ping each way, plus up to jitterMs more at random. Unreliable messages can be lost; a lost
// reliable one is resent, so it arrives a round trip later. Order is kept within reliable and within
// unreliable messages, like the real connection. Game-independent: tested in tests/.

#include <cstdint>
#include <map>
#include <random>

namespace net
{
    struct FakeLag
    {
        uint32_t pingMs = 0;       // round trip
        uint32_t jitterMs = 0;     // up to this much extra each way, at random
        uint32_t lossPercent = 0;  // share of messages lost (0 to 100)
        bool On() const { return pingMs || jitterMs || lossPercent; }
    };

    template <class T>
    class LagQueue
    {
    public:
        // Barrier: arrives after everything before it, and everything after it arrives later (connects and
        // disconnects).
        enum class Kind { Reliable, Unreliable, Barrier };

        explicit LagQueue(FakeLag lag, uint32_t seed = 1) : m_lag(lag), m_rng(seed)
        {
            if (m_lag.lossPercent > 100)
                m_lag.lossPercent = 100;
        }

        void Push(uint64_t now, Kind kind, T item)
        {
            uint64_t due = now + m_lag.pingMs / 2 + Random(m_lag.jitterMs);
            if (kind != Kind::Barrier && Random(99) < m_lag.lossPercent)
            {
                if (kind == Kind::Unreliable)
                    return;
                due += m_lag.pingMs + Random(m_lag.jitterMs);  // resent after a round trip
            }
            if (kind != Kind::Unreliable && due < m_lastReliable)
                due = m_lastReliable;
            if (kind != Kind::Reliable && due < m_lastUnreliable)
                due = m_lastUnreliable;
            if (kind != Kind::Unreliable)
                m_lastReliable = due;
            if (kind != Kind::Reliable)
                m_lastUnreliable = due;
            m_items.emplace(due, std::move(item));  // equal times stay in the order pushed
        }

        // The next message that's due by now, if any.
        bool Pop(uint64_t now, T& out)
        {
            if (m_items.empty() || m_items.begin()->first > now)
                return false;
            out = std::move(m_items.begin()->second);
            m_items.erase(m_items.begin());
            return true;
        }

        size_t Size() const { return m_items.size(); }

    private:
        uint32_t Random(uint32_t max)  // 0 to max
        {
            return max ? std::uniform_int_distribution<uint32_t>(0, max)(m_rng) : 0;
        }

        FakeLag m_lag;
        std::mt19937 m_rng;
        std::multimap<uint64_t, T> m_items;  // by when it arrives
        uint64_t m_lastReliable = 0, m_lastUnreliable = 0;
    };
}
