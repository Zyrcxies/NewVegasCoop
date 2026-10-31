// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#include "game/companions.h"

#include "coop/clock.h"
#include "game/addresses.h"
#include "game/game.h"
#include "game/standins.h"
#include "log.h"

#include "nvse/GameForms.h"
#include "nvse/GameObjects.h"

#include <cmath>

namespace game
{
    namespace
    {
        constexpr uint32_t kLookMs = 1000;
        constexpr uint32_t kHireMs = 5000;
    }

    void Companions::FriendLineRan(uint8_t playerId, uint32_t now)
    {
        m_lineFrom = playerId;
        m_lineAt = now;
    }

    void Companions::Follow(uint32_t id, uint8_t playerId, coop::Coop& coop, const StandIns& standIns)
    {
        TESObjectREFR* ref = LookupRef(id);
        TESObjectREFR* leader = standIns.Character(playerId);
        if (!ref || !leader)
            return;
        Package& p = m_packages[playerId];
        if (!p.form || p.target != RefId(leader))
        {
            p.form = FollowPackage(leader);
            p.target = RefId(leader);
        }
        if (!p.form)
            return LOG_WARN("Companions: couldn't make %s follow %s (no follow package)", Name(ref),
                coop.NameOf(playerId).c_str());
        if (const auto* t = *reinterpret_cast<const uint8_t* const*>(reinterpret_cast<const uint8_t*>(p.form) +
            layout::kPackageTarget))
            LOG_DEBUG("Companions: follow package %08X aims at %08X (target type %u)", p.form->refID,
                RefId(*reinterpret_cast<TESObjectREFR* const*>(t + 4)), t[0]);
        SetWaiting(ref, false);
        GivePackage(ref, p.form);
        m_assigned[id] = playerId;
        LOG_INFO("Companions: %s (%08X) now follows %s", Name(ref), id, coop.NameOf(playerId).c_str());
    }

    void Companions::Release(uint32_t id, const char* why)
    {
        if (m_assigned.erase(id) == 0)
            return;
        TESObjectREFR* ref = LookupRef(id);
        if (ref)
            DropPackage(ref);
        LOG_INFO("Companions: %s (%08X) follows the host again (%s)", ref ? Name(ref) : "?", id, why);
    }

    void Companions::ReleaseAll()
    {
        while (!m_assigned.empty())
            Release(m_assigned.begin()->first, "co-op over");
        m_packages.clear();
        m_known.clear();
    }

    void Companions::Update(coop::Coop& coop, const StandIns& standIns, uint32_t now)
    {
        if (coop.GetRole() != coop::Role::Host || !coop.InGame())
        {
            if (!m_assigned.empty() || !m_known.empty())
                ReleaseAll();
            return;
        }
        for (const coop::Coop::CompanionOrderFrom& o : coop.TakeCompanionOrders())
        {
            TESObjectREFR* ref = LookupRef(o.order.ref);
            if (!ref || !IsTeammate(ref))
                continue;
            if (o.order.order == net::CompanionOrderKind::FollowMe)
                Follow(o.order.ref, o.playerId, coop, standIns);
            else
            {
                Release(o.order.ref, "told to wait");
                SetWaiting(ref, true);
                LOG_INFO("Companions: %s told %s (%08X) to wait", coop.NameOf(o.playerId).c_str(), Name(ref), o.order.ref);
            }
            m_nextLook = now;
        }
        if (!coop::IsDue(now, m_nextLook))
            return;
        m_nextLook = now + kLookMs;

        if (!m_assigned.empty() && CompanionWheelOpen())
        {
            uint32_t nearest = 0;
            float best = 400;
            coop::Pose me, at;
            for (const auto& [id, player] : m_assigned)
                if (GetPose(Player(), me) && GetPose(LookupRef(id), at) && at.space == me.space)
                    if (float d = std::hypot(at.x - me.x, at.y - me.y); d < best)
                    {
                        best = d;
                        nearest = id;
                    }
            if (nearest)
                Release(nearest, "our player took it back");
        }

        std::vector<net::CompanionState> states;
        std::set<uint32_t> now_;
        for (uint32_t id : Teammates())
        {
            TESObjectREFR* ref = LookupRef(id);
            if (!ref || IsDead(ref) || !net::IsSharedRef(id) || states.size() >= net::kMaxCompanions)
                continue;
            now_.insert(id);
            auto a = m_assigned.find(id);
            if (a == m_assigned.end() && !m_known.count(id) && m_lineFrom && now - m_lineAt < kHireMs)
            {
                Follow(id, m_lineFrom, coop, standIns);
                a = m_assigned.find(id);
            }
            if (a != m_assigned.end())
            {
                TESObjectREFR* leader = standIns.Character(a->second);
                auto p = m_packages.find(a->second);
                if (!leader)
                {
                    Release(id, "their player has gone");
                    a = m_assigned.end();
                }
                else if (p == m_packages.end() || p->second.target != RefId(leader))
                    Follow(id, a->second, coop, standIns);
                else if (uint32_t doing = InCombat(ref) || !p->second.form ? 0 : CurrentPackage(ref);
                    doing && doing != p->second.form->refID)
                {
                    GivePackage(ref, p->second.form);
                    LOG_DEBUG("Companions: %s (%08X) was doing %08X; following %s again", Name(ref), id, doing,
                        coop.NameOf(a->second).c_str());
                }
            }
            uint8_t leader = a != m_assigned.end() ? a->second : IsWaiting(ref) ? net::kCompanionWaiting :
                coop::kHostPlayerId;
            states.push_back({id, leader});
        }
        std::vector<uint32_t> gone;
        for (const auto& [id, player] : m_assigned)
            if (!now_.count(id))
                gone.push_back(id);
        for (uint32_t id : gone)
            Release(id, "left the party");
        m_known = std::move(now_);
        coop.SetCompanions(states);
    }
}
