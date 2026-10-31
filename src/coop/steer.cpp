// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#include "coop/steer.h"

#include <cmath>

namespace coop
{
    namespace
    {
        using namespace moves;
        constexpr uint32_t kTurning = kTurnLeft | kTurnRight;
        constexpr uint32_t kCopied = 0xEC00;
        constexpr float kDirection = 0.38f;
        constexpr uint32_t kDirections = kForward | kBackward | kLeft | kRight;
    }

    uint32_t Steer::Flags(const Input& in)
    {
        float offX = in.wantX - in.atX, offY = in.wantY - in.atY;
        float off = std::sqrt(offX * offX + offY * offY);
        float vx = in.wantVx + kPull * offX;
        float vy = in.wantVy + kPull * offY;
        float speed = std::sqrt(vx * vx + vy * vy);

        bool theyMove = (in.theirFlags & kDirections) != 0;
        if (in.theirFlags & (kWalking | kRunning))
        {
            m_moving = theyMove || off > (m_moving ? kStopOff : kStartOff);
            m_running = theyMove && ((in.theirFlags & kRunning) || off > kCatchUpRun);
        }
        else
        {
            m_moving = speed > (m_moving ? kStopBelow : kStartAbove);
            m_running = speed > (m_running ? kWalkBelow : kRunAbove);
        }
        uint32_t flags = (in.theirFlags & kCopied) | (m_running ? kRunning : kWalking);
        if (!m_moving)
            return flags | (in.theirFlags & kTurning);
        if (speed < 1)
            return flags | (in.theirFlags & kDirections);

        float s = std::sin(in.heading), c = std::cos(in.heading);
        float forward = (vx * s + vy * c) / speed;
        float right = (vx * c - vy * s) / speed;
        if (forward > kDirection) flags |= kForward;
        if (forward < -kDirection) flags |= kBackward;
        if (right > kDirection) flags |= kRight;
        if (right < -kDirection) flags |= kLeft;
        return flags;
    }
}
