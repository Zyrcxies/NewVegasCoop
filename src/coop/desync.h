#pragma once

// The desync detector (DESIGN rule 8). A friend's game compares itself with the host's latest word (its NPCs, items
// and doors) and tells this what disagrees right now; most of it the usual fixes put right within a moment. One
// that lasts past kDesyncGraceMs (the fixes didn't take) is a desync: reported once, and again when it's over.
// Game-independent: tested in tests/.

#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace coop
{
    constexpr uint32_t kDesyncGraceMs = 5000;  // disagreeing this long: a desync
    constexpr uint32_t kDesyncGoneMs = 3000;   // not seen disagreeing this long: over
    constexpr size_t kMaxDesyncs = 512;        // tracked at once (more aren't, till some are over)

    enum class DesyncKind : uint8_t { Npc, Item, Door };
    // What about it: alive or dead, where it is, its health, whether it's there at all, open or locked.
    enum class DesyncAspect : uint8_t { Life, Place, Health, There, Door };

    struct Disagreement
    {
        DesyncKind kind = DesyncKind::Npc;
        uint32_t ref = 0;  // the host's id for it
        DesyncAspect aspect = DesyncAspect::Place;
        std::string detail;  // for the log: "dead here, alive on the host", "300 units off"
    };

    struct DesyncEvent
    {
        Disagreement what;  // (its detail as last seen)
        bool started = true;  // false: it's over
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
        void Saw(const Disagreement& d, uint32_t now);  // disagreeing now
        // Desyncs that started (lasted past the grace) or are over since the last call; call every second or so.
        std::vector<DesyncEvent> Sweep(uint32_t now);
        DesyncCounts Lasting() const;  // desyncs going on now, by kind
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

    const char* DesyncKindName(DesyncKind kind);  // "NPC", "item", "door"
}
