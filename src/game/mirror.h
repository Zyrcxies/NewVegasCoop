#pragma once

// Puppet lab (DESIGN step 1): an actor copies our player, a little to the east and 100 ms behind (as the
// network will deliver a friend), driven like a friend's puppet (game::Puppet). Logs how well it keeps up.
// Developer tool: "coop mirror". Only call from the game thread.

#include "coop/smoothing.h"
#include "game/puppet.h"

#include <cstdint>

class TESObjectREFR;

namespace game
{
    class Mirror
    {
    public:
        void Start(TESObjectREFR* actor);
        void Stop();
        uint32_t Copier() const { return m_refId; }  // ref id of the actor copying us, 0 if none
        void Update(uint32_t now);                    // every frame
        void Attack(uint8_t shots, bool release = false);  // we attacked (game::TakeMyAttacks)

    private:
        uint32_t m_refId = 0;
        coop::Smoother m_smoother;
        Puppet m_puppet;
        uint32_t m_nextFeed = 0, m_nextGear = 0;
        bool m_madeCompanion = false;
    };
}
