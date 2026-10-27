#pragma once

// On a crash: log what happened and write NVCoop-crash-<time>.dmp in the game folder,
// so players can send the log and dump.
void InstallCrashReporter();

// The game's address space (32-bit): whether it may use 4 GB (the "4GB patch"), and how much of it is free. Without
// the patch it has 2 GB, and running out crashes it (playthrough test, 2026-10-16: the friend's crashes on alt-tab).
struct AddressSpace
{
    bool large = false;
    unsigned freeMb = 0, totalMb = 0;
};
AddressSpace GetAddressSpace();
// Puts ours back in front if something replaced it since (theirs is still called after ours). Safe to repeat.
void CheckCrashReporter();
