#pragma once

// Timers on GetTickCount (milliseconds since the PC started, wrapping every 49.7 days).
// Game-independent: tested in tests/.

#include <cstdint>

namespace coop
{
    // Has `at` come? 0 means not set yet: due straight away. (Without that, a PC that has been on for more
    // than 24.8 days, Fast Startup included, would see 0 as in the future and never act.)
    inline bool IsDue(uint32_t now, uint32_t at)
    {
        return at == 0 || static_cast<int32_t>(now - at) >= 0;
    }
}
