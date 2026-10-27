#include "dev/second_copy.h"

#include "log.h"

#include <cstdio>
#include <cstring>
#include <intrin.h>
#include <windows.h>

// The game refuses to start twice by creating a lock named FalloutNVGameMutex.
// Our plugin loads before that check, so we swap the game's CreateMutexA/OpenMutexA
// (in its import table) for versions that give that one lock a per-process name.
// If the lock exists it then looks for the first game's window with FindWindowA, focuses it
// and quits, so that lookup is made to find nothing.

namespace
{
    constexpr const char* kGameMutex = "FalloutNVGameMutex";

    using CreateMutexA_t = HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR);
    using OpenMutexA_t = HANDLE(WINAPI*)(DWORD, BOOL, LPCSTR);
    CreateMutexA_t g_createMutex = nullptr;
    OpenMutexA_t g_openMutex = nullptr;
    using FindWindowA_t = HWND(WINAPI*)(LPCSTR, LPCSTR);
    FindWindowA_t g_findWindow = nullptr;

    LPCSTR Rename(LPCSTR name, char* buf, size_t size)
    {
        if (!name || std::strcmp(name, kGameMutex) != 0)
            return name;
        std::snprintf(buf, size, "%s_%lu", kGameMutex, GetCurrentProcessId());
        return buf;
    }

    HANDLE WINAPI CreateMutexHook(LPSECURITY_ATTRIBUTES attrs, BOOL owner, LPCSTR name)
    {
        char buf[64];
        return g_createMutex(attrs, owner, Rename(name, buf, sizeof(buf)));
    }

    HANDLE WINAPI OpenMutexHook(DWORD access, BOOL inherit, LPCSTR name)
    {
        char buf[64];
        return g_openMutex(access, inherit, Rename(name, buf, sizeof(buf)));
    }

    HWND WINAPI FindWindowHook(LPCSTR className, LPCSTR title)
    {
        auto isGame = [](LPCSTR s) { return s && !IS_INTRESOURCE(s) && std::strstr(s, "Fallout") != nullptr; };
        if (isGame(className) || isGame(title))
            return nullptr;
        return g_findWindow(className, title);
    }

    // Diagnostics: log where the game decides to quit, to find any further checks.
    using ExitProcess_t = void(WINAPI*)(UINT);
    using PostQuitMessage_t = void(WINAPI*)(int);
    ExitProcess_t g_exitProcess = nullptr;
    PostQuitMessage_t g_postQuit = nullptr;

    __declspec(noinline) void WINAPI ExitProcessHook(UINT code)
    {
        LOG_INFO("Game called ExitProcess(%u) from %p", code, _ReturnAddress());
        g_exitProcess(code);
    }

    __declspec(noinline) void WINAPI PostQuitMessageHook(int code)
    {
        LOG_INFO("Game called PostQuitMessage(%d) from %p", code, _ReturnAddress());
        g_postQuit(code);
    }

    using TerminateProcess_t = BOOL(WINAPI*)(HANDLE, UINT);
    using SteamAPI_Init_t = bool(__cdecl*)();
    TerminateProcess_t g_terminate = nullptr;
    SteamAPI_Init_t g_steamInit = nullptr;

    __declspec(noinline) BOOL WINAPI TerminateProcessHook(HANDLE process, UINT code)
    {
        LOG_INFO("Game called TerminateProcess(%s, %u) from %p",
            GetProcessId(process) == GetCurrentProcessId() ? "itself" : "another process", code, _ReturnAddress());
        return g_terminate(process, code);
    }

    bool g_isSecondCopy = false;  // another game with this setting was already running when we started

    // The second copy doesn't sign in to Steam: the first test on 2026-10-08 showed it ended from outside right
    // after signing in, likely by Steam seeing the same game twice. Testing connects on this PC, without Steam.
    __declspec(noinline) bool __cdecl SteamInitHook()
    {
        if (g_isSecondCopy)
        {
            LOG_INFO("Second copy: not signing in to Steam (called from %p)", _ReturnAddress());
            return false;
        }
        bool ok = g_steamInit();
        LOG_INFO("SteamAPI_Init returned %s (called from %p)", ok ? "true" : "false", _ReturnAddress());
        return ok;
    }

    // Every way of ending a process goes through ntdll's NtTerminateProcess (ExitProcess and TerminateProcess
    // from any module), so a jump at its start logs who ended us. If it never logs, something outside did.
    using NtTerminate_t = LONG(NTAPI*)(HANDLE, LONG);
    NtTerminate_t g_ntTerminate = nullptr;
    uint8_t g_ntTerminateStart[5];

    __declspec(noinline) LONG NTAPI NtTerminateHook(HANDLE process, LONG status)
    {
        bool self = process == GetCurrentProcess() || GetProcessId(process) == GetCurrentProcessId();
        LOG_INFO("Ending %s with status %08X; called from:", self ? "this game" : "another process", status);
        void* frames[16];
        USHORT count = CaptureStackBackTrace(0, 16, frames, nullptr);
        for (USHORT i = 0; i < count; ++i)
        {
            HMODULE module = nullptr;
            char name[MAX_PATH] = "?";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    static_cast<LPCSTR>(frames[i]), &module))
                GetModuleFileNameA(module, name, MAX_PATH);
            const char* file = std::strrchr(name, '\\');
            LOG_INFO("  %p %s+%X", frames[i], file ? file + 1 : name,
                static_cast<unsigned>(reinterpret_cast<uintptr_t>(frames[i]) - reinterpret_cast<uintptr_t>(module)));
        }
        // Put the start back (we're ending anyway) and carry on.
        DWORD old;
        VirtualProtect(reinterpret_cast<void*>(g_ntTerminate), 5, PAGE_EXECUTE_READWRITE, &old);
        std::memcpy(reinterpret_cast<void*>(g_ntTerminate), g_ntTerminateStart, 5);
        VirtualProtect(reinterpret_cast<void*>(g_ntTerminate), 5, old, &old);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(g_ntTerminate), 5);
        return g_ntTerminate(process, status);
    }

    bool TraceTerminate()
    {
        auto* start = reinterpret_cast<uint8_t*>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtTerminateProcess"));
        if (!start || start[0] != 0xB8)  // expected: mov eax, <system call number> (5 bytes)
            return false;
        g_ntTerminate = reinterpret_cast<NtTerminate_t>(start);
        std::memcpy(g_ntTerminateStart, start, 5);
        DWORD old;
        if (!VirtualProtect(start, 5, PAGE_EXECUTE_READWRITE, &old))
            return false;
        start[0] = 0xE9;  // jmp NtTerminateHook
        int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&NtTerminateHook) - reinterpret_cast<uintptr_t>(start + 5));
        std::memcpy(start + 1, &rel, 4);
        VirtualProtect(start, 5, old, &old);
        FlushInstructionCache(GetCurrentProcess(), start, 5);
        return true;
    }

    // Which DLL an address is in, and how far into it.
    const char* ModuleOf(const void* address, uintptr_t& offset)
    {
        static thread_local char name[MAX_PATH];
        HMODULE module = nullptr;
        strcpy_s(name, "?");
        offset = reinterpret_cast<uintptr_t>(address);
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                static_cast<LPCSTR>(address), &module))
        {
            GetModuleFileNameA(module, name, MAX_PATH);
            offset -= reinterpret_cast<uintptr_t>(module);
        }
        const char* file = std::strrchr(name, '\\');
        return file ? file + 1 : name;
    }

    // The C++ type thrown (e.g. ".?AVruntime_error@std@@"), read from the exception's throw info (32-bit
    // MSVC layout: ThrowInfo -> CatchableTypeArray -> first CatchableType -> TypeDescriptor name).
    const char* ThrownType(const EXCEPTION_RECORD* r)
    {
        if (r->ExceptionCode != 0xE06D7363 || r->NumberParameters < 3)
            return "";
        __try
        {
            auto* throwInfo = reinterpret_cast<const uint32_t*>(r->ExceptionInformation[2]);
            auto* types = reinterpret_cast<const uint32_t*>(throwInfo[3]);
            auto* first = reinterpret_cast<const uint32_t*>(types[1]);
            auto* descriptor = reinterpret_cast<const char*>(first[1]);
            return descriptor + 8;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return "(unreadable)";
        }
    }

    // Every error the game or a plugin hits (even ones handled later), with who hit it: finds what makes a
    // copy stop itself. Only the first few, with where they came from.
    LONG CALLBACK OnException(EXCEPTION_POINTERS* info)
    {
        static LONG s_count = 0;
        const EXCEPTION_RECORD* r = info->ExceptionRecord;
        DWORD code = r->ExceptionCode;
        if (code == 0x406D1388 || code == 0x40010006 || code == 0x4001000A)  // thread names, debug text
            return EXCEPTION_CONTINUE_SEARCH;
        LONG n = InterlockedIncrement(&s_count);
        if (n > 30)
            return EXCEPTION_CONTINUE_SEARCH;
        uintptr_t offset;
        const char* module = ModuleOf(r->ExceptionAddress, offset);
        LOG_INFO("Error %08lX %s in %s+%X (thread %lu)", code, ThrownType(r), module, static_cast<unsigned>(offset),
            GetCurrentThreadId());
        if (n <= 10)
        {
            void* frames[12];
            USHORT count = CaptureStackBackTrace(1, 12, frames, nullptr);
            for (USHORT i = 0; i < count; ++i)
            {
                const char* m = ModuleOf(frames[i], offset);
                LOG_INFO("  from %s+%X", m, static_cast<unsigned>(offset));
            }
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // "Still running" every 5 s for the first 2 minutes: shows when a copy stopped, even if nothing else logs.
    DWORD WINAPI Heartbeat(void*)
    {
        for (int s = 5; s <= 120; s += 5)
        {
            Sleep(5000);
            LOG_INFO("Still running (%d s)", s);
        }
        return 0;
    }

    // Replace one Windows function in the game exe's import table. Returns the original.
    void* PatchImport(const char* dll, const char* function, void* replacement)
    {
        auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleA(nullptr));
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        void* target = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA(dll), function));
        if (!dir.VirtualAddress || !target)
            return nullptr;

        for (auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); imp->Name; ++imp)
        {
            if (_stricmp(reinterpret_cast<const char*>(base + imp->Name), dll) != 0)
                continue;
            for (auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->FirstThunk); thunk->u1.Function; ++thunk)
            {
                if (reinterpret_cast<void*>(thunk->u1.Function) != target)
                    continue;
                DWORD old;
                VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
                thunk->u1.Function = reinterpret_cast<uintptr_t>(replacement);
                VirtualProtect(&thunk->u1.Function, sizeof(void*), old, &old);
                return target;
            }
        }
        return nullptr;
    }
}

void AllowSecondCopy(bool waitForDebugger)
{
    // Our own marker (the game's lock is renamed per copy below, so it can't tell us).
    CreateMutexA(nullptr, FALSE, "NVCoop_GameRunning");
    g_isSecondCopy = GetLastError() == ERROR_ALREADY_EXISTS;
    LOG_INFO("Developer setting AllowSecondCopy: this is the %s copy", g_isSecondCopy ? "second" : "first");
    if (g_isSecondCopy && waitForDebugger)
    {
        char text[200];
        std::snprintf(text, sizeof(text), "Second copy: attach Visual Studio to FalloutNV.exe with ID %lu now "
            "(Debug > Attach to Process), then press OK.", GetCurrentProcessId());
        MessageBoxA(nullptr, text, "NVCoop", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
        LOG_INFO("Second copy: going on, debugger %s", IsDebuggerPresent() ? "attached" : "NOT attached");
    }
    LOG_DEBUG("Exit tracing: NtTerminateProcess %s", TraceTerminate() ? "on" : "not found");
    AddVectoredExceptionHandler(1, OnException);
    if (HANDLE t = CreateThread(nullptr, 0, Heartbeat, nullptr, 0, nullptr))
        CloseHandle(t);

    g_createMutex = static_cast<CreateMutexA_t>(PatchImport("kernel32.dll", "CreateMutexA", reinterpret_cast<void*>(&CreateMutexHook)));
    g_openMutex = static_cast<OpenMutexA_t>(PatchImport("kernel32.dll", "OpenMutexA", reinterpret_cast<void*>(&OpenMutexHook)));
    g_findWindow = static_cast<FindWindowA_t>(PatchImport("user32.dll", "FindWindowA", reinterpret_cast<void*>(&FindWindowHook)));
    g_exitProcess = static_cast<ExitProcess_t>(PatchImport("kernel32.dll", "ExitProcess", reinterpret_cast<void*>(&ExitProcessHook)));
    g_postQuit = static_cast<PostQuitMessage_t>(PatchImport("user32.dll", "PostQuitMessage", reinterpret_cast<void*>(&PostQuitMessageHook)));
    g_terminate = static_cast<TerminateProcess_t>(PatchImport("kernel32.dll", "TerminateProcess", reinterpret_cast<void*>(&TerminateProcessHook)));
    g_steamInit = static_cast<SteamAPI_Init_t>(PatchImport("steam_api.dll", "SteamAPI_Init", reinterpret_cast<void*>(&SteamInitHook)));
    LOG_DEBUG("Exit tracing: TerminateProcess %s, SteamAPI_Init %s", g_terminate ? "on" : "not found", g_steamInit ? "on" : "not found");
    LOG_DEBUG("Exit tracing: ExitProcess %s, PostQuitMessage %s", g_exitProcess ? "on" : "not found", g_postQuit ? "on" : "not found");
    if ((g_createMutex || g_openMutex) && g_findWindow)
        LOG_INFO("Developer setting: a second copy of the game may run on this PC");
    else
        LOG_WARN("Developer setting AllowSecondCopy: couldn't find the game's lock check");
}
