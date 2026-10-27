#pragma once

// Items lying in the world on a friend's game (step 6.3). The host's game decides which items lie where: ours are
// taken away when the host's are, items the host's game made while playing (drops) get a copy here, and anything
// else made here is cleared away. Our player picking an item up or dropping one is sent to the host. Step 8.9: our
// game moving one of the host's items (our player carrying, pushing or shooting it, our grenade blowing it away) is
// sent too; the host's game moves its own to match, so everyone sees it.
// Game-independent: tested in tests/.

#include "coop/desync.h"
#include "net/protocol.h"

#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace coop
{
    constexpr float kItemMoveDistance = 32;  // a lying item further than this from the host's spot is moved there
    constexpr float kItemTurnAngle = 0.35f;  // (radians, 20 degrees) or turned more than this from the host's angles
    // Once our game has moved one: smaller changes are sent too (where it comes to rest), and the host's place (behind
    // ours till it has heard) counts again this long after our last move. A carried one is sent at least every
    // kHeldResendMs, so the host's game keeps holding it up.
    constexpr float kItemNudge = 2;
    constexpr float kItemNudgeAngle = 0.03f;
    constexpr uint32_t kOwnItemMs = 2000;
    constexpr uint32_t kHeldResendMs = 2000;
    // The host's word on an item not heard again for this long (it repeats what's near every 3 s) is out of date:
    // gone out of the host's reach (2026-10-16 lab: a tumbleweed blown out of it was held up where it last heard).
    constexpr uint32_t kHostItemStaleMs = 7000;
    constexpr size_t kMaxKnownItems = 4000;  // host items kept track of; more and the list starts afresh

    struct LocalItem
    {
        uint32_t ref = 0;
        uint32_t base = 0;
        int32_t count = 1;
        bool present = true;  // false: taken (or gone)
        float x = 0, y = 0, z = 0;
        bool nearMe = false;  // close enough to be our player's doing (a movable: to be pushed, kicked or blown)
        float rx = 0, ry = 0, rz = 0;  // its angles, radians
        bool held = false;       // our player is carrying it
        bool following = false;  // our game is taking it to the host's place, or it's settling there: not our doing
        // Not an item but something physics moves (a crate, a tumbleweed): never picked up, and its moves are ours to
        // send only near our player (or carried): the wind rolls tumbleweeds in every game, its own way (2026-10-16
        // lab: both games kept sending theirs, 80 moves a minute, fighting).
        bool movable = false;
        bool deleted = false;  // gone (not present) because the game deleted it (picked up), not just switched off
        uint32_t parent = 0;   // what switches it on and off here (net::ItemState::parent), and whether that's on
        bool parentOn = false;
    };

    // Where an item is and how it's turned; Differs: further than `distance` or turned more than `angle` (radians)
    // around any axis.
    struct ItemPose
    {
        float x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0;
        bool Differs(const ItemPose& other, float distance, float angle) const;
    };

    struct ItemActions
    {
        // Copies to place (then call Made): of the host's made items, and of the host's items our save had lost
        // (picked up in it).
        std::vector<net::ItemState> make;
        std::vector<uint32_t> remove;      // our refs to take away
        std::vector<uint32_t> enable;      // our refs switched off here but there in the host's game: switch on
        struct Parent { uint32_t ref; bool on; };
        std::vector<Parent> parents;       // enable parents to switch on or off, as the host's are
        struct Move
        {
            uint32_t ref;
            float x, y, z;
            float rx = 0, ry = 0, rz = 0;
            bool held = false;  // the host's player is carrying it: held there till a move without
        };
        std::vector<Move> move;
        std::vector<net::ItemTake> takes;  // to send
        std::vector<net::ItemDrop> drops;  // to send
        std::vector<net::ItemMove> moved;  // to send: the host's items our game moved (step 8.9)
        std::vector<Disagreement> differ;  // disagreeing with the host's now (the desync detector; most are put right)
    };

    class FriendItems
    {
    public:
        void Heard(const net::ItemState& host, uint32_t now = 0);  // now: Look's clock
        // Our game's items near us as they are now; call a couple of times a second. sameArea: false after a trip
        // elsewhere (a door, fast travel), when what was near us is gone because we left. now: a clock in ms.
        ItemActions Look(const std::vector<LocalItem>& local, bool sameArea = true, uint32_t now = 0);
        // The copy we placed of the host's item (0: it couldn't be placed; not tried again).
        void Made(uint32_t hostRef, uint32_t ourRef);
        void Clear();
        size_t Copies() const { return m_copies.size(); }

    private:
        std::map<uint32_t, net::ItemState> m_host;    // by the host's ref
        std::map<uint32_t, uint32_t> m_heardAt;       // the host's ref -> when its word came
        std::map<uint32_t, uint32_t> m_copies;        // the host's made ref -> ours (0: being made)
        struct Seen { bool present; int32_t count; uint32_t base; bool nearMe; ItemPose pose; bool movable; };
        std::map<uint32_t, Seen> m_seen;              // our ref -> at the last look (the game counts a taken stack as 1)
        struct Own { ItemPose sent; bool held = false; uint32_t movedAt = 0, sentAt = 0; };
        std::map<uint32_t, Own> m_own;                // our ref -> our game moved it, and what we sent (step 8.9)
        std::set<uint32_t> m_holding;                 // our refs held where the host's player carries them
        std::map<uint32_t, uint32_t> m_followed;      // our ref -> the host's word on it we last went by (its time)
        std::set<uint32_t> m_dropped;                 // our refs sent as our player's drops
        std::set<uint32_t> m_removed;                 // our refs we took away (going is then not a pick-up)
        std::set<uint32_t> m_taken;                   // our refs our player picked up
        std::set<uint32_t> m_switchedOn;              // our refs we switched back on (tried once each)
        std::map<uint32_t, uint32_t> m_goneHere;      // our refs gone here, there in the host's word: since which word
        struct ParentGap { uint32_t firstWord = 0; bool asked = false; };
        std::map<uint32_t, ParentGap> m_parentGaps;   // enable parents ours differs on: since which word of the host's
        void Taken(uint32_t ourRef, uint32_t hostRef);  // hostRef: the host's, for a copy (else 0)
        bool m_started = false;
    };
}
