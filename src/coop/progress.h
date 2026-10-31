// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <utility>

namespace coop
{
    struct Gain
    {
        int32_t xp = 0;
        int32_t karma = 0;
    };

    class ProgressWatcher
    {
    public:
        void Expect(int32_t xp, int32_t karma)
        {
            m_expectXp += xp;
            m_expectKarma += karma;
        }
        Gain Look(int32_t xp, int32_t karma)
        {
            Gain own;
            if (m_started && xp >= m_xp)
            {
                int32_t gained = xp - m_xp;
                own.xp = gained > m_expectXp ? gained - m_expectXp : 0;
                int32_t changed = karma - m_karma;
                bool covered = m_expectKarma != 0 && (changed < 0 ? -changed : changed) <=
                    (m_expectKarma < 0 ? -m_expectKarma : m_expectKarma);
                own.karma = covered ? 0 : changed - m_expectKarma;
            }
            m_xp = xp;
            m_karma = karma;
            m_started = true;
            m_expectXp = m_expectKarma = 0;
            return own;
        }
        void Forget() { m_started = false; m_expectXp = m_expectKarma = 0; }

    private:
        bool m_started = false;
        int32_t m_xp = 0, m_karma = 0;
        int32_t m_expectXp = 0, m_expectKarma = 0;
    };

    class ReputationWatcher
    {
    public:
        void Heard(uint32_t form, float fame, float infamy) { m_known[form] = {fame, infamy}; }
        bool Look(uint32_t form, float fame, float infamy, float& fameChange, float& infamyChange)
        {
            auto it = m_known.find(form);
            if (it == m_known.end() || (it->second.first == fame && it->second.second == infamy))
                return false;
            fameChange = fame - it->second.first;
            infamyChange = infamy - it->second.second;
            it->second = {fame, infamy};
            return true;
        }
        void Forget() { m_known.clear(); }

    private:
        std::map<uint32_t, std::pair<float, float>> m_known;
    };

    class TravelWatcher
    {
    public:
        bool Look(uint32_t marker) { return m_known.insert(marker).second; }
        void Heard(uint32_t marker) { m_known.insert(marker); }
        void Forget() { m_known.clear(); }
        size_t Known() const { return m_known.size(); }

    private:
        std::set<uint32_t> m_known;
    };
}
