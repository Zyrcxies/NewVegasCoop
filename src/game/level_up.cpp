// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#include "game/level_up.h"

#include "coop/balance.h"
#include "game/addresses.h"
#include "log.h"

#include <windows.h>
#include <cstdint>

namespace game
{
    namespace
    {
        using SkillPointsFn = int(__cdecl*)(void* actorValues);
        SkillPointsFn g_before = nullptr;
        const coop::Balance* g_balance = nullptr;

        int __cdecl SkillPoints(void* actorValues)
        {
            int points = g_before(actorValues);
            if (!g_balance)
                return points;
            int given = coop::FewerSkillPoints(points, *g_balance);
            LOG_INFO("Balance: a level up gives %d skill points before perks (the game's %d)", given, points);
            return given;
        }
    }

    void TakeOverLevelUpSkillPoints()
    {
        auto* at = reinterpret_cast<uint8_t*>(addr::kLevelUpSkillPointsCall);
        if (*at != 0xE8)
        {
            LOG_WARN("Level up: %08X isn't a call (%02X); left alone, so co-op balanced can't give fewer skill points",
                addr::kLevelUpSkillPointsCall, *at);
            return;
        }
        uintptr_t target = addr::kLevelUpSkillPointsCall + 5 + *reinterpret_cast<int32_t*>(at + 1);
        g_before = reinterpret_cast<SkillPointsFn>(target);
        DWORD old;
        VirtualProtect(at + 1, 4, PAGE_EXECUTE_READWRITE, &old);
        *reinterpret_cast<int32_t*>(at + 1) =
            static_cast<int32_t>(reinterpret_cast<uintptr_t>(&SkillPoints) - (addr::kLevelUpSkillPointsCall + 5));
        VirtualProtect(at + 1, 4, old, &old);
        FlushInstructionCache(GetCurrentProcess(), at, 5);
        LOG_INFO("Level up: skill points counted through us (%s first)",
            target == addr::kLevelUpSkillPoints ? "the game's count" : "another plugin's");
    }

    void SetLevelUpBalance(const coop::Balance* balance)
    {
        g_balance = balance;
    }
}
