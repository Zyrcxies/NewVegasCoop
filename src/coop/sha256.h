#pragma once

// SHA-256 (FIPS 180-4): the fingerprint of a mod file, so two games can tell whether they have the same version
// (step 8.6). Game-independent: tested in tests/ against the standard's examples.

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
        Digest Finish();  // the hash of everything added (the object can't be added to after)

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
        uint64_t m_total = 0;  // bytes added
    };
}
