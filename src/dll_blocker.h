#pragma once

// Keeps DLLs that crash the game from loading into it (see dll_names.h): Windows' DLL loader refuses them.
void BlockCrashingDlls();
