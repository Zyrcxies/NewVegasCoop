#pragma once

// An actor's "legs" (the game's ActorMover): the movement flags (coop::moves) that pick its walk, run and
// sneak animations. Every call checks the mover's function table is one we know (addresses.h, game::mover),
// so nothing is ever called through a guessed slot. Only call from the game thread.

#include <cstdint>

class TESObjectREFR;

namespace game
{
    enum class MoverKind { None, Npc, Player, Unknown };  // None: not an actor or no mover

    struct MoverState
    {
        MoverKind kind = MoverKind::None;
        uint32_t flags = 0;       // what it's moving with now
        uint32_t asked = 0;       // NPC: flags handed over with AskMovementFlags
        bool askedOn = false;     // NPC: those flags are in use instead of its path
    };

    // Checks the movers' function tables and the player's mover against the memory map, and logs the result.
    // Until it has passed, AskMovementFlags and ReleaseMovementFlags do nothing. Call once a world is loaded.
    bool CheckMoverMap();

    MoverState ReadMover(TESObjectREFR* ref);
    // NPC: move with these flags instead of following its path, until released. Player: set its flags.
    // False if it's not an actor with a known mover (or the check failed).
    bool AskMovementFlags(TESObjectREFR* ref, uint32_t flags);
    // NPC: like AskMovementFlags, but its AI can't switch them off (a puppet's legs). Call every frame: the hold
    // lapses kHoldMs after the last call, so a forgotten actor goes back to its AI by itself.
    bool HoldMovementFlags(TESObjectREFR* ref, uint32_t flags);
    constexpr uint32_t kHoldMs = 250;
    bool ReleaseMovementFlags(TESObjectREFR* ref);  // NPC: back to following its path (ends a hold)
    // Drops every hold without touching the movers: the world is going away (a save loading), so they may be
    // half torn down.
    void ForgetHolds();

    const char* MoverKindName(MoverKind kind);
}
