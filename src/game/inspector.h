#pragma once

// The inspector (DESIGN "See, don't describe"): follows one actor and logs what it really is doing (movement
// flags, animations, AI) whenever that changes, and where it is and how fast it moves twice a second.
// Developer tool: turned on with the console command "coop inspect". Only call from the game thread.

#include "dev/inspect_text.h"

#include <cstdint>

class TESObjectREFR;

namespace game
{
    bool ReadActorState(TESObjectREFR* ref, dev::ActorState& out);  // false if it isn't an actor
    std::string AnimGroupName(uint16_t groupId);
    // In a jump's take-off or air time (animation layer 1 is JumpStart... or JumpLoop...), not landing.
    bool IsJumping(TESObjectREFR* ref);

    class Inspector
    {
    public:
        // Start following this actor (logs everything about it now); null stops.
        bool Watch(TESObjectREFR* ref);
        uint32_t Watching() const { return m_refId; }  // its ref id, 0 if none
        void Update(uint32_t now);  // every frame
        void Forget();              // the world is going away (loading)

    private:
        uint32_t m_refId = 0;
        dev::ActorState m_last, m_lastMotion;
        uint32_t m_lastMotionAt = 0, m_nextAi = 0, m_nextMotion = 0;
        uint32_t m_secondStart = 0, m_linesThisSecond = 0;
        bool m_lost = false;
    };
}
