#pragma once

// Developer-only (setting DumpGameCode=1): saves the game's program as it is in memory, for reading in
// Ghidra. The file on disk is scrambled by Steam's copy protection; in memory it's readable. Runs at the
// very start (before other plugins change any game code). The file is never shared or committed.

#include <string>

bool DumpGameImage(const std::string& path);
