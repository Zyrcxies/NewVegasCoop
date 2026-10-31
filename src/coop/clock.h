// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstdint>

namespace coop
{
    inline bool IsDue(uint32_t now, uint32_t at)
    {
        return at == 0 || static_cast<int32_t>(now - at) >= 0;
    }
}
