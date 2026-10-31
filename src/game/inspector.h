// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include "dev/inspect_text.h"

#include <cstdint>

class TESObjectREFR;

namespace game
{
    bool ReadActorState(TESObjectREFR* ref, dev::ActorState& out);
    std::string AnimGroupName(uint16_t groupId);
    bool IsJumping(TESObjectREFR* ref);

    class Inspector
    {
    public:
        bool Watch(TESObjectREFR* ref);
        uint32_t Watching() const { return m_refId; }
        void Update(uint32_t now);
        void Forget();

    private:
        uint32_t m_refId = 0;
        dev::ActorState m_last, m_lastMotion;
        uint32_t m_lastMotionAt = 0, m_nextAi = 0, m_nextMotion = 0;
        uint32_t m_secondStart = 0, m_linesThisSecond = 0;
        bool m_lost = false;
    };
}
