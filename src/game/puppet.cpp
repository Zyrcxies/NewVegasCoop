#include "game/puppet.h"

#include "coop/clock.h"
#include "dev/inspect_text.h"
#include "game/game.h"
#include "game/mover.h"
#include "log.h"

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        constexpr uint32_t kWeaponCheckEveryMs = 250;
        constexpr uint32_t kDrawTakesMs = 800;  // checked again after this (drawing is an animation)
        constexpr float kPutBackDistance = 200;   // further off than this: put it back where it should be
        constexpr float kTurnAtLeast = 0.01f;     // radians
        constexpr uint32_t kPutBackPauseMs = 300; // a put-back shows up a frame or two later: don't repeat it
        constexpr float kPi = 3.14159265f;
        // Movement flags copied: direction, turning, walk/run/sneak/swim, jump/fly/fall/slide. Not
        // "Keyboard" (0x40): it says how the PLAYER is controlled.
        constexpr uint32_t kCopiedFlags = 0xFFBF;
        // What "its legs took the flags" means: same direction and pace (the game may change the rest).
        constexpr uint32_t kTakenMask = 0x030F;
        constexpr uint32_t kDirectionMask = 0x000F;  // forward, backward, left, right

        float AngleDiff(float a, float b)
        {
            float d = std::fmod(a - b, 2 * kPi);
            if (d > kPi) d -= 2 * kPi;
            if (d < -kPi) d += 2 * kPi;
            return d;
        }
    }

    void Puppet::Start(std::string who, uint32_t statsEveryMs)
    {
        *this = Puppet();
        m_who = std::move(who);
        m_statsEveryMs = statsEveryMs;
    }

    void Puppet::Release(TESObjectREFR* actor)
    {
        if (actor)
            ReleaseMovementFlags(actor);
        m_asked = 0;
        m_waiting = false;
    }

    void Puppet::Update(TESObjectREFR* actor, const coop::Pose& want, uint32_t now)
    {
        coop::Pose at;
        if (!actor || !GetPose(actor, at))
            return;

        // How fast that spot moves (smoothed a little: frame times jitter).
        if (m_lastWantAt && now != m_lastWantAt)
        {
            float dt = static_cast<float>(now - m_lastWantAt) / 1000.0f;
            m_wantVx += ((want.x - m_lastWantX) / dt - m_wantVx) * 0.3f;
            m_wantVy += ((want.y - m_lastWantY) / dt - m_wantVy) * 0.3f;
        }
        m_lastWantX = want.x;
        m_lastWantY = want.y;
        m_lastWantAt = now;

        // Legs: steered onto the spot, and held so its AI can't take them back. (A game that doesn't
        // send movement flags, like the fake friend's, still says whether they sneak.)
        uint32_t theirs = (want.moves & kCopiedFlags) | ((want.flags & coop::Pose::kSneaking) ? coop::moves::kSneaking : 0);
        coop::Steer::Input in{at.x, at.y, want.x, want.y, m_wantVx, m_wantVy, want.heading, theirs};
        uint32_t flags = m_steer.Flags(in);
        // What became of last frame's ask: a real NPC's AI can fight the legs (step 5 lab: a farmer and a
        // shopkeeper wouldn't follow, Doc Mitchell did).
        if (m_asked)
        {
            MoverState s = ReadMover(actor);
            ++m_checked;
            if (!s.askedOn)
                ++m_switchedOff;
            else if (s.asked != m_asked)
                ++m_replaced;
            else if ((s.flags & kDirectionMask) != (m_asked & kDirectionMask))
                ++m_ignored;
        }
        if ((flags & kTakenMask) != (m_asked & kTakenMask))
        {
            m_waiting = true;
            m_askedAt = now;
        }
        m_asked = flags;
        HoldMovementFlags(actor, flags);
        if (m_waiting && (ReadMover(actor).flags & kTakenMask) == (flags & kTakenMask))
        {
            uint32_t lag = now - m_askedAt;
            m_waiting = false;
            m_lagSum += lag;
            m_lagMax = lag > m_lagMax ? lag : m_lagMax;
            ++m_lagCount;
            if (lag > 250)
                LOG_INFO("%s: legs took %u ms to start moving %s", m_who.c_str(), lag,
                    dev::MoveFlagNames(flags).c_str());
        }

        bool jumping = (want.moves & coop::moves::kJump) != 0;
        if (jumping && !m_jumping)
        {
            ++m_jumps;
            if (!Jump(actor))
                LOG_INFO("%s: couldn't jump (no physics controller)", m_who.c_str());
        }
        m_jumping = jumping;
        UpdateWeaponOut(actor, (want.flags & coop::Pose::kWeaponOut) != 0, now);
        if (std::fabs(AngleDiff(at.heading, want.heading)) > kTurnAtLeast)
            Turn(actor, want.heading);

        float dx = at.x - want.x, dy = at.y - want.y;
        float error = std::sqrt(dx * dx + dy * dy);
        m_errorSum += error;
        m_errorMax = error > m_errorMax ? error : m_errorMax;
        ++m_frames;
        if (error > kPutBackDistance && now - m_putBackAt >= kPutBackPauseMs)
        {
            m_putBackAt = now;
            Move(actor, want);
            ++m_snaps;
        }
        LogStats(now);
    }

    void Puppet::SetGear(TESObjectREFR* actor, const std::vector<uint32_t>& gear)
    {
        auto has = [](const std::vector<uint32_t>& list, uint32_t id) {
            return std::find(list.begin(), list.end(), id) != list.end();
        };
        std::vector<uint32_t> worn;
        for (uint32_t id : m_worn)
            if (has(gear, id))
                worn.push_back(id);
            else
                RemoveItem(actor, id);
        bool changed = worn.size() != m_worn.size();
        for (uint32_t id : gear)
        {
            if (has(worn, id))
                continue;
            if (AddAndEquip(actor, id))
            {
                worn.push_back(id);
                changed = true;
            }
            else
                LOG_WARN("%s: item %08X isn't armour or a weapon in this game (mods differ?)", m_who.c_str(), id);
        }
        m_worn = std::move(worn);
        if (changed)
            LOG_DEBUG("%s now wears %zu items", m_who.c_str(), m_worn.size());
    }

    // Drawn or put away, like them. Told once per change, then checked: drawing takes a moment (the animation).
    void Puppet::UpdateWeaponOut(TESObjectREFR* actor, bool want, uint32_t now)
    {
        if (want != m_weaponOut)
        {
            m_weaponOut = want;
            if (want && InScriptAnimation(actor))
                StopIdle(actor);  // an idle (looping ones never end) holds the weapon back (step 5.3 test)
            SetAlert(actor, want);  // its AI puts the weapon away again after ~0.6 s unless it's alert (seen in the lab)
            SetWeaponOut(actor, want);
            m_nextWeaponCheck = now + kDrawTakesMs;
            LOG_DEBUG("%s: told it to %s its weapon", m_who.c_str(), want ? "draw" : "put away");
            return;
        }
        if (!coop::IsDue(now, m_nextWeaponCheck))
            return;
        m_nextWeaponCheck = now + kWeaponCheckEveryMs;
        bool out = (StanceFlags(actor) & coop::Pose::kWeaponOut) != 0;
        if (out != want)
        {
            // (Step 5.3 test: two NPC copies never drew. What they were doing: animation action, idle, and the
            // inspector's AI bits: +4 fighting, +8 restrained, +16 knocked, +128 sitting, /2048 package+1.)
            uint32_t ai = 0;
            AiStateBits(actor, ai);
            LOG_INFO("%s: its weapon is %s but should be %s; telling it again (action %d, idle %08X, AI bits %u)",
                m_who.c_str(), out ? "out" : "away", want ? "out" : "away", AnimAction(actor), PlayedIdle(actor), ai);
            if (want && InScriptAnimation(actor))
                StopIdle(actor);
            SetAlert(actor, want);
            SetWeaponOut(actor, want);
            m_nextWeaponCheck = now + kDrawTakesMs;
        }
    }

    void Puppet::Attack(TESObjectREFR* actor, uint8_t shots, bool release)
    {
        if (!actor)
            return;
        if (release)
        {
            // Their throw left their hand: ours throws the harmless grenade (or mine) now (step 8.8).
            ReleaseThrow(actor, m_worn);
            LOG_DEBUG("%s's throw left its hand", m_who.c_str());
            return;
        }
        // Attacking with the weapon away left it stuck, unable to draw again (seen in the lab).
        if (!(StanceFlags(actor) & coop::Pose::kWeaponOut))
        {
            LOG_DEBUG("%s: didn't attack (%u shots): its weapon isn't out", m_who.c_str(), shots);
            return;
        }
        PlayAttack(actor, m_worn, shots);
        LOG_DEBUG("%s attacks (%u shots)", m_who.c_str(), shots);
    }

    void Puppet::LogStats(uint32_t now)
    {
        if (!coop::IsDue(now, m_nextStats))
            return;
        bool first = m_nextStats == 0;
        m_nextStats = now + m_statsEveryMs;
        if (first || m_frames == 0)
            return;
        // 64 game units is about 1 metre.
        LOG_INFO("%s: off by %.0f on average (most %.0f) units; put back %u times; jumped %u times; legs took %u ms "
            "on average (most %u) for %u changes", m_who.c_str(), m_errorSum / m_frames, m_errorMax, m_snaps, m_jumps,
            m_lagCount ? m_lagSum / m_lagCount : 0, m_lagMax, m_lagCount);
        if (m_switchedOff || m_replaced || m_ignored)
            LOG_INFO("%s: its AI switched our legs off %u times, replaced our moves %u times, and the game didn't "
                "use them %u times, in %u frames", m_who.c_str(), m_switchedOff, m_replaced, m_ignored, m_checked);
        m_errorSum = m_errorMax = 0;
        m_frames = m_snaps = m_lagMax = m_lagCount = m_lagSum = m_jumps = 0;
        m_checked = m_switchedOff = m_replaced = m_ignored = 0;
    }
}
