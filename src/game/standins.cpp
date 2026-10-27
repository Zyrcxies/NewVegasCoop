#include "game/standins.h"

#include "game/game.h"
#include "coop/clock.h"
#include "log.h"

#include <cmath>


namespace game
{
    namespace
    {
        constexpr uint32_t kSpawnRetryMs = 5000;
        constexpr uint32_t kStatsEveryMs = 10000;
        constexpr uint32_t kCalmEveryMs = 1000;    // check it isn't fighting (its combat is off; just in case)
        constexpr uint32_t kKnockAgainMs = 1000;  // got up while still downed: knock out again after this
        constexpr uint32_t kMaxLoadingMs = 5000;  // carry on anyway if its body takes longer than this
        // Further from us than the loaded cells reach (less this), they're not shown: the world there isn't loaded,
        // and moving a character there every frame crashed the game (Oct 2026).
        constexpr float kShowInside = 192;
        constexpr uint32_t kBringEveryMs = 1000;
    }

    void StandIns::Update(const coop::Coop& coop, uint32_t now, const coop::Pose& myPose)
    {
        const auto& remotes = coop.Remotes();

        // Players who left. Not while a menu is open: deleting someone you're talking to crashes the game.
        for (auto it = m_entries.begin(); it != m_entries.end();)
        {
            if (remotes.count(it->first) || InMenu())
            {
                ++it;
                continue;
            }
            Remove(Live(it->second));
            it = m_entries.erase(it);
        }

        for (auto& [id, e] : m_entries)
            if (e.ref && !Live(e))
            {
                LOG_WARN("Player %u's character %08X is gone from the game (left in an unloaded place?); making a "
                    "new one", id, e.refId);
                e = Entry{};
            }

        for (auto& [id, remote] : remotes)
        {
            coop::Pose pose;
            bool known = remote.motion.Sample(now, pose);
            float apart = known ? std::hypot(pose.x - myPose.x, pose.y - myPose.y) : 0;
            bool tooFar = known && pose.space == myPose.space && apart > LoadedReach() - kShowInside;
            if (tooFar != m_tooFar[id])
            {
                m_tooFar[id] = tooFar;
                LOG_INFO("%s is %s (%.0f units away; shown within %.0f)", remote.name.c_str(), tooFar ?
                    "too far away to show" : "near enough to show again", apart, LoadedReach() - kShowInside);
            }
            if (!known || pose.space != myPose.space || tooFar)
                continue;  // no position yet, or somewhere else (on their way here, or we're on our way there)

            Entry& e = m_entries[id];
            auto looks = coop.Looks().find(id);
            if (!e.ref)
            {
                const net::PlayerLooks* with = looks != coop.Looks().end() ? &looks->second : nullptr;
                if (!Spawn(remote, e, now, with))
                    continue;
                if (with)
                    e.looksSeen = coop.LooksChanges();  // (made with them; told here, not by the clock: lab
                                                        // 2026-10-19, looks 10 ms later were taken for these)
            }
            if (e.loading)
            {
                bool loaded = Is3DLoaded(e.ref);
                if (!loaded && now - e.spawnedAt < kMaxLoadingMs)
                    continue;
                e.loading = false;
                LOG_DEBUG("%s's character %s after %u ms", remote.name.c_str(),
                    loaded ? "is loaded" : "still isn't loaded; carrying on", now - e.spawnedAt);
                MakeCompanion(e.ref);
            }

            // Their face, hair and the rest (heard after it was made, or changed since: a barber, the looks menu).
            if (e.looksSeen != coop.LooksChanges() && coop::IsDue(now, e.nextLooksAt))
            {
                if (looks != coop.Looks().end())
                {
                    e.looksSeen = coop.LooksChanges();
                    e.nextLooksAt = now + 2000;  // (a game sending its looks over and over can't keep remaking heads)
                    if (!ApplyLooks(e.ref, looks->second))
                        LOG_WARN("Looks: couldn't make %s's character look like them", remote.name.c_str());
                }
            }

            if (!e.hasGear || e.gearChanges != remote.gearChanges)
            {
                e.puppet.SetGear(e.ref, remote.gear);
                e.hasGear = true;
                e.gearChanges = remote.gearChanges;
            }
            if (coop::IsDue(now, e.nextCalmAt))
            {
                e.nextCalmAt = now + kCalmEveryMs;
                StopFighting(e.ref);
            }

            // Downed: the game's own knock-out (essential, below 0 health) makes them fall and lie there.
            if (remote.Downed() != e.downed)
            {
                e.downed = remote.Downed();
                if (e.downed)
                {
                    e.puppet.Release(e.ref);
                    SetEssential(e.ref, true);
                    KnockOut(e.ref);
                    e.knockedAt = now;
                    Notify((remote.name + " is down! Hold the activate key next to them to revive").c_str());
                }
                else
                {
                    WakeUp(e.ref);
                    ChangeHealth(e.ref, MaxHealth(e.ref) - Health(e.ref));
                    SetEssential(e.ref, false);  // (their own kind of character: no one else is affected)
                }
                LOG_INFO("%s's character is %s", remote.name.c_str(), e.downed ? "down" : "back up");
            }
            if (e.downed)
            {
                if (!IsKnockedOut(e.ref) && now - e.knockedAt >= kKnockAgainMs)
                {
                    LOG_DEBUG("%s's character got up after %.1f s; knocking out again", remote.name.c_str(),
                        (now - e.knockedAt) / 1000.f);
                    KnockOut(e.ref);
                    e.knockedAt = now;
                }
                continue;
            }

            // Another place, or far away in this one (fast travel): a proper move to us first, so it changes
            // cells, then onto their spot.
            coop::Pose current;
            if (!GetPose(e.ref, current) || current.space != pose.space ||
                std::hypot(current.x - pose.x, current.y - pose.y) > coop::kTeleportDistance)
            {
                if (!coop::IsDue(now, e.nextBringAt))
                    continue;
                e.nextBringAt = now + kBringEveryMs;
                BringToPlayer(e.ref);
                Move(e.ref, pose);
                LOG_INFO("Brought %s's character to this place", remote.name.c_str());
                continue;
            }
            e.puppet.Update(e.ref, pose, now);
            SetPitch(e.ref, pose.pitch);  // where they look, up or down (step 8.7)
        }
    }

    bool StandIns::Spawn(const coop::RemotePlayer& remote, Entry& e, uint32_t now, const net::PlayerLooks* looks)
    {
        if (!coop::IsDue(now, e.retryAt))
            return false;
        e.ref = SpawnStandIn(remote.name.c_str(), looks);
        if (!e.ref)
        {
            LOG_ERROR("Couldn't create a character for %s, retrying in %u s", remote.name.c_str(), kSpawnRetryMs / 1000);
            e.retryAt = now + kSpawnRetryMs;
            return false;
        }
        e.refId = RefId(e.ref);
        LOG_INFO("Created %s's character (ref %08X)", remote.name.c_str(), e.refId);
        // It stays where it appeared (next to us) until loaded; then Update moves it to them. (Its kind of
        // character comes with no items: it wears the player's gear.)
        e.puppet.Start(remote.name + "'s character", kStatsEveryMs);
        e.hasGear = false;
        e.downed = false;
        e.loading = true;
        e.spawnedAt = now;
        return true;
    }

    void StandIns::RemoveAll()
    {
        if (m_entries.empty() || InMenu())
            return;  // (see Update)
        for (auto& [_, e] : m_entries)
            Remove(Live(e));
        m_entries.clear();
    }

    void StandIns::Forget()
    {
        m_entries.clear();
    }

    std::vector<uint32_t> StandIns::RefIds() const
    {
        std::vector<uint32_t> ids;
        for (auto& [_, e] : m_entries)
        {
            if (e.refId)
                ids.push_back(e.refId);
        }
        return ids;
    }

    TESObjectREFR* StandIns::Live(const Entry& e)
    {
        return e.ref && LookupRef(e.refId) == e.ref ? e.ref : nullptr;
    }

    void StandIns::Attack(uint8_t playerId, uint8_t shots, bool release)
    {
        auto it = m_entries.find(playerId);
        if (it != m_entries.end() && Live(it->second) && !it->second.loading && !it->second.downed)
            it->second.puppet.Attack(it->second.ref, shots, release);
    }

    TESObjectREFR* StandIns::Character(uint8_t playerId) const
    {
        auto it = m_entries.find(playerId);
        return it == m_entries.end() ? nullptr : Live(it->second);
    }

    int StandIns::PlayerIdOf(const TESObjectREFR* ref) const
    {
        for (auto& [id, e] : m_entries)
            if (ref && e.ref == ref)
                return id;
        return -1;
    }
}
