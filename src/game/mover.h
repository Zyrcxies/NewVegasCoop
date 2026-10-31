// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstdint>

class TESObjectREFR;

namespace game
{
    enum class MoverKind { None, Npc, Player, Unknown };

    struct MoverState
    {
        MoverKind kind = MoverKind::None;
        uint32_t flags = 0;
        uint32_t asked = 0;
        bool askedOn = false;
    };

    bool CheckMoverMap();

    MoverState ReadMover(TESObjectREFR* ref);
    bool AskMovementFlags(TESObjectREFR* ref, uint32_t flags);
    bool HoldMovementFlags(TESObjectREFR* ref, uint32_t flags);
    constexpr uint32_t kHoldMs = 250;
    bool ReleaseMovementFlags(TESObjectREFR* ref);
    void ForgetHolds();

    const char* MoverKindName(MoverKind kind);
}
