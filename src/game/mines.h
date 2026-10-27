#pragma once

// Mines (Alex, 2026-10-19 session: a friend disarming a mine set off the host's copy of it, which their puppet had
// walked into). Each game's mines go off for its own player only (game::SetMineRules), and a mine placed in the world
// that's disarmed, blown up or picked up in one game is the same in all (thrown ones exist only in their own game).

#include "coop/coop.h"

#include <cstdint>
#include <map>
#include <vector>

namespace game
{
    class StandIns;

    class Mines
    {
    public:
        void Update(coop::Coop& coop, const StandIns& standIns, uint32_t now);  // every frame
        void Forget();  // a save is loading: its mines are as that save has them
        // Host: we just set off our copy of a mine this player's game blew up; its hits on their character aren't
        // sent to them (they were hurt by their own).
        bool SparedFrom(int playerId, uint32_t now) const { return playerId == m_spared && now - m_sparedAt < 2000; }
        // Friend: a blast the host shares this close to one of our mines that just blew up is ours coming back
        // (the host's copy, set off for its NPCs): not shown twice.
        bool BlewNear(float x, float y, float z, uint32_t now) const;

    private:
        // Ours made `state` (another player's change; fresh: just now, from `playerId`); true if done.
        bool Apply(uint32_t id, uint8_t state, bool fresh, uint8_t playerId, uint32_t now);
        struct Blast { float x = 0, y = 0, z = 0; uint32_t at = 0; };
        struct Pending { uint32_t id = 0, since = 0; };

        std::vector<uint32_t> m_refs;         // the mines near us (placed in the world), listed now and then
        std::map<uint32_t, uint8_t> m_seen;   // each one's state when we last looked (0 armed)
        std::map<uint32_t, uint8_t> m_known;  // the furthest each got anywhere (ours, or others told us)
        const void* m_cell = nullptr;         // the cell we were in when listing them
        uint32_t m_nextList = 0, m_nextLook = 0;
        uint32_t m_reported = 0, m_applied = 0, m_nextLog = 0;
        int m_spared = -1;            // host: whose blast we last set off (SparedFrom)
        uint32_t m_sparedAt = 0;
        Blast m_blasts[4];            // our last mines blown up here (BlewNear)
        size_t m_nextBlast = 0;
        std::vector<Pending> m_toBlow;  // host: friends' blasts to set off here, once our mine is gone
    };
}
