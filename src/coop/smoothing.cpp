#include "coop/smoothing.h"

#include <cmath>

namespace coop
{
    namespace
    {
        constexpr float kPi = 3.14159265f;

        float LerpAngle(float a, float b, float t)
        {
            float d = std::fmod(b - a, 2 * kPi);
            if (d > kPi) d -= 2 * kPi;
            if (d < -kPi) d += 2 * kPi;
            float r = std::fmod(a + d * t, 2 * kPi);
            return r < 0 ? r + 2 * kPi : r;
        }
    }

    void Smoother::Add(uint32_t senderMs, const Pose& pose, uint32_t localMs)
    {
        if (m_count > 0 && static_cast<int32_t>(senderMs - m_snaps[m_count - 1].senderMs) <= 0)
            return;

        // The fastest delivery tells us how the two clocks line up. Creep towards slower deliveries
        // a little each time so clock drift and changed routes don't leave us lagging forever.
        int32_t offset = static_cast<int32_t>(localMs - senderMs);
        if (!m_hasOffset || offset < m_offset)
            m_offset = offset;
        else if (offset > m_offset)
            ++m_offset;
        m_hasOffset = true;

        if (m_count == kMaxSnaps)
        {
            for (size_t i = 1; i < kMaxSnaps; ++i)
                m_snaps[i - 1] = m_snaps[i];
            --m_count;
        }
        m_snaps[m_count++] = Snap{senderMs, pose};
    }

    bool Smoother::Sample(uint32_t localMs, Pose& out) const
    {
        if (m_count == 0)
            return false;

        // The moment to show, on the sender's clock.
        uint32_t target = localMs - kDelayMs - static_cast<uint32_t>(m_offset);
        auto before = [target](uint32_t t) { return static_cast<int32_t>(t - target) <= 0; };

        if (!before(m_snaps[0].senderMs))
        {
            out = m_snaps[0].pose;
            return true;
        }
        for (size_t i = 1; i < m_count; ++i)
        {
            const Snap& a = m_snaps[i - 1];
            const Snap& b = m_snaps[i];
            if (before(b.senderMs))
                continue;
            if (a.pose.space != b.pose.space)
            {
                out = a.pose;  // changed place: jump, don't slide between two different places
                return true;
            }
            float t = static_cast<float>(target - a.senderMs) / static_cast<float>(b.senderMs - a.senderMs);
            out.space = a.pose.space;
            out.x = a.pose.x + (b.pose.x - a.pose.x) * t;
            out.y = a.pose.y + (b.pose.y - a.pose.y) * t;
            out.z = a.pose.z + (b.pose.z - a.pose.z) * t;
            out.heading = LerpAngle(a.pose.heading, b.pose.heading, t);
            out.pitch = a.pose.pitch + (b.pose.pitch - a.pose.pitch) * t;
            out.flags = a.pose.flags;  // switches, not blended
            out.moves = a.pose.moves;
            return true;
        }
        out = m_snaps[m_count - 1].pose;  // no newer update yet: stay at the last one
        return true;
    }
}
