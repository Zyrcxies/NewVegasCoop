#include "game/dialogue.h"

#include "coop/clock.h"
#include "game/addresses.h"
#include "game/game.h"
#include "game/npcs.h"
#include "game/progress.h"
#include "log.h"

#include <windows.h>

#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace game
{
    namespace
    {
        constexpr uint32_t kOurConversationMs = 2000;  // a conversation opening this soon after our activate is ours
        constexpr uint32_t kSettleConversationMs = 500;  // one we close is closed once open this long
        // (Closing it as it opened crashed the laptop, 2026-10-15, inside JIP's CloseActiveMenu.)

        Dialogue* g_dialogue = nullptr;
        uint8_t* g_original = nullptr;  // the game's RunResultScript, as it was before our hook

        void RunOriginal(void* info, uint32_t onEnd, TESObjectREFR* speaker)
        {
            using Fn = void(__thiscall*)(void*, uint32_t, TESObjectREFR*);
            DialogueLineRunning(true);  // (what it does to a player stays theirs: not mirrored to friends)
            reinterpret_cast<Fn>(g_original)(info, onEnd, speaker);
            DialogueLineRunning(false);
        }

        // In place of the game's TESTopicInfo::RunResultScript (thiscall with two arguments: a fastcall with the
        // unused edx takes them the same way and pops the same).
        void __fastcall RunResultScriptHook(void* info, void*, uint32_t onEnd, TESObjectREFR* speaker)
        {
            if (g_dialogue)
                g_dialogue->Line(info, onEnd, speaker);
            else
                RunOriginal(info, onEnd, speaker);
        }

        void JumpTo(uint8_t* from, const void* to)
        {
            from[0] = 0xE9;
            int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(to) - reinterpret_cast<uintptr_t>(from + 5));
            std::memcpy(from + 1, &rel, 4);
        }

        // Puts our hook at the start of RunResultScript. Its first 6 bytes (or another plugin's jump there) move to
        // g_original, which then carries on in the game's code.
        bool InstallHook()
        {
            static const uint8_t kStart[6] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C};
            auto* at = reinterpret_cast<uint8_t*>(addr::kTopicInfoRunResultScript);
            auto* back = static_cast<uint8_t*>(VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE,
                PAGE_EXECUTE_READWRITE));
            if (!back)
                return false;
            size_t length = 6;
            if (std::memcmp(at, kStart, 6) == 0)
            {
                std::memcpy(back, at, 6);
                JumpTo(back + 6, at + 6);
            }
            else if (at[0] == 0xE9)
            {
                // Another plugin's hook: ours runs first, then theirs (which runs the game's).
                int32_t rel;
                std::memcpy(&rel, at + 1, 4);
                JumpTo(back, at + 5 + rel);
                length = 5;
                LOG_INFO("Dialogue: another plugin hooks the game's dialogue scripts too; ours goes first and passes on");
            }
            else
            {
                VirtualFree(back, 0, MEM_RELEASE);
                LOG_ERROR("Dialogue: the game's dialogue script function isn't as expected (%02X %02X %02X); "
                    "conversations won't be shared", at[0], at[1], at[2]);
                return false;
            }
            DWORD old;
            if (!VirtualProtect(at, length, PAGE_EXECUTE_READWRITE, &old))
            {
                VirtualFree(back, 0, MEM_RELEASE);
                return false;
            }
            g_original = back;
            JumpTo(at, reinterpret_cast<const void*>(&RunResultScriptHook));
            if (length == 6)
                at[5] = 0x90;  // (never run: the way back starts after it)
            VirtualProtect(at, length, old, &old);
            FlushInstructionCache(GetCurrentProcess(), at, length);
            return true;
        }
    }

    void Dialogue::Install(coop::Coop& coop, const Npcs& npcs, Progress& progress)
    {
        if (g_original)
            return;
        m_coop = &coop;
        m_npcs = &npcs;
        m_progress = &progress;
        m_thread = GetCurrentThreadId();
        if (InstallHook())
        {
            g_dialogue = this;
            LOG_INFO("Dialogue: hooked dialogue lines' scripts");
        }
        if (InstallSayHooks())
            LOG_INFO("Dialogue: other players' conversations can be heard (hooked the game's line picking)");
        else
            LOG_ERROR("Dialogue: the game's line picking isn't as expected; other players' conversations are "
                "subtitles only");
    }

    uint32_t Dialogue::HostRef(TESObjectREFR* ref) const
    {
        if (!ref)
            return 0;
        return m_coop->GetRole() == coop::Role::Host ? RefId(ref) : m_npcs->HostRefOf(RefId(ref));
    }

    void Dialogue::Line(void* info, uint32_t onEnd, TESObjectREFR* speaker)
    {
        // Another player's line our NPC is saying (SayLine): its script is theirs (the host runs it once).
        // (Not our own conversation's: we may be talking to them ourselves now, with the same line.)
        if (speaker && speaker != DialoguePartner() && SaidForOthers(speaker, info))
            return;
        // Only our player's own conversation, in co-op, on the game's main thread; everything else as usual.
        if (m_replaying || GetCurrentThreadId() != m_thread || !m_coop || !m_coop->InGame() || !speaker)
            return RunOriginal(info, onEnd, speaker);
        TESObjectREFR* partner = DialoguePartner();
        LOG_DEBUG("Dialogue: line %08X (%s) %s, said by %s %08X; conversation screen %s %08X", TopicInfoId(info),
            TopicInfoPrompt(info).c_str(), onEnd ? "ends" : "starts", Name(speaker), RefId(speaker),
            partner ? "with" : "closed", partner ? RefId(partner) : 0);
        bool host = m_coop->GetRole() == coop::Role::Host;
        // An NPC's line outside a conversation with us runs in every game (Dead Money's intro slides are its
        // narrator's lines swapping a texture). Its screen effects and control changes count only in the host's
        // game (game::ScreenFromHost: the narrator's fades once left a friend's screen black).
        if (speaker != partner)
            return RunOriginal(info, onEnd, speaker);
        uint32_t now = GetTickCount();
        bool ours = host || (m_openSince ? m_ours : now - m_lastActivate <= kOurConversationMs);
        if (!ours || !m_coop->MayTalk())
        {
            // Someone else is talking (ours is being closed), or an NPC started it with us here (the host's game has
            // those).
            ++m_linesSkipped;
            LOG_INFO("Dialogue: line %08X's script not run here (%s)", TopicInfoId(info),
                ours ? "someone else is talking" : "an NPC started this conversation here");
            return;
        }
        if (host)
            return RunOriginal(info, onEnd, speaker);
        // Friend: ours runs here as usual; what it gives us is ours alone (the host's game gives the others theirs).
        int32_t xp = PlayerXp(), karma = PlayerKarma();
        RunOriginal(info, onEnd, speaker);
        int32_t gainedXp = PlayerXp() - xp, gainedKarma = PlayerKarma() - karma;
        if (gainedXp || gainedKarma)
            m_progress->Expect(gainedXp, gainedKarma);
        uint32_t hostSpeaker = HostRef(speaker);
        if (!hostSpeaker)
            return LOG_WARN("Dialogue: %s isn't in the host's game; line %08X not sent", Name(speaker),
                TopicInfoId(info));
        // (The host takes lines only from whoever is talking: it hears that first. The greeting's line runs before
        // our Update sees the conversation open: two-PC test 2026-10-15, Trudy's greetings were dropped.)
        if (!m_reportedTalking || m_reportedNpc != hostSpeaker)
        {
            m_coop->SetMyConversation(true, hostSpeaker);
            m_reportedTalking = true;
            m_reportedNpc = hostSpeaker;
        }
        m_coop->SendDialogueLine({TopicInfoId(info), onEnd != 0, hostSpeaker});
        ++m_linesSent;
        LOG_DEBUG("Dialogue: sent line %08X (%s) to the host (we gained %d XP, %d karma)", TopicInfoId(info),
            onEnd ? "end" : "start", gainedXp, gainedKarma);
    }

    void Dialogue::RunForFriend(uint8_t player, const net::DialogueLine& line)
    {
        m_lastFriendLine = player;
        m_lastFriendLineAt = GetTickCount();
        void* info = TopicInfo(line.info);
        TESObjectREFR* speaker = LookupRef(line.speaker);
        if (!info || !speaker || !IsActor(speaker))
            return LOG_WARN("Dialogue: %s's line %08X said by %08X: %s", m_coop->NameOf(player).c_str(), line.info,
                line.speaker, !info ? "not a dialogue line" : "no such character here");
        if (!HasResultScript(info, line.onEnd))
            return;
        if (uint16_t command = ScreenCommandIn(info, line.onEnd))
            return LOG_INFO("Dialogue: %s's line %08X opens a screen (command %04X): not run here",
                m_coop->NameOf(player).c_str(), line.info, command);
        // What the line gives or takes from the player is the talker's (their game did it): ours goes back.
        std::vector<Item> before, after;
        bool counted = Contents(Player(), before);
        int32_t xp = PlayerXp(), karma = PlayerKarma();
        m_replaying = true;
        RunOriginal(info, line.onEnd ? 1 : 0, speaker);
        m_replaying = false;
        ++m_linesRun;
        int putBack = 0;
        if (counted && Contents(Player(), after))
        {
            std::map<uint32_t, int32_t> change;
            for (const Item& i : after)
                change[i.id] += i.count;
            for (const Item& i : before)
                change[i.id] -= i.count;
            for (auto& [id, delta] : change)
                if (delta)
                {
                    ChangeItem(Player(), id, -delta);
                    ++putBack;
                    LOG_INFO("Dialogue: %s's line changed our player's %08X by %d; put back",
                        m_coop->NameOf(player).c_str(), id, delta);
                }
        }
        else
            LOG_WARN("Dialogue: couldn't count our inventory around %s's line", m_coop->NameOf(player).c_str());
        int32_t gainedXp = PlayerXp() - xp, gainedKarma = PlayerKarma() - karma;
        if (gainedXp || gainedKarma)
        {
            m_progress->Expect(gainedXp, gainedKarma);
            m_coop->ShareProgressFor(player, gainedXp, gainedKarma);
        }
        LOG_INFO("Dialogue: ran %s's line %08X (\"%s\", %s) said by %s; %d item kinds put back, %d XP, %d karma",
            m_coop->NameOf(player).c_str(), line.info, TopicInfoPrompt(info).c_str(), line.onEnd ? "end" : "start",
            Name(speaker), putBack, gainedXp, gainedKarma);
    }

    void Dialogue::LabRun(uint32_t info, bool onEnd, uint32_t speaker)
    {
        if (g_dialogue && g_dialogue->m_coop)
            g_dialogue->RunForFriend(1, {info, onEnd, speaker});
    }

    void Dialogue::Update(uint32_t now)
    {
        m_thread = GetCurrentThreadId();
        if (!m_coop || !m_coop->InGame())
        {
            m_openSince = 0;
            m_reportedTalking = false;
            m_reportedNpc = 0;
            return;
        }
        bool host = m_coop->GetRole() == coop::Role::Host;
        if (IsControlPressed(layout::kControlActivate))
            m_lastActivate = now;
        bool open = InDialogue();
        TESObjectREFR* partner = open ? DialoguePartner() : nullptr;
        if (!open)
        {
            // Ours went into another screen (trading, repairs): the conversation coming back after it is ours too
            // (two-PC test 2026-10-15: Trudy's came back after bartering and was closed as an NPC's).
            if (m_openSince && m_ours && !m_closed)
                m_afterOurs = true;
            if (m_afterOurs && InMenu())
                m_lastActivate = now;
            else
                m_afterOurs = false;
            m_openSince = 0;
            m_closed = false;
        }
        else if (!m_openSince)
        {
            m_openSince = now ? now : 1;
            m_ours = host || now - m_lastActivate <= kOurConversationMs;
            LOG_INFO("Dialogue: a conversation with %s (%08X) opened (%s)", partner ? Name(partner) : "?",
                partner ? RefId(partner) : 0, m_ours ? "ours" : "an NPC started it");
            // One of the group's companions (DESIGN: companions are the party's): our co-op menu first.
            uint8_t leader = 0;
            uint32_t ref = partner ? HostRef(partner) : 0;
            if (!host && m_ours && ref && m_coop->CompanionLeader(ref, leader) &&
                !(ref == m_talkAllowed && now < m_talkAllowedUntil))
            {
                m_closed = true;
                CloseMenu();
                m_companionAsk = ref;
                LOG_INFO("Dialogue: %s is a companion; asking what to tell it", Name(partner));
            }
        }

        // Tell the host (or as the host, everyone) who we're talking to.
        // (Trading or another screen our conversation opened still counts: two-PC test 2026-10-15, the laptop could
        // talk to Chet while the host traded with him.)
        bool inScreen = !open && m_afterOurs && m_reportedTalking;
        bool talking = (open && m_ours && !m_closed) || inScreen;
        uint32_t npc = inScreen ? m_reportedNpc : talking ? HostRef(partner) : 0;
        if (talking != m_reportedTalking || npc != m_reportedNpc)
        {
            m_coop->SetMyConversation(talking, npc);
            m_reportedTalking = talking;
            m_reportedNpc = npc;
        }

        // One conversation at a time (DESIGN): ours closes if someone else is talking. A friend's game also closes
        // conversations an NPC starts with it (the host's game has them).
        if (open && !m_closed && now - m_openSince >= kSettleConversationMs && (!m_ours || !m_coop->MayTalk()))
        {
            m_closed = true;
            ++m_closedCount;
            CloseMenu();
            if (m_ours)
            {
                std::string who = m_coop->NameOf(m_coop->Talk().Talker());
                LOG_INFO("Dialogue: %s is talking; our conversation closed", who.c_str());
                Notify((who + " is already talking to someone. One conversation at a time.").c_str());
            }
            else
                LOG_INFO("Dialogue: an NPC started a conversation with us; closing it (the host's game runs those)");
        }

        if (host)
            for (const coop::Coop::DialogueLineFrom& l : m_coop->TakeDialogueLines())
                RunForFriend(l.playerId, l.line);
        Subtitles(open && talking && m_coop->MayTalk(), partner, now);

        if (coop::IsDue(now, m_nextLog))
        {
            if (m_nextLog && (m_linesSent || m_linesRun || m_linesSkipped || m_closedCount || m_subtitlesSent ||
                m_subtitlesShown || m_subtitlesFar || m_linesSaid))
                LOG_INFO("Dialogue: %u lines sent to the host, %u friends' lines run, %u not run here, %u "
                    "conversations closed; spoken lines: %u sent, %u said here, %u as subtitles only, %u too far "
                    "away, %u stopped", m_linesSent, m_linesRun, m_linesSkipped, m_closedCount, m_subtitlesSent,
                    m_linesSaid, m_subtitlesShown, m_subtitlesFar, m_stops);
            m_nextLog = now + 60000;
            m_linesSent = m_linesRun = m_linesSkipped = m_closedCount = 0;
            m_subtitlesSent = m_subtitlesShown = m_subtitlesFar = m_linesSaid = m_stops = 0;
        }
    }

    void Dialogue::Subtitles(bool talking, TESObjectREFR* partner, uint32_t now)
    {
        // Ours: each new response the one we're talking to says goes to everyone else, and when they stop (the
        // line skipped, or done) so does that (DESIGN: everyone nearby hears it).
        constexpr uint32_t kStartTalkingMs = 1000;  // seen talking this soon after a response starts, or never
        Spoken spoken = talking ? SpokenNow() : Spoken{};
        if (talking && spoken.id && spoken.id != m_lastSpoken && *spoken.text)
        {
            m_lastSpoken = spoken.id;
            if (uint32_t speaker = HostRef(partner))
            {
                net::Subtitle s;
                s.speaker = speaker;
                s.topic = spoken.topic;
                s.info = spoken.info;
                s.response = static_cast<uint8_t>(spoken.response);
                s.text = spoken.text;
                m_coop->ShareSubtitle(s);
                m_spokenSpeaker = speaker;
                m_spokenAt = now;
                m_sawTalking = false;
                m_spokenLive = true;
                ++m_subtitlesSent;
                LOG_DEBUG("Dialogue: %s says \"%s\" (line %08X response %d; shared)", Name(partner), spoken.text,
                    spoken.info, spoken.response);
            }
        }
        if (m_spokenLive)
        {
            bool saying = talking && IsTalking(partner);
            m_sawTalking = m_sawTalking || saying;
            if (!talking || (m_sawTalking && !saying))
            {
                net::Subtitle s;
                s.speaker = m_spokenSpeaker;
                s.stop = true;
                m_coop->ShareSubtitle(s);
                m_spokenLive = false;
                LOG_DEBUG("Dialogue: stopped talking (shared)");
            }
            else if (!m_sawTalking && now - m_spokenAt > kStartTalkingMs)
                m_spokenLive = false;  // (never seen talking: nothing to stop)
        }
        if (!talking)
            m_lastSpoken = 0;

        // Everyone else's: our copy of the NPC says it (voice, mouth, subtitle) if we're near.
        bool host = m_coop->GetRole() == coop::Role::Host;
        coop::Pose me, at;
        bool placed = GetPose(Player(), me);
        for (const net::Subtitle& s : m_coop->TakeSubtitles())
        {
            if (s.stop)
            {
                if (s.speaker == m_heardSpeaker)
                {
                    StopSound(m_heard);
                    EndSubtitle(m_heardText);
                    ++m_stops;
                }
                continue;
            }
            // (A new line cuts the last one short, as it did for the talker.)
            StopSound(m_heard);
            EndSubtitle(m_heardText);
            m_heardText = s.text;
            TESObjectREFR* speaker = host ? LookupRef(s.speaker) : m_npcs->OurRefOf(s.speaker);
            if (!placed || !speaker || !GetPose(speaker, at) || !coop::SamePlace(me, at))
            {
                ++m_subtitlesFar;
                continue;
            }
            m_heardSpeaker = s.speaker;
            const char* name = Name(speaker);
            if (s.info && SayLine(speaker, s.topic, s.info, s.response, m_heard))
                ++m_linesSaid;
            else
            {
                m_heardText = *name ? std::string(name) + ": " + s.text : s.text;
                ShowSubtitle(speaker, m_heardText.c_str());
                ++m_subtitlesShown;
            }
            LOG_DEBUG("Dialogue: %s's conversation: %s says \"%s\" (line %08X response %u; sound %08X)",
                m_coop->NameOf(s.playerId).c_str(), name, s.text.c_str(), s.info, s.response, m_heard.id);
        }
    }
}
