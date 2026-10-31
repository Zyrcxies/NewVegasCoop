// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace coop
{
    enum class GameMode : uint8_t { Unset = 0, Normal = 1, Balanced = 2 };

    struct SaveMode
    {
        GameMode mode = GameMode::Unset;
        bool friendlyFire = false;
        bool operator==(const SaveMode& o) const { return mode == o.mode && friendlyFire == o.friendlyFire; }
    };

    constexpr uint8_t kSaveModeVersion = 1;
    std::vector<uint8_t> EncodeSaveMode(const SaveMode& m);
    SaveMode DecodeSaveMode(const uint8_t* data, size_t size);

    std::vector<std::string> SaveModeButtons();
    SaveMode SaveModeFromButton(int button);
    std::string Describe(const SaveMode& m);
}
