#pragma once

// A puppet (DESIGN rule 7): an actor we drive to copy someone, from the puppet lab (step 1). Its legs are
// steered (coop::Steer) onto where they are, asked every frame so its AI can't take them back; it faces their
// way, draws and holsters, attacks and jumps when they do, wears their gear, and is put back in place if it
// drifts too far. Used for friends' characters (StandIns) and the lab's mirror. Only call from the game thread.

#include "coop/smoothing.h"
#include "coop/steer.h"

#include <cstdint>
#include <string>
#include <vector>

class TESObjectREFR;

namespace game
{
    class Puppet
    {
    public:
        // who: for the log ("Mirror", "Alex's character"). Stats are logged every statsEveryMs.
        void Start(std::string who, uint32_t statsEveryMs);
        void Release(TESObjectREFR* actor);  // give its legs back to its AI (stopping, or downed)
        // Every frame. want: where they are and what they do (smoothed; moves has coop::moves::kJump while they jump).
        // The actor must be loaded and in the same place as want (otherwise fetch it there first).
        // latest (optional): their newest known pose, to measure how far behind it the puppet is.
        void Update(TESObjectREFR* actor, const coop::Pose& want, uint32_t now, const coop::Pose* latest = nullptr);
        void SetGear(TESObjectREFR* actor, const std::vector<uint32_t>& gear);  // base ids they have equipped
        void Attack(TESObjectREFR* actor, uint8_t shots, bool release = false);  // they shot, swung or threw

    private:
        void UpdateWeaponOut(TESObjectREFR* actor, bool want, uint32_t now);
        void LogStats(uint32_t now);

        std::string m_who;
        uint32_t m_statsEveryMs = 1000;
        coop::Steer m_steer;
        // Where it should be last frame, to know how fast that spot moves.
        float m_lastWantX = 0, m_lastWantY = 0, m_wantVx = 0, m_wantVy = 0;
        uint32_t m_lastWantAt = 0;
        bool m_jumping = false;      // they're in a jump (so it jumps once per jump)
        uint32_t m_putBackAt = 0;    // when it was last put back (it takes a moment to land there)
        std::vector<uint32_t> m_worn;  // base ids of what it wears (weapons as harmless copies, game::AddAndEquip)
        // Weapon drawn: what we last told it, and when to check it did.
        bool m_weaponOut = false;
        uint32_t m_nextWeaponCheck = 0;
        // Its legs: the flags we last asked for, when, and whether it has taken them yet.
        uint32_t m_asked = 0, m_askedAt = 0;
        bool m_waiting = false;
        // Since the last stats line.
        uint32_t m_nextStats = 0;
        float m_errorSum = 0, m_errorMax = 0;
        uint32_t m_frames = 0, m_snaps = 0, m_lagMax = 0, m_lagCount = 0, m_lagSum = 0, m_jumps = 0;
        // Frames where last frame's ask was switched off, replaced with other moves, or not used (out of checked).
        uint32_t m_checked = 0, m_switchedOff = 0, m_replaced = 0, m_ignored = 0;
        // How far behind their newest pose it is while they move (ms), and how often its gait (stop, walk, run)
        // changed (Alex, 2026-10-30: delay, jitter and sliding).
        uint32_t m_behindSum = 0, m_behindMax = 0, m_behindCount = 0, m_gaitChanges = 0, m_speedChanges = 0;
        bool m_wasMoving = false, m_wasRunning = false;
        int m_speedMult = 100;  // what its speed is set to (catching up: Alex, 2026-10-30)
        uint32_t m_speedSetAt = 0;
        float m_ownRunSpeed = 0;  // its running speed at 100% (from the game's cached value), 0 not known yet
    };
}
