#pragma once

// Steering a puppet (DESIGN rule 7): picks the movement flags (coop::moves) that take it to where its player
// is. It faces where they face, so the flags say which way to step (forward, back, sideways) and how fast
// (walk or run). It aims for where they are going plus a pull back onto where they are, so small drift
// fixes itself instead of building up. Game-independent: tested in tests/.

#include <cstdint>

namespace coop
{
    // The game's movement flags (an actor's mover; names from JIP LN's ActorMover::MovementFlags): which way an
    // actor moves and how. The game picks its walk, run, strafe and sneak animations from them.
    namespace moves
    {
        constexpr uint32_t kForward = 0x1, kBackward = 0x2, kLeft = 0x4, kRight = 0x8;
        constexpr uint32_t kTurnLeft = 0x10, kTurnRight = 0x20;
        constexpr uint32_t kWalking = 0x100, kRunning = 0x200, kSneaking = 0x400;
        constexpr uint32_t kJump = 0x1000;  // senders set it while in a jump (the player's own flags don't say)
    }

    class Steer
    {
    public:
        // Game units per second.
        static constexpr float kStartAbove = 40, kStopBelow = 20;  // moving at all (with a gap: no flicker)
        static constexpr float kRunAbove = 220, kWalkBelow = 180;  // the game's walk is ~90, its run ~340
        static constexpr float kPull = 2.0f;  // per second: 100 units off adds 200 units/s towards the spot

        struct Input
        {
            float atX, atY;      // where the puppet is
            float wantX, wantY;  // where it should be
            float wantVx, wantVy;  // how fast that spot moves
            float heading;       // where it faces (radians; the game's forward is (sin, cos))
            uint32_t theirFlags; // their own movement flags: sneaking, swimming etc. are copied
        };

        uint32_t Flags(const Input& in);  // call every frame

    private:
        bool m_moving = false, m_running = false;
    };
}
