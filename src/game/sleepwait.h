// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include "coop/coop.h"

#include <cstdint>

namespace game
{
    class SleepWait
    {
    public:
        void Install(coop::Coop& coop);
        void Update(uint32_t now);

        bool Clicked(void* menu, uint32_t tile);
        static void LabPress(uint8_t hours, bool sleep);

    private:
        void StartWait(uint8_t hours, bool sleep, bool mine);
        void Ask(const coop::Coop::WaitEvent& e);
        void FollowHostClock();
        coop::Coop* m_coop = nullptr;
        bool m_asking = false;
        bool m_popupOpen = false;
        uint32_t m_nextClock = 0;
    };
}
