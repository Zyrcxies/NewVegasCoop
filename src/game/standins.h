#pragma once

// A character in our world for each other player: a puppet (game::Puppet, DESIGN rule 7) copying what their
// game sends. Downed players' characters lie knocked out.

#include "coop/coop.h"
#include "game/puppet.h"

#include <cstdint>
#include <map>
#include <vector>

class TESObjectREFR;

namespace game
{
    class StandIns
    {
    public:
        // Every frame while a game is loaded. myPose: where our own character is.
        void Update(const coop::Coop& coop, uint32_t now, const coop::Pose& myPose);
        void RemoveAll();  // delete them from the world
        void Forget();     // the world was unloaded (they're gone with it)
        std::vector<uint32_t> RefIds() const;  // their characters, for cleaning up after loading a save
        TESObjectREFR* Character(uint8_t playerId) const;  // null if they have none right now
        // That player shot, swung or threw (release: their throw left their hand): their character does too.
        void Attack(uint8_t playerId, uint8_t shots, bool release = false);
        bool IsStandIn(const TESObjectREFR* ref) const { return PlayerIdOf(ref) >= 0; }
        int PlayerIdOf(const TESObjectREFR* ref) const;  // whose stand-in it is; -1: not a stand-in

    private:
        struct Entry
        {
            TESObjectREFR* ref = nullptr;  // the character (checked against refId each frame: see Live)
            uint32_t refId = 0;
            Puppet puppet;
            uint32_t gearChanges = 0;
            bool hasGear = false;
            bool downed = false;  // lying down because its player is downed
            uint32_t knockedAt = 0;
            uint32_t nextCalmAt = 0;  // when to check it isn't fighting
            // Created, but the game is still loading its body in the background: until then we leave it alone
            // (changing it meanwhile can damage the game's memory and crash it later).
            bool loading = true;
            uint32_t spawnedAt = 0;
            uint32_t retryAt = 0;  // when spawning failed, try again then
            uint32_t nextBringAt = 0;  // moved over to us at most this often
            uint32_t looksSeen = 0;    // coop's LooksChanges when its looks were last made their player's (0: never)
            uint32_t nextLooksAt = 0;  // looks made again at most this often (changes meanwhile wait, the latest kept)
        };
        bool Spawn(const coop::RemotePlayer& remote, Entry& e, uint32_t now, const net::PlayerLooks* looks);
        // Its character, if the game still has it (one left in a part of the world that was unloaded can be
        // deleted; NPCs are looked up each frame for the same reason).
        static TESObjectREFR* Live(const Entry& e);

        std::map<uint8_t, Entry> m_entries;  // by player id
        std::map<uint8_t, bool> m_tooFar;    // players further off than we show (logged when it changes)
    };
}
