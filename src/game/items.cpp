#include "game/items.h"

#include "coop/clock.h"
#include "game/addresses.h"
#include "game/game.h"
#include "log.h"

#include "nvse/GameObjects.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <windows.h>

namespace game
{
    namespace
    {
        constexpr uint32_t kLookMs = 500;     // how often the items near us are listed (walks the loaded cells)
        // (Outdoors: items as far as the loaded cells reach, LoadedReach, count as near the group.)
        constexpr float kMyReach = 300;       // an item this close may be our player's pick-up or drop
        constexpr float kMyPushReach = 600;   // something physics moves (a crate, a tumbleweed) this close: our push
        // Going to another game's place (step 8.9): gliding there over a bit less than a look, then staying put this
        // long (a carried one: as long as its carrier keeps saying so, at least every coop::kHeldResendMs); not our
        // doing while it settles after. At most kMaxGlides at once (more are put there straight away).
        constexpr uint32_t kGlideMs = 450;
        constexpr uint32_t kStayMs = 700;
        constexpr uint32_t kHeldStayMs = 10000;
        constexpr uint32_t kSettleMs = 1500;
        constexpr size_t kMaxGlides = 16;
        constexpr float kGlideRange = 3000;  // further from our player: put there straight away (saves frame time)

        coop::ItemPose PoseOf(const WorldItem& w) { return {w.x, w.y, w.z, w.rx, w.ry, w.rz}; }

        // (Logs: where it is, and where its 3D is if that's elsewhere.)
        std::string Where(TESObjectREFR* ref)
        {
            WorldItem w = ReadWorldItem(ref);
            char text[96];
            float x, y, z;
            if (Get3DPosition(ref, x, y, z) && (std::fabs(x - w.x) > 4 || std::fabs(y - w.y) > 4 || std::fabs(z - w.z) > 4))
                std::snprintf(text, sizeof(text), "%.0f, %.0f, %.0f (its 3D at %.0f, %.0f, %.0f)", w.x, w.y, w.z, x, y, z);
            else
                std::snprintf(text, sizeof(text), "%.0f, %.0f, %.0f", w.x, w.y, w.z);
            return text;
        }

        void MoveTo(TESObjectREFR* ref, const coop::ItemPose& p) { MoveItem(ref, p.x, p.y, p.z, p.rx, p.ry, p.rz); }

        float AngleBetween(float a, float b, float t)
        {
            return a + std::remainder(b - a, 6.2831853f) * t;  // (the short way round)
        }

        coop::ItemPose Between(const coop::ItemPose& a, const coop::ItemPose& b, float t)
        {
            return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, AngleBetween(a.rx, b.rx, t),
                AngleBetween(a.ry, b.ry, t), AngleBetween(a.rz, b.rz, t)};
        }
    }

    void WorldItems::Update(coop::Coop& coop, uint32_t now)
    {
        if (!coop.InGame())
        {
            m_glides.clear();
            m_settling.clear();
            return;
        }
        bool host = coop.GetRole() == coop::Role::Host;
        if (host)
            HostApply(coop, now);
        Glides(now);
        if (!coop::IsDue(now, m_nextLook))
            return;
        m_nextLook = now + kLookMs;
        TESObjectREFR* player = Player();
        if (!player || !player->parentCell)
            return;

        LARGE_INTEGER start, end, freq;
        QueryPerformanceCounter(&start);
        uint32_t grabbed = GrabbedItem();
        if (grabbed != m_grabbed)
        {
            if (m_grabbed)
                LOG_DEBUG("Items: our player let go of %08X at %s", m_grabbed, Where(LookupRef(m_grabbed)).c_str());
            if (grabbed)
                LOG_DEBUG("Items: our player carries %08X", grabbed);
        }
        m_grabbed = grabbed;
        // Items, then things physics moves that aren't items (crates, tumbleweeds: step 8.9).
        std::vector<TESObjectREFR*> refs, movables;
        const float range = LoadedReach();
        ItemsNear(refs, range);
        size_t items = refs.size();
        MovablesNear(movables, range);
        refs.insert(refs.end(), movables.begin(), movables.end());
        m_near = static_cast<uint32_t>(refs.size());
        if (host)
        {
            std::vector<net::ItemState> states;
            states.reserve(refs.size());
            for (TESObjectREFR* ref : refs)
            {
                WorldItem w = ReadWorldItem(ref);
                TESObjectREFR* parent = TopEnableParent(ref);
                if (parent && net::IsMadeWhilePlaying(RefId(parent)))
                    parent = nullptr;  // (one only our game has: nothing to match)
                if (w.base && w.count >= 1)
                    states.push_back({RefId(ref), w.base, w.count, IsThere(w), w.x, w.y, w.z, w.rx, w.ry, w.rz,
                        RefId(ref) == m_grabbed, parent ? RefId(parent) : 0, parent && IsEnabled(parent)});
            }
            coop.SetWorldItems(states);
        }
        else
        {
            std::vector<coop::LocalItem> local;
            local.reserve(refs.size());
            for (size_t i = 0; i < refs.size(); ++i)
            {
                WorldItem w = ReadWorldItem(refs[i]);
                if (!w.base || w.count < 1)
                    continue;
                uint32_t id = RefId(refs[i]);
                bool movable = i >= items;
                float dx = w.x - player->posX, dy = w.y - player->posY, dz = w.z - player->posZ;
                float reach = movable ? kMyPushReach : kMyReach;
                local.push_back({id, w.base, w.count, IsThere(w), w.x, w.y, w.z,
                    dx * dx + dy * dy + dz * dz < reach * reach, w.rx, w.ry, w.rz, id == m_grabbed,
                    Following(id, now), movable, (w.flags & layout::kRefDeleted) != 0});
                if (TESObjectREFR* parent = TopEnableParent(refs[i]))
                {
                    local.back().parent = RefId(parent);
                    local.back().parentOn = IsEnabled(parent);
                }
            }
            // Indoors the room, outdoors the worldspace: when it changes, what was near us is gone because we left.
            const void* world = *reinterpret_cast<const void* const*>(
                reinterpret_cast<const char*>(player->parentCell) + layout::kCellWorldSpace);
            const void* area = world ? world : player->parentCell;
            coop::ItemActions actions = coop.LookAtItems(local, area == m_area);
            m_area = area;
            for (const net::ItemState& h : actions.make)
            {
                TESObjectREFR* made = PlaceItem(h.base, h.count, h.x, h.y, h.z);
                coop.ItemMade(h.ref, made ? RefId(made) : 0);
                if (made && !net::IsMadeWhilePlaying(h.ref))
                    LOG_DEBUG("Items: %08X was picked up in our save but is there in the host's game: a copy (%08X) "
                        "in its place", h.ref, RefId(made));
                if (made)
                    ++m_made;
                else
                    LOG_WARN("Items: couldn't place a copy of the host's %08X (%d of %08X)", h.ref, h.count, h.base);
            }
            for (uint32_t id : actions.remove)
            {
                RemoveWorldItem(LookupRef(id));
                ++m_removed;
            }
            for (const coop::ItemActions::Parent& p : actions.parents)
            {
                SetEnabled(LookupRef(p.ref), p.on);
                ++m_made;
                LOG_DEBUG("Items: enable parent %08X switched %s, as in the host's game", p.ref, p.on ? "on" : "off");
            }
            for (uint32_t id : actions.enable)
            {
                SetEnabled(LookupRef(id), true);
                ++m_made;
                LOG_DEBUG("Items: %08X was switched off in our save but is there in the host's game: switched on", id);
            }
            for (const coop::ItemActions::Move& m : actions.move)
            {
                if (m.ref == m_grabbed)
                    continue;  // (our player has it: the host's game hears that from us)
                GlideTo(m.ref, {m.x, m.y, m.z, m.rx, m.ry, m.rz}, m.held, now, "the host");
                ++m_moved;
            }
            m_takes += static_cast<uint32_t>(actions.takes.size());
            m_drops += static_cast<uint32_t>(actions.drops.size());
            m_sent += static_cast<uint32_t>(actions.moved.size());
        }
        QueryPerformanceCounter(&end);
        QueryPerformanceFrequency(&freq);
        m_lookMs += 1000.0 * (end.QuadPart - start.QuadPart) / freq.QuadPart;
        ++m_looks;

        if (coop::IsDue(now, m_nextLog))
        {
            if (m_nextLog && m_looks)
                LOG_INFO("Items: %u near; a look took %.3f ms on average (%u looks); %u pick-ups, %u drops, %u given "
                    "back; %u copies made, %u removed, %u moved here (%s), %u moves of ours sent", m_near,
                    m_lookMs / m_looks, m_looks, m_takes, m_drops, m_refunds, m_made, m_removed, m_moved,
                    host ? "friends' moves" : "the host's places", m_sent);
            m_nextLog = now + 60000;
            m_lookMs = 0;
            m_looks = m_takes = m_drops = m_refunds = m_made = m_removed = m_moved = m_sent = 0;
        }
    }

    void WorldItems::HostApply(coop::Coop& coop, uint32_t now)
    {
        for (const coop::Coop::ItemTakeFrom& t : coop.TakeItemTakes())
        {
            // (Only an item lying here: never a crate, an NPC or a door, whatever a friend's game says.)
            TESObjectREFR* ref = LookupRef(t.take.ref);
            WorldItem w = ReadWorldItem(ref);
            if (IsThere(w) && IsItem(w.base))
            {
                RemoveWorldItem(ref);  // they have it now
                ++m_takes;
                LOG_DEBUG("Items: %s picked up %08X", coop.NameOf(t.playerId).c_str(), t.take.ref);
            }
            else
            {
                // Someone was quicker: it comes back out of their inventory.
                coop.RefundItems(t.playerId, {t.take.base, -t.take.count});
                ++m_refunds;
                LOG_DEBUG("Items: %s picked up %08X, already gone here", coop.NameOf(t.playerId).c_str(), t.take.ref);
            }
        }
        for (const coop::Coop::ItemDropFrom& d : coop.TakeItemDrops())
        {
            const net::ItemDrop& drop = d.drop;
            if (PlaceItem(drop.base, drop.count, drop.x, drop.y, drop.z))
            {
                ++m_drops;
                LOG_DEBUG("Items: %s dropped %d of %08X", coop.NameOf(d.playerId).c_str(), drop.count, drop.base);
            }
            else
            {
                coop.RefundItems(d.playerId, {drop.base, drop.count});  // couldn't: back to them
                ++m_refunds;
                LOG_WARN("Items: couldn't place %s's drop (%d of %08X); given back", coop.NameOf(d.playerId).c_str(),
                    drop.count, drop.base);
            }
        }
        // Moved in a friend's game (step 8.9): ours goes there too (not one our player carries: ours wins).
        for (const coop::Coop::ItemMoveFrom& f : coop.TakeItemMoves())
        {
            const net::ItemMove& m = f.move;
            if (m.ref == m_grabbed)
                continue;
            GlideTo(m.ref, {m.x, m.y, m.z, m.rx, m.ry, m.rz}, m.held, now, coop.NameOf(f.playerId).c_str());
            ++m_moved;
        }
    }

    void WorldItems::GlideTo(uint32_t id, const coop::ItemPose& to, bool held, uint32_t now, const char* whose)
    {
        TESObjectREFR* ref = LookupRef(id);
        if (!ref)
            return;
        bool fresh = m_glides.find(id) == m_glides.end();
        TESObjectREFR* player = Player();
        float dx = player ? to.x - player->posX : 0, dy = player ? to.y - player->posY : 0;
        if (fresh && (!Is3DLoaded(ref) || m_glides.size() >= kMaxGlides || dx * dx + dy * dy > kGlideRange * kGlideRange))
        {
            MoveTo(ref, to);  // (out of sight, or many at once: there straight away)
            m_settling[id] = now;
            return;
        }
        if (fresh)
            LOG_DEBUG("Items: %08X goes from %s to %s's place (%.0f, %.0f, %.0f%s)", id, Where(ref).c_str(), whose,
                to.x, to.y, to.z, held ? ", carried" : "");
        Glide& g = m_glides[id];
        g.from = PoseOf(ReadWorldItem(ref));
        g.to = to;
        g.startedAt = now;
        g.held = held;
    }

    void WorldItems::Glides(uint32_t now)
    {
        // (Our player grabbing one: let go of it that same frame, not at the next look. 2026-10-16 test: the host
        // grabbed a pitcher this game was holding where the friend carried it, and the two ended up apart.)
        uint32_t grabbed = m_glides.empty() ? 0 : GrabbedItem();
        for (auto it = m_glides.begin(); it != m_glides.end();)
        {
            TESObjectREFR* ref = LookupRef(it->first);
            const Glide& g = it->second;
            uint32_t age = now - g.startedAt;
            if (!ref || !IsThere(ReadWorldItem(ref)) || it->first == grabbed ||
                age > kGlideMs + (g.held ? kHeldStayMs : kStayMs))
            {
                if (ref)
                    LOG_DEBUG("Items: %08X left at %s%s", it->first, Where(ref).c_str(),
                        it->first == grabbed ? " (our player took it)" : "");
                m_settling[it->first] = now;  // (let go: it rests or falls from here)
                it = m_glides.erase(it);
                continue;
            }
            MoveTo(ref, Between(g.from, g.to, age >= kGlideMs ? 1.f : static_cast<float>(age) / kGlideMs));
            ++it;
        }
        for (auto it = m_settling.begin(); it != m_settling.end();)
            it = now - it->second > kSettleMs ? m_settling.erase(it) : std::next(it);
    }

    bool WorldItems::Following(uint32_t ref, uint32_t now) const
    {
        if (m_glides.count(ref))
            return true;
        auto it = m_settling.find(ref);
        return it != m_settling.end() && now - it->second <= kSettleMs;
    }
}
