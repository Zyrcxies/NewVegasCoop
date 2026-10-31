// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace coop
{
    constexpr uint32_t kDesyncGraceMs = 5000;
    constexpr uint32_t kDesyncGoneMs = 3000;
    constexpr size_t kMaxDesyncs = 512;

    enum class DesyncKind : uint8_t { Npc, Item, Door };
    enum class DesyncAspect : uint8_t { Life, Place, Health, There, Door };

    struct Disagreement
    {
        DesyncKind kind = DesyncKind::Npc;
        uint32_t ref = 0;
        DesyncAspect aspect = DesyncAspect::Place;
        std::string detail;
    };

    struct DesyncEvent
    {
        Disagreement what;
        bool started = true;
        uint32_t lastedMs = 0;
    };

    struct DesyncCounts
    {
        uint16_t npcs = 0, items = 0, doors = 0;
        bool operator==(const DesyncCounts& o) const { return npcs == o.npcs && items == o.items && doors == o.doors; }
        bool operator!=(const DesyncCounts& o) const { return !(*this == o); }
        uint32_t Total() const { return uint32_t(npcs) + items + doors; }
    };

    class DesyncTracker
    {
    public:
        void Saw(const Disagreement& d, uint32_t now);
        std::vector<DesyncEvent> Sweep(uint32_t now);
        DesyncCounts Lasting() const;
        void Clear() { m_entries.clear(); }

    private:
        struct Entry
        {
            Disagreement what;
            uint32_t firstSeen = 0, lastSeen = 0;
            bool reported = false;
        };
        std::map<std::tuple<DesyncKind, uint32_t, DesyncAspect>, Entry> m_entries;
    };

    const char* DesyncKindName(DesyncKind kind);
}
