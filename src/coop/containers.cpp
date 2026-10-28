#include "coop/containers.h"

#include <map>
#include <tuple>

namespace coop
{
    std::vector<net::ItemCount> ItemDiff(const std::vector<net::ItemCount>& from,
        const std::vector<net::ItemCount>& to)
    {
        // (By kind: the same gun at another condition, or with other mods, is another kind.)
        std::map<std::tuple<uint32_t, uint8_t, uint8_t>, int64_t> change;
        for (const net::ItemCount& i : to)
            change[{i.item, i.health, i.mods}] += i.count;
        for (const net::ItemCount& i : from)
            change[{i.item, i.health, i.mods}] -= i.count;
        std::vector<net::ItemCount> out;
        for (const auto& [kind, count] : change)
            if (count != 0)
                out.push_back({std::get<0>(kind), static_cast<int32_t>(count), std::get<1>(kind), std::get<2>(kind)});
        return out;
    }

    bool FriendContainer::View(uint32_t ref, bool open)
    {
        m_open = ref && open;
        if (ref == m_ref)
            return false;
        m_ref = ref;
        m_hasSnapshot = m_hasHost = false;
        m_snapshot.clear();
        return true;
    }

    std::vector<net::ItemCount> FriendContainer::Look(const std::vector<net::ItemCount>& local,
        std::vector<net::ContainerChange>& changes)
    {
        if (!m_ref)
            return {};
        // (Only aimed at: anything that changed it wasn't our player.)
        if (m_hasSnapshot && m_open)
            for (const net::ItemCount& d : ItemDiff(m_snapshot, local))
                changes.push_back({m_ref, ++m_seq, d});  // it gained: put in; it lost: taken out
        m_snapshot = local;
        m_hasSnapshot = true;
        // The host's contents, once they include all our changes (none may still be on the way).
        if (m_hasHost && m_host.ref == m_ref && static_cast<int32_t>(m_host.seen - m_seq) >= 0)
        {
            m_hasHost = false;
            return ItemDiff(local, m_host.items);
        }
        return {};
    }

    void FriendContainer::Heard(const net::ContainerContents& m)
    {
        if (m.ref != m_ref || !m_ref)
            return;
        m_host = m;
        m_hasHost = true;
    }
}
