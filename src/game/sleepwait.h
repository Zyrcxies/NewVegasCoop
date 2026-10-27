#pragma once

// Sleep and wait (step 8.2, DESIGN: everyone must agree). In co-op, pressing Wait or Sleep in the game's menu asks
// everyone else first (a popup on their screen); only when all accept does every game wait the same hours (the
// one who asked from their menu, everyone else from the same menu opened for them). The host's clock is the real
// one: friends' clocks follow it.

#include "coop/coop.h"

#include <cstdint>

namespace game
{
    class SleepWait
    {
    public:
        // Once, when the game has loaded: our hook on the menu's buttons (passes everything on outside co-op).
        void Install(coop::Coop& coop);
        void Update(uint32_t now);  // every frame in the world

        // The hook's: a button in the sleep/wait menu was clicked. True: go on to the game's own handling.
        bool Clicked(void* menu, uint32_t tile);
        // Lab (coop sleepwait): opens the menu (if it isn't), sets the hours and presses its button like a player.
        static void LabPress(uint8_t hours, bool sleep);

    private:
        void StartWait(uint8_t hours, bool sleep, bool mine);
        void Ask(const coop::Coop::WaitEvent& e);
        void FollowHostClock();
        coop::Coop* m_coop = nullptr;
        bool m_asking = false;        // we pressed Wait or Sleep and the others are being asked
        bool m_popupOpen = false;     // we were asked and haven't answered yet
        uint32_t m_nextClock = 0;     // host: when to read our clock again
    };
}
