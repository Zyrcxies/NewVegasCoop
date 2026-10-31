// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#include "coop/desync.h"

namespace coop
{
    void DesyncTracker::Saw(const Disagreement& d, uint32_t now)
    {
        auto key = std::make_tuple(d.kind, d.ref, d.aspect);
        auto it = m_entries.find(key);
        if (it == m_entries.end())
        {
            if (m_entries.size() >= kMaxDesyncs)
                return;
            it = m_entries.emplace(key, Entry{d, now, now, false}).first;
        }
        else if (now - it->second.lastSeen > kDesyncGoneMs)
            it->second.firstSeen = now;
        it->second.what.detail = d.detail;
        it->second.lastSeen = now;
    }

    std::vector<DesyncEvent> DesyncTracker::Sweep(uint32_t now)
    {
        std::vector<DesyncEvent> out;
        for (auto it = m_entries.begin(); it != m_entries.end();)
        {
            Entry& e = it->second;
            if (now - e.lastSeen > kDesyncGoneMs)
            {
                if (e.reported)
                    out.push_back({e.what, false, e.lastSeen - e.firstSeen});
                it = m_entries.erase(it);
                continue;
            }
            if (!e.reported && e.lastSeen - e.firstSeen >= kDesyncGraceMs)
            {
                e.reported = true;
                out.push_back({e.what, true, e.lastSeen - e.firstSeen});
            }
            ++it;
        }
        return out;
    }

    DesyncCounts DesyncTracker::Lasting() const
    {
        DesyncCounts c;
        for (const auto& [key, e] : m_entries)
            if (e.reported)
            {
                uint16_t& n = e.what.kind == DesyncKind::Npc ? c.npcs : e.what.kind == DesyncKind::Item ? c.items : c.doors;
                if (n < 0xFFFF)
                    ++n;
            }
        return c;
    }

    const char* DesyncKindName(DesyncKind kind)
    {
        return kind == DesyncKind::Npc ? "NPC" : kind == DesyncKind::Item ? "item" : "door";
    }
}
