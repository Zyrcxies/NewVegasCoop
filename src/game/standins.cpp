#include "game/standins.h"

#include "game/game.h"
#include "coop/clock.h"
#include "log.h"

#include <algorithm>
#include <cmath>


namespace game
{
    namespace
    {
        constexpr uint32_t kSpawnRetryMs = 5000;
        // Downed players lie in pain (Alex, 2026-10-30, from four the lab showed: base game, Fallout - Meshes.bsa).
        const std::string kDownedAnim = "Characters\\_Male\\IdleAnims\\dlcpittlayingonfloorinpain.kf";
        // Personal space: a friend's character stands at least this far (game units, about a metre) from ours and from
        // each other friend's, where they'd be closer (a character is about 40 across).
        constexpr float kPersonalSpace = 64;
        constexpr uint32_t kHoldSeatedMs = 100;  // a sitting friend's character put back on their seat this often
        constexpr uint32_t kAnimCheckMs = 500;   // and checked to really play their animation
        constexpr uint32_t kStatsEveryMs = 10000;
        constexpr uint32_t kCalmEveryMs = 1000;    // check it isn't fighting (its combat is off; just in case)
        constexpr uint32_t kMaxLoadingMs = 5000;  // carry on anyway if its body takes longer than this
        // Further from us than the loaded cells reach (less this), they're not shown: the world there isn't loaded,
        // and moving a character there every frame crashed the game (Oct 2026).
        constexpr float kShowInside = 192;
        constexpr uint32_t kBringEveryMs = 1000;
    }

    void StandIns::Update(const coop::Coop& coop, uint32_t now, const coop::Pose& myPose, bool cutscene)
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
            ShowOnMap(it->second, false);
            Remove(Live(it->second));
            it = m_entries.erase(it);
        }

        for (auto& [id, e] : m_entries)
            if (e.ref && !Live(e))
            {
                LOG_WARN("Player %u's character %08X is gone from the game (left in an unloaded place?); making a "
                    "new one", id, e.refId);
                ShowOnMap(e, false);
                e = Entry{};
            }

        std::vector<std::pair<float, float>> placed{{myPose.x, myPose.y}};  // where characters stand (us first)
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
            {
                if (auto it = m_entries.find(id); it != m_entries.end())
                    ShowOnMap(it->second, false);  // (left where it was last shown)
                continue;  // no position yet, or somewhere else (on their way here, or we're on our way there)
            }

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
            ShowOnMap(e, true);
            SetOnCompass(e.ref, true);  // (downed too: easier to find to revive)

            // Collision off only while it sits or lies downed (held in place there: without collision a character falls
            // through the floor, as friends' did going through doors: lab, 2026-10-30). Unseen in a cutscene (Alex,
            // 2026-10-30: they blocked the view).
            bool ghost = e.downed || !e.anim.empty();
            if (ghost != e.ghost)
            {
                SetCollision(e.ref, !ghost);
                e.ghost = ghost;
                LOG_DEBUG("%s's character %s", remote.name.c_str(), ghost ? "goes through everyone" : "collides again");
            }
            if (cutscene != e.hidden)
            {
                SetActorAlpha(e.ref, cutscene ? 0.0f : 1.0f);
                e.hidden = cutscene;
                LOG_INFO("%s's character is %s (cutscene)", remote.name.c_str(), cutscene ? "hidden" : "shown again");
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

            // Downed: lying on the ground in pain (Alex, 2026-10-30; it was the game's knock-out, a ragdoll knocked out
            // again whenever it got up, which glitched). Essential meanwhile, so nothing in our game kills it.
            if (remote.Downed() != e.downed)
            {
                e.downed = remote.Downed();
                if (e.downed)
                {
                    SetEssential(e.ref, true);
                    Notify((remote.name + " is down! Hold the activate key next to them to revive").c_str());
                }
                else
                {
                    ChangeHealth(e.ref, MaxHealth(e.ref) - Health(e.ref));
                    SetEssential(e.ref, false);  // (their own kind of character: no one else is affected)
                }
                LOG_INFO("%s's character is %s", remote.name.c_str(), e.downed ? "down" : "back up");
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
            // Animations from mods (Alex, 2026-10-30: Enhanced Movement's sprint, or any kNVSE animation mod): the files
            // their character plays from mods, applied to ours (kNVSE overrides: its own animation system then plays them,
            // so walking still matches its speed), taken off when they stop.
            auto mod = coop.ModAnims().find(id);
            const std::vector<std::string> noAnims;
            const std::vector<std::string>& wantAnims = mod != coop.ModAnims().end() ? mod->second : noAnims;
            if (wantAnims != e.modAnims)
            {
                for (const std::string& path : e.modAnims)
                    if (std::find(wantAnims.begin(), wantAnims.end(), path) == wantAnims.end())
                        SetAnimOverride(e.ref, path, false);
                for (const std::string& path : wantAnims)
                    if (std::find(e.modAnims.begin(), e.modAnims.end(), path) == e.modAnims.end())
                        SetAnimOverride(e.ref, path, true);
                LOG_DEBUG("%s's character: %zu animations from mods (was %zu)", remote.name.c_str(), wantAnims.size(),
                    e.modAnims.size());
                e.modAnims = wantAnims;
            }
            if (Furniture(coop, id, e, pose, now))
                continue;
            // Personal space (Alex, 2026-10-30: arriving together through a door, characters pushed each other): where
            // they'd stand on top of us or of another friend's character, it stands just beside, until they move off.
            coop::Pose target = pose;
            for (size_t k = 0; k < placed.size(); ++k)
            {
                float dx = target.x - placed[k].first, dy = target.y - placed[k].second;
                float apart = std::hypot(dx, dy);
                if (apart >= kPersonalSpace)
                    continue;
                if (apart < 1)  // right on the spot: to our right, then round from there for each one more
                {
                    float angle = myPose.heading + 1.5707963f * static_cast<float>(1 + placed.size());
                    dx = std::sin(angle);
                    dy = std::cos(angle);
                    apart = 1;
                }
                target.x = placed[k].first + dx / apart * kPersonalSpace;
                target.y = placed[k].second + dy / apart * kPersonalSpace;
            }
            placed.push_back({target.x, target.y});
            coop::Pose latest;
            e.puppet.Update(e.ref, target, now, remote.motion.Newest(latest) ? &latest : nullptr);
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
        {
            ShowOnMap(e, false);
            Remove(Live(e));
        }
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

    bool StandIns::Furniture(const coop::Coop& coop, uint8_t id, Entry& e, const coop::Pose& pose, uint32_t now)
    {
        auto it = coop.Anims().find(id);
        const std::string want = e.downed ? kDownedAnim : it != coop.Anims().end() ? it->second : std::string();
        if (want != e.anim)
        {
            if (!e.anim.empty())
                StopAnimFile(e.ref, e.anim);
            // (Its collision is off while it sits, by Update: a seated player is put lower than standing, in the chair,
            // and the chair pushed a character placed there up onto its seat: lab, 2026-10-30.)
            if (!want.empty())
            {
                // Not steered while it sits: steered, its legs pushed it up out of the chair (lab, 2026-10-30); a
                // still lab dummy stayed in it.
                e.puppet.Release(e.ref);
                Move(e.ref, pose);
                PlayAnimFile(e.ref, want);
            }
            LOG_INFO("%s's character %s %s", coop.NameOf(id).c_str(), want.empty() ? "stops" : "plays",
                want.empty() ? e.anim.c_str() : want.c_str());
            e.anim = want;
            e.nextHoldAt = now;
            e.nextAnimCheckAt = now + kAnimCheckMs;
        }
        // Started just as its legs were let go, it sometimes didn't play (lab, 2026-10-30; played again, it did).
        if (!e.anim.empty() && coop::IsDue(now, e.nextAnimCheckAt))
        {
            e.nextAnimCheckAt = now + kAnimCheckMs;
            if (!IsPlayingAnimFile(e.ref, e.anim))
            {
                LOG_DEBUG("%s's character isn't playing %s; again", coop.NameOf(id).c_str(), e.anim.c_str());
                PlayAnimFile(e.ref, e.anim);
            }
        }
        // Put on their seat now and then (just after a move, the character floated up out of the chair: lab).
        if (!e.anim.empty() && coop::IsDue(now, e.nextHoldAt))
        {
            e.nextHoldAt = now + kHoldSeatedMs;
            Move(e.ref, pose);
        }
        return !e.anim.empty();
    }

    void StandIns::ShowOnMap(Entry& e, bool on)
    {
        if (e.onMap == on || !e.ref)
            return;
        SetRefMapMarker(e.ref, on);
        if (!on)
            SetOnCompass(e.ref, false);
        e.onMap = on;
        LOG_DEBUG("Map: character %08X %s", e.refId, on ? "shown" : "no longer shown");
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
