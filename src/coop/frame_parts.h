#pragma once

// What each part of our per-frame work costs (CLAUDE.md: measure before optimizing): the average and the most of each
// part over a stretch of frames, and a log line naming the parts worth looking at.

#include <cstdint>
#include <cstdio>
#include <string>

namespace coop
{
    class FrameParts
    {
    public:
        static constexpr int kMaxParts = 32;

        void Add(int part, double ms)
        {
            if (part < 0 || part >= kMaxParts)
                return;
            m_sum[part] += ms;
            if (ms > m_most[part])
                m_most[part] = ms;
        }

        // "npcs 0.10 (most 3.20), radio 0.02 (most 1.50)": each part (names[0..count)) averaging at least minAverage
        // ms a frame over these frames, or taking minMost ms at least once, the costliest first. Then starts over.
        std::string Report(const char* const* names, int count, uint32_t frames, double minAverage, double minMost)
        {
            std::string out;
            bool listed[kMaxParts] = {};
            count = count < kMaxParts ? count : kMaxParts;
            for (;;)
            {
                int best = -1;
                for (int i = 0; i < count; ++i)
                    if (!listed[i] && frames && (m_sum[i] / frames >= minAverage || m_most[i] >= minMost) &&
                        (best < 0 || m_sum[i] > m_sum[best]))
                        best = i;
                if (best < 0)
                    break;
                listed[best] = true;
                char part[96];
                std::snprintf(part, sizeof(part), "%s%s %.2f (most %.2f)", out.empty() ? "" : ", ", names[best],
                    m_sum[best] / frames, m_most[best]);
                out += part;
            }
            *this = FrameParts{};
            return out;
        }

    private:
        double m_sum[kMaxParts] = {};
        double m_most[kMaxParts] = {};
    };
}
