// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstdint>

namespace coop
{
    struct Balance
    {
        float damageToPlayers = 2.0f;
        float damageByPlayers = 1.25f;
        int stimpakPerSecond = 2, stimpakSeconds = 10;
        int superStimpakPerSecond = 10, superStimpakSeconds = 4;
        float carryWeightBase = 75;
        float headshotMult = 1.5f, limbMult = 0.85f;
        int mediumArmorDR = 10, heavyArmorDR = 20;
        float suppliesKept = 0.5f;
        float boxLootKept = 0.5f;
        float bodyAmmoKept = 0.5f;
        float ammoValueMult = 2.0f;
        float ammoFindKept = 0.5f;
        int skillPointsBase = 7;
        float killXpMult = 0.75f;
        int takeCoverAtLeast = 75;
        float coverSearchMult = 1.5f;
        int sideStepChance = 75;
    };

    inline uint8_t ScarceChanceNone(uint8_t chanceNone, float kept)
    {
        if (chanceNone >= 100)
            return 100;
        float none = 100 - (100 - chanceNone) * kept;
        return static_cast<uint8_t>(none < 0 ? 0 : none > 100 ? 100 : none + 0.5f);
    }

    inline uint32_t AmmoValue(uint32_t value, const Balance& b)
    {
        return static_cast<uint32_t>(static_cast<float>(value) * b.ammoValueMult + 0.5f);
    }
    inline int32_t AmmoFound(int32_t count, const Balance& b)
    {
        if (count <= 1)
            return count;
        int32_t n = static_cast<int32_t>(static_cast<float>(count) * b.ammoFindKept + 0.5f);
        return n < 1 ? 1 : n;
    }

    constexpr int kGameSkillPointsBase = 10;
    inline int FewerSkillPoints(int gamePoints, const Balance& b)
    {
        int points = gamePoints - (kGameSkillPointsBase - b.skillPointsBase);
        return points > 0 ? points : 0;
    }

    inline const Balance& CoopBalance()
    {
        static const Balance kBalance;
        return kBalance;
    }

    inline float DamageTaken(float damage, bool balanced)
    {
        return balanced ? damage * CoopBalance().damageToPlayers : damage;
    }
}
