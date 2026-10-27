#pragma once

// Items lying in the world (step 6.3): the host's game decides which lie where. The host shares the items near
// the group and does friends' pick-ups and drops; a friend's game follows (coop::FriendItems decides what to do).
// Step 8.9: items a friend's game moved (carried, knocked over, blown away) are moved in the host's game too, and
// each game glides items to the other's place rather than popping them there. Things physics moves that aren't
// items (crates, tumbleweeds) are shared the same way; they're never picked up.

#include "coop/coop.h"

#include <cstdint>
#include <map>

namespace game
{
    class WorldItems
    {
    public:
        void Update(coop::Coop& coop, uint32_t now);  // every frame

    private:
        void HostApply(coop::Coop& coop, uint32_t now);
        // An item going to another game's place for it: it glides there over about a look, then stays put a while
        // (held: while they carry it, as long as they keep saying so) so it doesn't fall between their updates.
        struct Glide
        {
            coop::ItemPose from, to;
            uint32_t startedAt = 0;
            bool held = false;
        };
        void GlideTo(uint32_t ref, const coop::ItemPose& to, bool held, uint32_t now, const char* whose);
        void Glides(uint32_t now);  // every frame
        bool Following(uint32_t ref, uint32_t now) const;  // gliding, or settling after
        std::map<uint32_t, Glide> m_glides;
        std::map<uint32_t, uint32_t> m_settling;  // ref -> when we let it go
        uint32_t m_grabbed = 0;                   // what our player carries (at the last look)

        uint32_t m_nextLook = 0;
        const void* m_area = nullptr;  // the room we're in, or the worldspace outdoors (at the last look)
        // Logged now and then (CLAUDE.md: measure).
        double m_lookMs = 0;
        uint32_t m_looks = 0, m_near = 0, m_takes = 0, m_drops = 0, m_refunds = 0, m_made = 0, m_removed = 0,
            m_moved = 0, m_sent = 0, m_nextLog = 0;
    };
}
