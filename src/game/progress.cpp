// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#include "game/progress.h"

#include "coop/clock.h"
#include "game/game.h"
#include "log.h"

#include <algorithm>
#include <string>
#include <vector>

namespace game
{
    namespace
    {
        constexpr uint32_t kLookMs = 250;
        constexpr uint32_t kReputationLookMs = 1000;
        constexpr uint32_t kReputationResendMs = 10000;
        constexpr uint32_t kQuietAfterJoinMs = 10000;
        constexpr uint32_t kTravelLookMs = 2000;
    }

    void Progress::Update(coop::Coop& coop, uint32_t now)
    {
        if (!coop.InGame())
        {
            if (m_wasInGame)
            {
                m_watcher.Forget();
                m_sentReputations.clear();
                m_reputationWatcher.Forget();
                m_travelWatcher.Forget();
                m_joins = 0;
                m_travelJoins = 0;
            }
            m_wasInGame = false;
            return;
        }
        if (!m_wasInGame)
            m_quietUntil = now + kQuietAfterJoinMs;
        m_wasInGame = true;
        bool host = coop.GetRole() == coop::Role::Host;

        for (const net::Progress& p : coop.TakeProgress())
        {
            m_watcher.Expect(p.xp, p.karma);
            GiveXp(p.xp);
            GiveKarma(p.karma);
            m_gotXp += p.xp;
            LOG_DEBUG("Progress: %s gained %d XP and %d karma; so do we", coop.NameOf(p.playerId).c_str(), p.xp,
                p.karma);
        }
        int32_t hostXp = 0;
        if (!host && coop::IsDue(now, m_quietUntil) && coop.TakeHostXp(hostXp))
        {
            int32_t mine = PlayerXp();
            if (hostXp > mine)
            {
                m_watcher.Expect(hostXp - mine, 0);
                GiveXp(hostXp - mine);
                LOG_INFO("Progress: caught up to the host: %d XP more (ours %d, the host's %d)", hostXp - mine, mine,
                    hostXp);
                Notify(("Co-op: caught up to the host's level (+" + std::to_string(hostXp - mine) + " XP)").c_str(), 6);
            }
            else
                LOG_INFO("Progress: not behind the host (ours %d XP, the host's %d)", mine, hostXp);
        }
        if (coop::IsDue(now, m_nextLook))
        {
            m_nextLook = now + kLookMs;
            int32_t xpNow = PlayerXp();
            if (host)
                coop.SetMyXp(xpNow);
            coop::Gain own = m_watcher.Look(xpNow, PlayerKarma());
            if (own.xp > 0 || own.karma)
            {
                coop.ShareProgress(own.xp, own.karma);
                m_sharedXp += own.xp;
                LOG_DEBUG("Progress: we gained %d XP and %d karma; shared", own.xp, own.karma);
            }
        }
        Reputations(coop, now, host);
        Travel(coop, now, host);

        if (coop::IsDue(now, m_nextLog))
        {
            if (m_nextLog && (m_sharedXp || m_gotXp || m_reputationsSet))
                LOG_INFO("Progress: shared %d XP of ours, got %d XP from others; %u reputations set to the host's",
                    m_sharedXp, m_gotXp, m_reputationsSet);
            m_nextLog = now + 60000;
            m_sharedXp = m_gotXp = 0;
            m_reputationsSet = 0;
        }
    }

    void Progress::Travel(coop::Coop& coop, uint32_t now, bool host)
    {
        if (m_mapMarkers.empty())
        {
            m_mapMarkers = MapMarkers();
            LOG_INFO("Progress: %zu map markers", m_mapMarkers.size());
        }
        std::vector<uint32_t> heard = coop.TakeTravelMarkers();
        std::vector<uint32_t> relay;
        std::string lastName;
        uint32_t unlocked = 0;
        for (uint32_t id : heard)
        {
            TESObjectREFR* marker = LookupRef(id);
            if (!marker || std::find(m_mapMarkers.begin(), m_mapMarkers.end(), marker) == m_mapMarkers.end())
                continue;
            if (m_travelWatcher.Look(id))
                relay.push_back(id);
            if (CanTravelTo(marker))
                continue;
            UnlockTravel(marker);
            ++unlocked;
            lastName = MarkerName(marker);
            LOG_INFO("Progress: fast travel to %08X %s unlocked (someone else found it)", id, lastName.c_str());
        }
        if (host && !relay.empty())
            coop.ShareTravelMarkers(relay);
        if (unlocked == 1 && !lastName.empty())
            Notify(("Fast travel unlocked: " + lastName).c_str());
        else if (unlocked > 1)
            Notify(("Co-op: " + std::to_string(unlocked) + " places unlocked for fast travel").c_str());

        if (!coop::IsDue(now, m_nextTravelLook))
            return;
        m_nextTravelLook = now + kTravelLookMs;
        if (host && coop.Joins() != m_travelJoins)
        {
            m_travelJoins = coop.Joins();
            m_travelWatcher.Forget();
        }
        std::vector<uint32_t> mine;
        for (TESObjectREFR* marker : m_mapMarkers)
            if (CanTravelTo(marker) && m_travelWatcher.Look(RefId(marker)))
                mine.push_back(RefId(marker));
        if (mine.empty())
            return;
        coop.ShareTravelMarkers(mine);
        LOG_INFO("Progress: told the %s about %zu places we can fast travel to (%zu known)", host ? "friends" :
            "host", mine.size(), m_travelWatcher.Known());
    }

    void Progress::Reputations(coop::Coop& coop, uint32_t now, bool host)
    {
        if (host)
        {
            if (!coop::IsDue(now, m_nextReputationLook))
                return;
            m_nextReputationLook = now + kReputationLookMs;
            bool all = coop::IsDue(now, m_nextReputationResend) || coop.Joins() != m_joins;
            if (all)
                m_nextReputationResend = now + kReputationResendMs;
            m_joins = coop.Joins();
            for (const auto& [playerId, g] : coop.TakeReputationGains())
                for (const ReputationInfo& r : ReadReputations())
                    if (r.form == g.form)
                    {
                        float fame = std::clamp(r.fame + g.fame, 0.0f, net::kMaxReputation);
                        float infamy = std::clamp(r.infamy + g.infamy, 0.0f, net::kMaxReputation);
                        WriteReputation(r.form, fame, infamy);
                        LOG_INFO("Progress: %s's game changed reputation %08X %s (fame %+.0f, infamy %+.0f); so does "
                            "ours", coop.NameOf(playerId).c_str(), r.form, r.name.c_str(), g.fame, g.infamy);
                        if (!r.name.empty() && g.fame > 0)
                            Notify(("Fame gained with " + r.name).c_str());
                        if (!r.name.empty() && g.infamy > 0)
                            Notify(("Infamy gained with " + r.name).c_str());
                    }
            std::vector<net::Reputation> send;
            for (const ReputationInfo& r : ReadReputations())
            {
                auto sent = m_sentReputations.find(r.form);
                bool changed = sent == m_sentReputations.end() || sent->second != std::make_pair(r.fame, r.infamy);
                if (changed || all)
                {
                    send.push_back({r.form, r.fame < 0 ? 0 : r.fame, r.infamy < 0 ? 0 : r.infamy});
                    m_sentReputations[r.form] = {r.fame, r.infamy};
                }
            }
            if (!send.empty())
                coop.SendReputations(send);
            return;
        }
        std::vector<net::Reputation> heard = coop.TakeReputations();
        bool look = coop::IsDue(now, m_nextReputationLook);
        if (heard.empty() && !look)
            return;
        if (look)
            m_nextReputationLook = now + kReputationLookMs;
        bool quiet = !coop::IsDue(now, m_quietUntil);
        std::vector<ReputationInfo> mine = ReadReputations();
        std::vector<net::ReputationGain> gains;
        for (const ReputationInfo& r : mine)
        {
            net::ReputationGain g{r.form};
            if (m_reputationWatcher.Look(r.form, r.fame, r.infamy, g.fame, g.infamy))
            {
                gains.push_back(g);
                ++m_reputationsShared;
                LOG_INFO("Progress: our game changed reputation %08X %s (fame %+.0f, infamy %+.0f); told the host",
                    r.form, r.name.c_str(), g.fame, g.infamy);
            }
        }
        if (!gains.empty())
            coop.ShareReputationGains(gains);
        for (const net::Reputation& h : heard)
            m_reputationWatcher.Heard(h.form, h.fame, h.infamy);
        for (const net::Reputation& h : heard)
        {
            for (const ReputationInfo& r : mine)
            {
                if (r.form != h.form || (r.fame == h.fame && r.infamy == h.infamy))
                    continue;
                WriteReputation(h.form, h.fame, h.infamy);
                ++m_reputationsSet;
                LOG_DEBUG("Progress: reputation %08X %s set to the host's (fame %.0f -> %.0f, infamy %.0f -> %.0f)",
                    h.form, r.name.c_str(), r.fame, h.fame, r.infamy, h.infamy);
                if (!quiet && !r.name.empty())
                {
                    if (h.fame > r.fame)
                        Notify(("Fame gained with " + r.name).c_str());
                    if (h.infamy > r.infamy)
                        Notify(("Infamy gained with " + r.name).c_str());
                }
            }
        }
    }
}
