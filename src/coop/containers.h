#pragma once

// Containers and bodies on a friend's game (step 6.2): the one our player has open is kept to what the host's game
// says it holds. Our player's takes and puts are numbered and sent to the host; the host's contents are only used
// once they include all of them, so nothing is taken or put twice. Game-independent: tested in tests/.

#include "net/protocol.h"

#include <cstdint>
#include <vector>

namespace coop
{
    // What to add (positive) or take out (negative) to turn `from` into `to`.
    std::vector<net::ItemCount> ItemDiff(const std::vector<net::ItemCount>& from,
        const std::vector<net::ItemCount>& to);

    class FriendContainer
    {
    public:
        // Our player opened `ref` (the host's id for it; 0: closed it). True if that's a change (tell the host).
        bool View(uint32_t ref);
        uint32_t Viewing() const { return m_ref; }
        // What it holds in our game now; call a few times a second while it's open. Our player's takes and puts
        // since the last look go in `changes` (to send). Returns what to add to or take out of ours to match the
        // host's, once the host has seen all our changes; then call Settle.
        std::vector<net::ItemCount> Look(const std::vector<net::ItemCount>& local,
            std::vector<net::ContainerChange>& changes);
        // What ours holds after matching the host's (anything that couldn't be added isn't taken for our doing).
        void Settle(const std::vector<net::ItemCount>& local) { m_snapshot = local; }
        void Heard(const net::ContainerContents& m);

    private:
        uint32_t m_ref = 0;
        uint32_t m_seq = 0;  // our changes sent so far (all containers)
        bool m_hasSnapshot = false;
        std::vector<net::ItemCount> m_snapshot;  // ours at the last look
        bool m_hasHost = false;
        net::ContainerContents m_host;
    };
}
