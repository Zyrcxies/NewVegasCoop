#pragma once

// The co-op rebalance mode's switch (REBALANCE.md, build step 1; Alex, October 2026): each save is either the game as
// it is (Normal) or balanced for co-op, picked before it starts and kept with it, and friendly fire on or off. The
// host's save decides for everyone in the session. Game-independent: tested in tests/.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace coop
{
    enum class GameMode : uint8_t { Unset = 0, Normal = 1, Balanced = 2 };

    struct SaveMode
    {
        GameMode mode = GameMode::Unset;  // Unset: not picked yet (a new game, or a save from before)
        bool friendlyFire = false;
        bool operator==(const SaveMode& o) const { return mode == o.mode && friendlyFire == o.friendlyFire; }
    };

    // Kept next to the save, in "<save>.nvcoop" (xNVSE keeps plugins' own data in its co-save only for plugins with
    // opcodes of their own: ours are the shared test ones). "NVCM", the version (1), the mode, friendly fire. A bad
    // one reads as Unset.
    constexpr uint8_t kSaveModeVersion = 1;
    std::vector<uint8_t> EncodeSaveMode(const SaveMode& m);
    SaveMode DecodeSaveMode(const uint8_t* data, size_t size);

    // The mode question's buttons (one box: the mode and friendly fire), and the mode a button picks.
    std::vector<std::string> SaveModeButtons();
    SaveMode SaveModeFromButton(int button);  // Unset: not one of them
    std::string Describe(const SaveMode& m);   // e.g. "Co-op balanced, friendly fire on"
}
