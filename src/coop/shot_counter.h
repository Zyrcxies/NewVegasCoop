#pragma once

// Counting an NPC's gun shots (host, step 5.3), looked at a few times a second. Two clues, each missing some:
// - its attack animation starting: one per shot for most guns, but an automatic fires many in one;
// - rounds leaving its magazine: every shot, but a single-shot gun fires and reloads between two looks (rounds
//   go 1 -> 0 -> 1 unseen; step 5.4 test: a Powder Ganger's single-shot shotgun showed only its reloads).
// So each attack start is a shot at once, and the magazine adds only shots beyond those.

#include <cstdint>

namespace coop
{
    class ShotCounter
    {
    public:
        // One look: did an attack animation start, is it still in one (attack, follow-through, latency,
        // throw), how many rounds left the magazine since the last look, did a reload start. Returns the shots
        // to share now.
        uint32_t Look(bool attackStarted, bool inAttack, uint32_t roundsGone, bool reloadStarted)
        {
            uint32_t shots = 0;
            if (attackStarted)
            {
                ++shots;
                ++m_ahead;
            }
            uint32_t matched = roundsGone < m_ahead ? roundsGone : m_ahead;
            m_ahead -= matched;
            shots += roundsGone - matched;
            // Out of the attack (or reloading): an attack start nothing left the magazine for doesn't cancel later
            // rounds out.
            if (!inAttack || reloadStarted)
                m_ahead = 0;
            return shots;
        }

    private:
        uint32_t m_ahead = 0;  // shots shared for attack starts that no rounds have been seen leaving for yet
    };
}
