// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#include "coop/game_mode.h"

namespace coop
{
    std::vector<uint8_t> EncodeSaveMode(const SaveMode& m)
    {
        return {'N', 'V', 'C', 'M', kSaveModeVersion, static_cast<uint8_t>(m.mode),
            static_cast<uint8_t>(m.friendlyFire ? 1 : 0)};
    }

    SaveMode DecodeSaveMode(const uint8_t* data, size_t size)
    {
        SaveMode m;
        if (!data || size != 7 || data[0] != 'N' || data[1] != 'V' || data[2] != 'C' || data[3] != 'M' ||
            data[4] != kSaveModeVersion || data[5] > static_cast<uint8_t>(GameMode::Balanced) || data[6] > 1)
            return m;
        m.mode = static_cast<GameMode>(data[5]);
        m.friendlyFire = data[6] == 1;
        return m;
    }

    std::vector<std::string> SaveModeButtons()
    {
        return {"Normal", "Normal, friendly fire", "Co-op balanced", "Co-op balanced, friendly fire"};
    }

    SaveMode SaveModeFromButton(int button)
    {
        if (button < 0 || button > 3)
            return {};
        return SaveMode{button < 2 ? GameMode::Normal : GameMode::Balanced, button % 2 == 1};
    }

    std::string Describe(const SaveMode& m)
    {
        if (m.mode == GameMode::Unset)
            return "not picked yet";
        return std::string(m.mode == GameMode::Balanced ? "Co-op balanced" : "Normal") + ", friendly fire " +
            (m.friendlyFire ? "on" : "off");
    }
}
