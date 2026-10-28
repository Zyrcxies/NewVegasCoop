#pragma once

// Shared XP and karma (step 7.2): what our player gained by themselves, as opposed to what we just gave them
// because another player gained it (which mustn't be passed on again). Game-independent: tested in tests/.

#include <cstdint>
#include <map>
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
        // We're about to give our player this (another player's gain).
        void Expect(int32_t xp, int32_t karma)
        {
            m_expectXp += xp;
            m_expectKarma += karma;
        }
        // Our player's XP and karma now; call a few times a second. Returns what they gained by themselves.
        Gain Look(int32_t xp, int32_t karma)
        {
            Gain own;
            if (m_started && xp >= m_xp)
            {
                int32_t gained = xp - m_xp;
                // Ours: beyond what other players' gains (just given) account for.
                own.xp = gained > m_expectXp ? gained - m_expectXp : 0;
                // Karma stops at its limits, so a gain we gave may not all show: a change no bigger than it is
                // put down to it.
                int32_t changed = karma - m_karma;
                bool covered = m_expectKarma != 0 && (changed < 0 ? -changed : changed) <=
                    (m_expectKarma < 0 ? -m_expectKarma : m_expectKarma);
                own.karma = covered ? 0 : changed - m_expectKarma;
            }
            // (XP going down: a save was loaded. Starts afresh.)
            m_xp = xp;
            m_karma = karma;
            m_started = true;
            m_expectXp = m_expectKarma = 0;
            return own;
        }
        void Forget() { m_started = false; m_expectXp = m_expectKarma = 0; }  // a save is loading

    private:
        bool m_started = false;
        int32_t m_xp = 0, m_karma = 0;
        int32_t m_expectXp = 0, m_expectKarma = 0;
    };

    // Friend: faction reputations are the host's (set to theirs), but our own game changes them too, by itself (our
    // murder: two-PC lab, 2026-10-28). Those changes go to the host. Only reputations the host has told us about.
    class ReputationWatcher
    {
    public:
        // The host's (we set ours to it).
        void Heard(uint32_t form, float fame, float infamy) { m_known[form] = {fame, infamy}; }
        // Ours now: true, with the change, if our game changed it since.
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
        void Forget() { m_known.clear(); }  // a save is loading

    private:
        std::map<uint32_t, std::pair<float, float>> m_known;
    };
}
