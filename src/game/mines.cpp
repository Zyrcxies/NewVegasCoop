#include "game/mines.h"

#include "coop/clock.h"
#include "game/addresses.h"
#include "game/game.h"
#include "game/standins.h"
#include "log.h"

#include "nvse/GameObjects.h"

namespace game
{
    namespace
    {
        constexpr uint32_t kLookMs = 250;   // how often the mines' states are read
        constexpr uint32_t kListMs = 2000;  // how often the mines near us are listed again (and on a new cell)
        constexpr size_t kMaxRemembered = 4096;

        const char* StateName(uint8_t state)
        {
            return state == net::kMineOff ? "disarmed" : state == net::kMineGone ? "gone" :
                state == net::kMineBlown ? "blown up" : "armed";
        }
    }

    void Mines::Forget()
    {
        m_refs.clear();
        m_seen.clear();
        m_known.clear();
        m_toBlow.clear();
        m_cell = nullptr;
        m_nextList = 0;
    }

    bool Mines::BlewNear(float x, float y, float z, uint32_t now) const
    {
        for (const Blast& b : m_blasts)
            if (b.at && now - b.at < 3000 && (b.x - x) * (b.x - x) + (b.y - y) * (b.y - y) + (b.z - z) * (b.z - z) <
                300.0f * 300.0f)
                return true;
        return false;
    }

    bool Mines::Apply(uint32_t id, uint8_t state, bool fresh, uint8_t playerId, uint32_t now)
    {
        TESObjectREFR* ref = LookupRef(id);
        int was = MineState(ref);
        if (was < 0 || was >= state || was >= net::kMineGone)
            return false;  // (not loaded here, or already so)
        bool done, blown = false;
        if (state == net::kMineOff)
            done = TurnOffMine(ref);
        else if (state == net::kMineBlown && fresh && playerId != coop::kHostPlayerId)
        {
            // Host: a friend's game blew it up, so ours goes off too, for real: our NPCs (and anyone else) near it are
            // hurt, and everyone's game shows it. Not the friend again (SparedFrom). The mine goes first (its blast
            // would set it off too), the blast once it's gone (the next frame or so: Update).
            TurnOffMine(ref);
            RemoveWorldItem(ref);
            if (m_toBlow.size() < 8)
                m_toBlow.push_back({id, now});
            done = blown = true;
            m_spared = playerId;
            m_sparedAt = now;
        }
        else
        {
            RemoveWorldItem(ref);  // (its Disable shows on the ref a frame later: lab 2026-10-19)
            done = true;
        }
        uint8_t reached = state > net::kMineGone ? net::kMineGone : state;  // (blown up reads as gone)
        m_seen[id] = done ? reached : static_cast<uint8_t>(was);
        if (done)
            ++m_applied;
        LOG_DEBUG("Mines: %08X %s, as in another player's game%s", id, blown ? "set off" : StateName(state),
            done ? "" : " (couldn't)");
        return done;
    }

    void Mines::Update(coop::Coop& coop, const StandIns& standIns, uint32_t now)
    {
        // Who doesn't set mines off here: everyone else's character (it's a puppet; their own game has their
        // mines), and in a friend's game every NPC for the mines placed in the world (the host's NPCs, in the
        // host's game, do).
        TESObjectREFR* puppets[8];
        size_t count = 0;
        bool inGame = coop.InGame();
        if (inGame)
            for (const auto& [id, remote] : coop.Remotes())
                if (TESObjectREFR* who = standIns.Character(id); who && count < 8)
                    puppets[count++] = who;
        SetMineRules(puppets, count, inGame && coop.GetRole() == coop::Role::Friend);
        if (!inGame)
        {
            if (m_cell)
                Forget();
            return;
        }

        for (size_t i = 0; i < m_toBlow.size();)
        {
            TESObjectREFR* ref = LookupRef(m_toBlow[i].id);
            if (MineState(ref) == net::kMineGone || now - m_toBlow[i].since > 1000)
            {
                bool shown = MineState(ref) == net::kMineGone && MineExplosion(ref);
                LOG_DEBUG("Mines: %08X's blast %s", m_toBlow[i].id, shown ? "set off here" : "not made (the mine stayed)");
                m_sparedAt = now;  // (its hits come from now)
                m_toBlow.erase(m_toBlow.begin() + i);
            }
            else
                ++i;
        }

        for (const coop::Coop::MineFrom& from : coop.TakeMines())
        {
            const net::MineChange& m = from.change;
            uint8_t& known = m_known[m.ref];
            if (m.state <= known)
                continue;
            known = m.state;
            Apply(m.ref, m.state, true, from.playerId, now);  // (not loaded here: it's made so when we come near)
        }

        if (!coop::IsDue(now, m_nextLook))
            return;
        m_nextLook = now + kLookMs;
        TESObjectREFR* player = Player();
        if (!player || !player->parentCell)
            return;

        // Changes here go to the others (looked at before listing again, so none is missed in between).
        for (uint32_t id : m_refs)
        {
            TESObjectREFR* ref = LookupRef(id);
            int state = MineState(ref);
            if (state < 0)
                continue;
            auto known = m_known.find(id);
            uint8_t furthest = known == m_known.end() ? 0 : known->second;
            auto seen = m_seen.find(id);
            if (seen == m_seen.end())
            {
                m_seen[id] = static_cast<uint8_t>(state);  // first look: as it was
                if (furthest > state)
                    Apply(id, furthest, false, 0, now);  // someone else's change from before it was loaded here
                continue;
            }
            if (state <= seen->second)
                continue;
            seen->second = static_cast<uint8_t>(state);
            // Gone: picked up (the ref deleted), or blown up (only disabled: lab 2026-10-19, flags 0x2B and 0x80B).
            uint8_t change = static_cast<uint8_t>(state);
            if (state == net::kMineGone && !(ref->flags & layout::kRefDeleted))
            {
                change = net::kMineBlown;
                m_blasts[m_nextBlast++ % 4] = {ref->posX, ref->posY, ref->posZ, now ? now : 1};
            }
            if (change > furthest)
            {
                m_known[id] = change;
                coop.ReportMine(id, change);
                ++m_reported;
                LOG_DEBUG("Mines: %08X %s here (flags %08X)", id, StateName(change), static_cast<unsigned>(ref->flags));
            }
        }

        if (player->parentCell != m_cell || coop::IsDue(now, m_nextList))
        {
            std::vector<TESObjectREFR*> found;
            MinesNear(found, LoadedReach());
            m_refs.clear();
            for (TESObjectREFR* mine : found)
                if (!net::IsMadeWhilePlaying(RefId(mine)))
                    m_refs.push_back(RefId(mine));
            m_cell = player->parentCell;
            m_nextList = now + kListMs;
            if (m_seen.size() > kMaxRemembered || m_known.size() > kMaxRemembered)
            {
                m_seen.clear();  // (a very long session: start over; what's near is looked at afresh)
                m_known.clear();
            }
        }

        if (coop::IsDue(now, m_nextLog))
        {
            if (m_nextLog && (m_reported || m_applied))
                LOG_INFO("Mines: %u near; %u changes here sent, %u from others made here",
                    static_cast<unsigned>(m_refs.size()), m_reported, m_applied);
            m_nextLog = now + 60000;
            m_reported = m_applied = 0;
        }
    }
}
