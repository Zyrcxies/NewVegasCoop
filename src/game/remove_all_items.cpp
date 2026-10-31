// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#include "game/remove_all_items.h"

#include "coop/item_stacks.h"
#include "game/addresses.h"
#include "log.h"

#include "nvse/GameObjects.h"

#include <windows.h>
#include <cstdint>

namespace game
{
    namespace
    {
        using TakeOffFn = char(__thiscall*)(void* changes, bool* done, TESForm* item, int32_t count,
            TESObjectREFR* actor, const void* stack, uint32_t left, uint32_t unknown, void* entry, bool* freed);
        TakeOffFn g_before = nullptr;
        int g_told = 0;

        char __fastcall TakeOff(void* changes, void*, bool* done, TESForm* item, int32_t count, TESObjectREFR* actor,
            const void* stack, uint32_t left, uint32_t unknown, void* entry, bool* freed)
        {
            char result = g_before(changes, done, item, count, actor, stack, left, unknown, entry, freed);
            if (stack && entry && freed && !*freed &&
                !coop::HoldsStack(*static_cast<const coop::StackNode* const*>(entry), stack))
            {
                *freed = true;
                if (g_told < 20 && ++g_told)
                    LOG_INFO("RemoveAllItems: %08X's worn %08X merged into another of its stacks when taken off; the "
                        "game had freed it, so it's skipped (it would have read freed memory)", actor ? actor->refID : 0,
                        item ? item->refID : 0);
            }
            return result;
        }
    }

    void FixRemoveAllItemsTakeOff()
    {
        auto* at = reinterpret_cast<uint8_t*>(addr::kRemoveAllTakeOffCall);
        if (*at != 0xE8)
        {
            LOG_WARN("RemoveAllItems: %08X isn't a call (%02X); left alone, so stripping a character can still crash "
                "the game", addr::kRemoveAllTakeOffCall, *at);
            return;
        }
        uintptr_t target = addr::kRemoveAllTakeOffCall + 5 + *reinterpret_cast<int32_t*>(at + 1);
        g_before = reinterpret_cast<TakeOffFn>(target);
        DWORD old;
        VirtualProtect(at + 1, 4, PAGE_EXECUTE_READWRITE, &old);
        *reinterpret_cast<int32_t*>(at + 1) =
            static_cast<int32_t>(reinterpret_cast<uintptr_t>(&TakeOff) - (addr::kRemoveAllTakeOffCall + 5));
        VirtualProtect(at + 1, 4, old, &old);
        FlushInstructionCache(GetCurrentProcess(), at, 5);
        LOG_INFO("RemoveAllItems: taking worn items off goes through us (%s first)",
            target == addr::kTakeOffItem ? "the game's" : "another plugin's");
    }
}
