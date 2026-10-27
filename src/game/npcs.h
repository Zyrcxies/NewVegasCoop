#pragma once

// The host's NPCs on friends' games (DESIGN rule 5, build step 5). The host shares its NPCs near the group
// (where they are, health, death, idle, sitting, weapon) and their shots and reloads; on a friend's game each
// copy is a puppet (game::Puppet) copying it, with its fighting off and a harmless copy of its weapon. NPCs the
// host's game made while playing (FF......) aren't in a friend's game: it makes its own copy of each (step 5.4).

#include "coop/coop.h"
#include "coop/shot_counter.h"
#include "game/game.h"
#include "game/puppet.h"

#include <cstdint>
#include <map>
#include <set>
#include <vector>

class TESObjectREFR;

namespace game
{
    class StandIns;

    class Npcs
    {
    public:
        // Host, every frame: tells coop about our NPCs near us and their attacks (looked at a few times a second).
        void Share(coop::Coop& coop, uint32_t now, const coop::Pose& myPose, const StandIns& standIns);
        // Friend, every frame: our copies of the host's NPCs copy them (moves, health, death, idles).
        void Update(const coop::Coop& coop, uint32_t now, const coop::Pose& myPose, const StandIns& standIns);
        // Friend: the host's NPC attacked (shots, maybe 0) and/or started reloading; our copy does too (harmlessly).
        void Attack(uint32_t ref, uint8_t shots, bool reload, bool release = false);
        // Friend, every frame (looks once a second): our copies that disagree with the host's NPCs (alive or dead,
        // where, health) go to the desync detector (DESIGN rule 8).
        void CheckDesyncs(coop::Coop& coop, uint32_t now, const coop::Pose& myPose);
        void ReleaseAll();  // back to their own AI (co-op ended); copies we made are deleted
        void Forget();      // the world was unloaded
        std::vector<uint32_t> MadeRefIds() const;  // friend: copies we made (a save keeps them: delete on load)
        // Lab (coop copysteps N): which steps a copy goes through (all by default): 1 stripped of its own outfit,
        // 2 dressed like the host's, 4 given its weapon, 8 its fighting switched off. To find the one that crashes.
        static void LabCopySteps(uint32_t steps) { s_labSteps = steps; }
        // Friend: the host's id for one of our refs (our copy of one its game made while playing has an id of our
        // own); 0 for one made while playing in our game only.
        uint32_t HostRefOf(uint32_t ourRef) const;
        // Friend: our actor for one of the host's refs (see Find), or null.
        TESObjectREFR* OurRefOf(uint32_t hostRef) const { return Find(hostRef); }

    private:
        struct Entry
        {
            Puppet puppet;
            uint32_t nextBringAt = 0;
            float hostHealth = -1;     // the host's health we last matched (-1 not yet)
            float ownMaxHealth = 0;    // its own max health before we matched the host's (0: unchanged)
            uint32_t idle = 0;         // the host's idle we last played
            uint32_t weapon = 0;       // the host's weapon we last matched
            uint32_t gaveWeapon = 0;   // a weapon we gave it (taken back on release)
            uint32_t nextWeaponCheck = 0;
            uint32_t weaponGifts = 0;  // times we gave it the host's current weapon
            uint32_t allGifts = 0;     // weapons we gave it in all
            uint32_t nextSitCheck = 0;
            uint32_t sittingSince = 0;  // sitting here but not on the host, since (0: isn't)
        };
        void UpdateCopies(const coop::Coop& coop, uint32_t now, const coop::Pose& myPose, const StandIns& standIns);
        // Our actor for the host's ref: the same ref, or for one made while playing our own copy (never the host's
        // id looked up here: in our game it's something else, or nothing). Null if there's none.
        TESObjectREFR* Find(uint32_t ref) const;
        TESObjectREFR* Make(uint32_t ref, const coop::RemoteNpc& npc, const coop::Pose& pose, uint32_t now);
        void Unmake(uint32_t ref);  // release and delete our copy of one made while playing
        void Release(uint32_t ref);
        void LogStats(uint32_t now, size_t shared);
        uint32_t m_nextDesyncCheck = 0, m_nextDesyncLog = 0, m_desyncLooks = 0, m_desyncSeen = 0;

        std::map<uint32_t, Entry> m_entries;  // friend: copies we're steering, by ref id
        std::vector<TESObjectREFR*> m_nearby;  // (kept to reuse its memory)
        struct Watched  // host: a shared NPC at the last look
        {
            int action = -1;
            GunState gun;
            coop::ShotCounter shotCounter;
            uint32_t nextGearAt = 0;
            uint32_t nextGunCheck = 0;   // its weapon is away: when to look at its gun again
            bool isGun = false;          // it held a gun at the last look at its gun
            bool weaponWasOut = false;
            uint16_t limbs = 0;          // dead: body parts gone (at the last look at them)
            uint32_t nextLimbCheck = 0;
            uint32_t diedAt = 0;         // when we first saw it dead (0: alive)
            uint8_t pile = 0;            // dead: turned to goo or ash (net::kPile...; looked for in its first 4 s)
        };
        // Friend: our copies of the host's NPCs made while playing, by the host's ref.
        struct Made
        {
            uint32_t localId = 0;      // our copy's ref id
            uint32_t gearChanges = 0;  // the host's gear we last dressed it in (RemoteNpc::gearChanges)
            uint32_t loadedAt = 0;     // when we first saw its body loaded (0: not yet)
        };
        std::map<uint32_t, Made> m_made;
        static inline uint32_t s_labSteps = 15;
        uint32_t m_nextMakeAt = 0;
        uint32_t m_madeCount = 0, m_unmadeCount = 0, m_dressed = 0;  // since the last stats line
        std::map<uint32_t, Watched> m_watched;
        struct Killed { uint32_t at = 0; uint16_t limbs = 0; uint8_t pile = 0; };
        std::map<uint32_t, Killed> m_killed;       // friend: copies we killed because the host's died (and limbs cut)
        std::map<uint32_t, uint32_t> m_nextRevive; // friend: copies dead only here, when to try bringing them back
        std::map<uint32_t, uint32_t> m_localDeaths;  // friend: copies dead here, when (looked at for goo or ash)
        std::set<uint32_t> m_piled;                // friend: copies already a pile here (ours or the host's)
        uint32_t m_nextShare = 0;
        uint32_t m_nextStats = 0;
        uint32_t m_brought = 0;     // since the last stats line
        double m_shareMs = 0;       // host: time spent looking, since the last stats line
        uint32_t m_shares = 0;
        double m_updateMs = 0;      // friend: time spent copying, since the last stats line
        uint32_t m_updates = 0;
        // Since the last stats line: host, attacks shared; friend, attacks shown, not shown (no copy or weapon
        // away), copies killed, health matched, idles played, copies dead here but alive on the host.
        uint32_t m_attacks = 0, m_attacksShown = 0, m_attacksNotShown = 0, m_kills = 0, m_healthSets = 0;
        uint32_t m_idles = 0, m_deadHere = 0, m_weapons = 0, m_reloads = 0, m_stoodUp = 0, m_reloadsShown = 0;
        bool m_warnedFull = false;
        bool m_foundNone = true;  // host: found no one to share last time
    };
}
