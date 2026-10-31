// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

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
        constexpr uint32_t kLookMs = 250;
        constexpr uint32_t kListMs = 2000;
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
            return false;
        bool done, blown = false;
        if (state == net::kMineOff)
            done = TurnOffMine(ref);
        else if (state == net::kMineBlown && fresh && playerId != coop::kHostPlayerId)
        {
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
            RemoveWorldItem(ref);
            done = true;
        }
        uint8_t reached = state > net::kMineGone ? net::kMineGone : state;
        m_seen[id] = done ? reached : static_cast<uint8_t>(was);
        if (done)
            ++m_applied;
        LOG_DEBUG("Mines: %08X %s, as in another player's game%s", id, blown ? "set off" : StateName(state),
            done ? "" : " (couldn't)");
        return done;
    }

    void Mines::Update(coop::Coop& coop, const StandIns& standIns, uint32_t now)
    {
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
                m_sparedAt = now;
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
            Apply(m.ref, m.state, true, from.playerId, now);
        }

        if (!coop::IsDue(now, m_nextLook))
            return;
        m_nextLook = now + kLookMs;
        TESObjectREFR* player = Player();
        if (!player || !player->parentCell)
            return;

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
                m_seen[id] = static_cast<uint8_t>(state);
                if (furthest > state)
                    Apply(id, furthest, false, 0, now);
                continue;
            }
            if (state <= seen->second)
                continue;
            seen->second = static_cast<uint8_t>(state);
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
                m_seen.clear();
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
