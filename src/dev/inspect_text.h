#pragma once

// The inspector's words (CLAUDE.md, DESIGN "See, don't describe"): what an actor really is doing, as log
// lines. Game-independent so it's tested; game/inspector.cpp reads the state from the game.

#include <cstdint>
#include <string>
#include <vector>

namespace dev
{
    constexpr int kAnimLayers = 8;

    struct ActorState
    {
        float x = 0, y = 0, z = 0, heading = 0;  // heading in radians
        uint32_t space = 0;                      // worldspace or interior cell id

        const char* mover = "none";  // kind of mover (game::MoverKindName)
        uint32_t flags = 0;          // movement flags it's moving with
        uint32_t askedFlags = 0;     // NPC: flags handed over to it...
        bool askedOn = false;        // ...and whether they're in use instead of its path

        int lifeState = -1;     // Actor::lifeState (0 alive), -1 not an actor
        int processLevel = -1;  // AI detail: 0 high (nearby) ... 3 low, -1 none
        bool animsKnown = false;
        uint16_t anims[kAnimLayers] = {};  // animation group in each layer
        uint32_t idle = 0;                 // idle animation form it's playing (its animation data's), 0 none
        float walkSpeed = 0, runSpeed = 0;  // its process's cached walk and run speed (0: not cached)
        int lockedControls = -1;            // the player: controls a script locked (DisablePlayerControls flags)
        std::string sequences[kAnimLayers];  // the animation file playing in each layer (its sequence's name)

        int action = -1;  // animation action (JIP LN AnimAction: 2 attack, 14 script animation...), -1 none

        bool aiKnown = false;
        uint32_t ai = 0;  // AI bits, as the inspector's game script packs them (see AiBits)
    };

    // What the inspector's game script packs into one number.
    struct AiBits
    {
        bool weaponOut, sneaking, inCombat, restrained;
        int knocked, sitting;  // GetKnockedState, GetSitting
        int package;           // GetCurrentAIPackage (type), -1 none
        int procedure;         // GetCurrentAIProcedure, -1 none
    };
    AiBits UnpackAi(uint32_t bits);
    uint32_t PackAi(const AiBits& ai);  // (for tests; the game script does the packing)

    using AnimNameFn = std::string (*)(uint16_t groupId);

    std::string MoveFlagNames(uint32_t flags);  // e.g. "Forward Running", "none"
    std::string Describe(const ActorState& s, AnimNameFn animName);  // everything, one line
    // What changed (one line each), leaving out position and heading (see Motion).
    std::vector<std::string> Changes(const ActorState& before, const ActorState& now, AnimNameFn animName);
    // Where it is and how fast it went since `before`, `ms` milliseconds earlier.
    std::string Motion(const ActorState& before, const ActorState& now, uint32_t ms);
}
