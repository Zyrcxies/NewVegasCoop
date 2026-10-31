// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace coop
{
    class Sha256
    {
    public:
        using Digest = std::array<uint8_t, 32>;

        Sha256();
        void Add(const void* data, size_t size);
        Digest Finish();

        static Digest Of(const void* data, size_t size)
        {
            Sha256 h;
            h.Add(data, size);
            return h.Finish();
        }

    private:
        void Block(const uint8_t* p);

        uint32_t m_state[8];
        uint8_t m_buffer[64];
        size_t m_buffered = 0;
        uint64_t m_total = 0;
    };
}
