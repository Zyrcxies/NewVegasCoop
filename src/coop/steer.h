// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstdint>

namespace coop
{
    namespace moves
    {
        constexpr uint32_t kForward = 0x1, kBackward = 0x2, kLeft = 0x4, kRight = 0x8;
        constexpr uint32_t kTurnLeft = 0x10, kTurnRight = 0x20;
        constexpr uint32_t kWalking = 0x100, kRunning = 0x200, kSneaking = 0x400;
        constexpr uint32_t kJump = 0x1000;
    }

    class Steer
    {
    public:
        static constexpr float kStartAbove = 40, kStopBelow = 20;
        static constexpr float kRunAbove = 220, kWalkBelow = 180;
        static constexpr float kPull = 2.0f;
        static constexpr float kStartOff = 40, kStopOff = 15, kCatchUpRun = 120;

        struct Input
        {
            float atX, atY;
            float wantX, wantY;
            float wantVx, wantVy;
            float heading;
            uint32_t theirFlags;
        };

        uint32_t Flags(const Input& in);
        bool Moving() const { return m_moving; }
        bool Running() const { return m_running; }

    private:
        bool m_moving = false, m_running = false;
    };
}
