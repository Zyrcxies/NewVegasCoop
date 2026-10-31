// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dev
{
    constexpr int kAnimLayers = 8;

    struct ActorState
    {
        float x = 0, y = 0, z = 0, heading = 0;
        uint32_t space = 0;

        const char* mover = "none";
        uint32_t flags = 0;
        uint32_t askedFlags = 0;
        bool askedOn = false;

        int lifeState = -1;
        int processLevel = -1;
        bool animsKnown = false;
        uint16_t anims[kAnimLayers] = {};
        uint32_t idle = 0;
        float walkSpeed = 0, runSpeed = 0;
        int lockedControls = -1;
        std::string sequences[kAnimLayers];

        int action = -1;

        bool aiKnown = false;
        uint32_t ai = 0;
    };

    struct AiBits
    {
        bool weaponOut, sneaking, inCombat, restrained;
        int knocked, sitting;
        int package;
        int procedure;
    };
    AiBits UnpackAi(uint32_t bits);
    uint32_t PackAi(const AiBits& ai);

    using AnimNameFn = std::string (*)(uint16_t groupId);

    std::string MoveFlagNames(uint32_t flags);
    std::string Describe(const ActorState& s, AnimNameFn animName);
    std::vector<std::string> Changes(const ActorState& before, const ActorState& now, AnimNameFn animName);
    std::string Motion(const ActorState& before, const ActorState& now, uint32_t ms);
}
