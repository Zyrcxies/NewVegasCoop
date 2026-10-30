#include "coop/steer.h"

#include <cmath>

namespace coop
{
    namespace
    {
        using namespace moves;
        constexpr uint32_t kTurning = kTurnLeft | kTurnRight;
        // Copied from them: Sneaking, Swimming, Flying, Fall, Slide. Not Jump (0x1000): a puppet jumps for real
        // instead (game::Jump), so the game moves it through the air.
        constexpr uint32_t kCopied = 0xEC00;
        // A step counts towards a direction within 67.5 degrees of it, so diagonals get two flags.
        constexpr float kDirection = 0.38f;  // cos(67.5)
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
        if (in.theirFlags & (kWalking | kRunning))  // their own flags arrived (a fake friend's may not have them)
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
            return flags | (in.theirFlags & kTurning);  // standing: their turning-on-the-spot
        if (speed < 1)
            return flags | (in.theirFlags & kDirections);  // (moving on the spot, against a wall: their keys)

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
