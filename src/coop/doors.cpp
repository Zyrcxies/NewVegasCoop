// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#include "coop/doors.h"

#include "coop/clock.h"

#include <algorithm>

namespace coop
{
    void FriendDoors::Heard(const net::DoorState& host)
    {
        auto it = m_doors.find(host.ref);
        if (it == m_doors.end())
        {
            if (m_doors.size() >= kMaxKnownDoors)
                return;
            it = m_doors.emplace(host.ref, Door{}).first;
        }
        it->second.host = host;
        it->second.heard = true;
    }

    std::vector<net::DoorState> FriendDoors::Observe(uint32_t now, const std::vector<LocalDoor>& local,
        std::vector<net::DoorState>& uses)
    {
        std::vector<net::DoorState> fixes;
        for (auto it = m_doors.begin(); it != m_doors.end();)
        {
            bool here = std::any_of(local.begin(), local.end(),
                [&](const LocalDoor& d) { return d.state.ref == it->first; });
            it = here ? std::next(it) : m_doors.erase(it);
        }
        for (const LocalDoor& d : local)
        {
            auto it = m_doors.find(d.state.ref);
            if (it == m_doors.end())
            {
                if (m_doors.size() >= kMaxKnownDoors)
                    continue;
                it = m_doors.emplace(d.state.ref, Door{}).first;
            }
            Door& k = it->second;
            bool changed = k.seen && d.state != k.last;
            k.last = d.state;
            k.seen = true;
            if (k.waitUntil)
            {
                if (k.heard && k.host == d.state)
                    k.waitUntil = 0;
                else if (!IsDue(now, k.waitUntil))
                    continue;
                k.waitUntil = 0;
            }
            if (changed && d.nearMe && (!k.heard || d.state != k.host))
            {
                uses.push_back(d.state);
                k.waitUntil = now + kDoorUseWaitMs;
                if (!k.waitUntil)
                    k.waitUntil = 1;
                continue;
            }
            if (k.heard && d.state != k.host)
                fixes.push_back(k.host);
        }
        return fixes;
    }
}
