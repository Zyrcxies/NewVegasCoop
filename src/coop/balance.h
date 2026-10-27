#pragma once

// Co-op balanced (REBALANCE.md step 2: damage, healing and resources; JSawyer as the base, Alex, October 2026). The
// numbers a game in a Co-op balanced session plays with; Normal leaves the game's own. Game-independent: tested in
// tests/.

#include <cstdint>

namespace coop
{
    struct Balance
    {
        // Damage players take, times (2: the same as JSawyer's halved player health, 50 + END x 10 + 5 a level
        // against the game's 100 + END x 20 + 5 a level, at every Endurance).
        float damageToPlayers = 2.0f;
        // Damage players deal, times (enemies go down a little faster: no bullet sponges).
        float damageByPlayers = 1.25f;
        // Stimpaks heal less and slower (JSawyer: 20): health a second, seconds (the game's: 5 for 6, 30 in all).
        int stimpakPerSecond = 2, stimpakSeconds = 10;
        // Super Stimpaks (the game's: 20 a second for 3, 60 in all).
        int superStimpakPerSecond = 10, superStimpakSeconds = 4;
        // Base carry weight (the game's 150; JSawyer's 50).
        float carryWeightBase = 75;
        // Body parts' damage, times the game's (realism mods: head about 3x, the game's 2x; limbs a little less), for
        // every creature and character, players too.
        float headshotMult = 1.5f, limbMult = 0.85f;
        // Armor matters more (JSawyer direction: medium and heavy armor get Damage Resistance, %), added to its own.
        int mediumArmorDR = 10, heavyArmorDR = 20;
        // Supplies are scarcer (JSawyer direction): loot lists that hand out only food, drink and medicine (Stimpaks,
        // water, meat...) give something this share as often as the game's.
        float suppliesKept = 0.5f;
        // And across the board (Alex, October 2026: ammo and weapons too): loot lists only boxes and vendors use (none
        // that kit out characters, or enemies would go unarmed) give something this share as often; and a generic
        // enemy that dies keeps this share of its ammo (it fought with all of it).
        float boxLootKept = 0.5f;
        float bodyAmmoKept = 0.5f;
        // Slower leveling (REBALANCE.md step 5; JSawyer direction): skill points a level before Intelligence (the
        // game's 11), and kill XP times this (more enemies come with bigger groups).
        int skillPointsBase = 8;
        float killXpMult = 0.75f;
        // Smarter enemies (REBALANCE.md step 4, through each enemy type's combat style): those that use cover take it
        // at least this often (%) and look this much further for it, and step aside this often (%, the game's 50);
        // every one backs off when badly hurt (the style's "flee based on personal survival").
        int takeCoverAtLeast = 75;
        float coverSearchMult = 1.5f;
        int sideStepChance = 75;
    };

    // A loot list's chance of giving nothing (%), when what it gives comes `kept` as often.
    inline uint8_t ScarceChanceNone(uint8_t chanceNone, float kept)
    {
        if (chanceNone >= 100)
            return 100;
        float none = 100 - (100 - chanceNone) * kept;
        return static_cast<uint8_t>(none < 0 ? 0 : none > 100 ? 100 : none + 0.5f);
    }

    // The co-op balanced numbers (one set for now; tuned in play).
    inline const Balance& CoopBalance()
    {
        static const Balance kBalance;
        return kBalance;
    }

    // A friend's game takes the host's damage (Damage messages, friendly fire) itself: times the balance's.
    inline float DamageTaken(float damage, bool balanced)
    {
        return balanced ? damage * CoopBalance().damageToPlayers : damage;
    }
}
