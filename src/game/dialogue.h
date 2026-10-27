#pragma once

// Conversations (step 7.4, DESIGN: whoever starts a conversation picks the answers). One at a time in the group:
// the host's game keeps the lock (coop::TalkLock); anyone else's conversation is closed with a message. A friend's
// conversation runs in their own game (their skills count), and each line's script runs in the host's game too, so
// what it does to the world (quests, NPCs) happens there; what it gives or takes from the player stays the talker's
// (the host's game puts its own player's items back, and its XP and karma go to everyone but the talker, who has
// them already).

#include "coop/coop.h"
#include "game/game.h"

#include <cstdint>
#include <string>

class TESObjectREFR;

namespace game
{
    class Npcs;
    class Progress;

    class Dialogue
    {
    public:
        // Once, when the game has loaded: our hook on dialogue lines' scripts (passes everything on outside co-op).
        void Install(coop::Coop& coop, const Npcs& npcs, Progress& progress);
        void Update(uint32_t now);  // every frame
        // Friend: our player started talking to one of the group's companions; the conversation was closed for our
        // co-op menu (follow me, wait, talk). The host's id for it, once (0 none). AllowTalk: they picked Talk, so
        // a conversation with it till `until` goes ahead.
        uint32_t TakeCompanionAsk() { uint32_t r = m_companionAsk; m_companionAsk = 0; return r; }
        void AllowTalk(uint32_t hostRef, uint32_t until) { m_talkAllowed = hostRef; m_talkAllowedUntil = until; }
        // Host: the friend whose conversation line last ran here, and when (false: none yet).
        bool LastFriendLine(uint8_t& player, uint32_t& at) const
        {
            player = m_lastFriendLine;
            at = m_lastFriendLineAt;
            return m_lastFriendLineAt != 0;
        }

        // Lab (coop friendline): our game runs this line as if friend 1 said it (host).
        static void LabRun(uint32_t info, bool onEnd, uint32_t speaker);
        // The hook's: a line's script is about to run (on any thread).
        void Line(void* info, uint32_t onEnd, TESObjectREFR* speaker);

    private:
        void RunForFriend(uint8_t player, const net::DialogueLine& line);
        void Subtitles(bool talking, TESObjectREFR* partner, uint32_t now);
        uint32_t HostRef(TESObjectREFR* ref) const;
        coop::Coop* m_coop = nullptr;
        const Npcs* m_npcs = nullptr;
        Progress* m_progress = nullptr;
        uint32_t m_thread = 0;          // the game's main thread
        bool m_replaying = false;       // host: running a friend's line
        uint32_t m_lastActivate = 0;    // when our player last pressed the activate key
        uint32_t m_openSince = 0;       // when the conversation open now opened (0: none open)
        bool m_ours = false;            // it's one our player started (friend; the host's are all its own)
        bool m_closed = false;          // we closed it (waiting for the screen to go)
        bool m_afterOurs = false;       // ours ended into another screen (trading...); the game comes back to it
        uint32_t m_companionAsk = 0, m_talkAllowed = 0, m_talkAllowedUntil = 0;
        uint8_t m_lastFriendLine = 0;
        uint32_t m_lastFriendLineAt = 0;
        bool m_reportedTalking = false;
        uint32_t m_reportedNpc = 0;
        uintptr_t m_lastSpoken = 0;     // the response we last shared (0 none)
        uint32_t m_spokenSpeaker = 0;   // who said it (the host's id)
        uint32_t m_spokenAt = 0;        // when we shared it
        bool m_sawTalking = false;      // the one saying it has been seen talking since
        bool m_spokenLive = false;      // shared, and not yet said to have stopped
        Sound m_heard;                  // the voice of the line we last heard someone else's NPC say
        uint32_t m_heardSpeaker = 0;
        std::string m_heardText;
        // Logged now and then (CLAUDE.md: measure).
        uint32_t m_linesSent = 0, m_linesRun = 0, m_linesSkipped = 0, m_closedCount = 0, m_nextLog = 0;
        uint32_t m_subtitlesSent = 0, m_subtitlesShown = 0, m_subtitlesFar = 0, m_linesSaid = 0, m_stops = 0;
    };
}
