#pragma once

// Turns the few position updates a second we get for another player into smooth movement:
// we show them slightly in the past (kDelayMs) and blend between the two updates around that moment.
// Game-independent: tested in tests/.

#include <array>
#include <cstddef>
#include <cstdint>

namespace coop
{
    struct Pose
    {
        uint32_t space = 0;  // worldspace or interior cell form id
        float x = 0, y = 0, z = 0;
        float heading = 0;   // radians
        uint8_t flags = 0;   // kWeaponOut | kSneaking
        uint16_t moves = 0;  // the game's movement flags (coop::moves): which way and how they move; 0: not sent
        float pitch = 0;     // radians, looking down (+) or up (-); players only
        uint16_t runSpeed = 0;  // players: their running speed now (game units a second; a sprint mod raises it), 0 unknown

        static constexpr uint8_t kWeaponOut = 0x01;
        static constexpr uint8_t kSneaking = 0x02;
        static constexpr uint8_t kKnownFlags = kWeaponOut | kSneaking;
    };

    class Smoother
    {
    public:
        // (Halved from 100, 2026-10-30: a friend's character was 250-340 ms behind them; puppets steer smoothly
        // anyway.)
        static constexpr uint32_t kDelayMs = 50;

        // senderMs: the sender's clock when the pose was taken. localMs: our clock now.
        // Updates older than the newest one are ignored (they arrived out of order).
        void Add(uint32_t senderMs, const Pose& pose, uint32_t localMs);

        // Where to show them at localMs. False until the first update arrives.
        bool Sample(uint32_t localMs, Pose& out) const;
        // The newest update (where they really are, as far as we know). False until the first arrives.
        bool Newest(Pose& out) const
        {
            if (m_count == 0)
                return false;
            out = m_snaps[m_count - 1].pose;
            return true;
        }

        void Clear() { m_count = 0; m_hasOffset = false; }

    private:
        struct Snap { uint32_t senderMs; Pose pose; };
        static constexpr size_t kMaxSnaps = 16;
        std::array<Snap, kMaxSnaps> m_snaps{};  // oldest first
        size_t m_count = 0;
        int32_t m_offset = 0;  // our clock minus the sender's, using the fastest delivery seen
        bool m_hasOffset = false;
    };
}
