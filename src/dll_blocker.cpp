#include "dll_blocker.h"

#include "dll_names.h"
#include "log.h"

#include <cstdint>
#include <windows.h>
#include <winternl.h>

namespace
{
    using LdrLoadDll_t = LONG(NTAPI*)(PWSTR, PULONG, PUNICODE_STRING, PVOID*);
    LdrLoadDll_t g_loadDll = nullptr;  // the real LdrLoadDll (just past our jump)
    volatile LONG g_refused = 0;
    constexpr LONG kStatusDllNotFound = static_cast<LONG>(0xC0000135);

    // Every DLL load in the game goes through here, including ones other programs push into it.
    LONG NTAPI LdrLoadDllHook(PWSTR searchPath, PULONG flags, PUNICODE_STRING name, PVOID* module)
    {
        if (name && name->Buffer && IsBlockedDll(name->Buffer, name->Length / sizeof(wchar_t)))
        {
            LONG n = InterlockedIncrement(&g_refused);
            if (n <= 3)
                LOG_INFO("Kept Discord's overlay out of the game (it crashes New Vegas)%s",
                    n == 3 ? "; not logging this again" : "");
            if (module)
                *module = nullptr;
            return kStatusDllNotFound;
        }
        return g_loadDll(searchPath, flags, name, module);
    }
}

void BlockCrashingDlls()
{
    if (GetModuleHandleW(L"DiscordHook.dll"))
    {
        LOG_WARN("Discord's overlay is already in the game; it may crash it (close Discord before starting)");
        return;
    }
    // Windows' hot-patch spot: "mov edi, edi" at the start, with 5 unused bytes before it. We put a jump to
    // our function there; the real one carries on 2 bytes in. (Chains to it: other hooks keep working.)
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto* fn = ntdll ? reinterpret_cast<uint8_t*>(GetProcAddress(ntdll, "LdrLoadDll")) : nullptr;
    if (!fn)
    {
        LOG_WARN("Couldn't find LdrLoadDll; Discord's overlay can't be kept out of the game");
        return;
    }
    bool padding = true;
    for (int i = 1; i <= 5; ++i)
        padding = padding && (fn[-i] == 0x90 || fn[-i] == 0xCC);
    if (fn[0] != 0x8B || fn[1] != 0xFF || !padding)
    {
        LOG_WARN("LdrLoadDll starts %02X %02X (already hooked?); Discord's overlay can't be kept out of the game",
            fn[0], fn[1]);
        return;
    }
    DWORD old;
    if (!VirtualProtect(fn - 5, 7, PAGE_EXECUTE_READWRITE, &old))
        return;
    g_loadDll = reinterpret_cast<LdrLoadDll_t>(fn + 2);
    fn[-5] = 0xE9;  // jmp LdrLoadDllHook
    *reinterpret_cast<int32_t*>(fn - 4) = static_cast<int32_t>(reinterpret_cast<intptr_t>(&LdrLoadDllHook) -
        reinterpret_cast<intptr_t>(fn));
    *reinterpret_cast<volatile uint16_t*>(fn) = 0xF9EB;  // jmp short back to it (one write: no half-done jump)
    VirtualProtect(fn - 5, 7, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn - 5, 7);
    LOG_DEBUG("Watching DLL loads (to keep Discord's overlay out)");
}
