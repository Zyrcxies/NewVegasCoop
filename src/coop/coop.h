#pragma once

// Who's in the game: hosting, joining, and the join handshake.
// Game-independent (used by the plugin, the fake friend tool and tests).
// Call Update() every frame from one thread.

#include "coop/containers.h"
#include "coop/dialogue.h"
#include "coop/loading.h"
#include "coop/mod_transfer.h"
#include "coop/mods.h"
#include "coop/chat.h"
#include "coop/voice.h"
#include "coop/desync.h"
#include "coop/game_mode.h"
#include "coop/doors.h"
#include "coop/saves.h"
#include "coop/smoothing.h"
#include "coop/waitvote.h"
#include "coop/world_items.h"
#include "net/protocol.h"
#include "net/session.h"

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace coop
{
    constexpr size_t kMaxFriends = 4;
    constexpr uint32_t kHelloTimeoutMs = 5000;
    constexpr uint32_t kModIdleMs = 120000;
    constexpr uint32_t kApprovalTimeoutMs = 60000;  // the host not answering "let them in?" this long is a no
    constexpr uint32_t kMaxExplosionsPerSecond = 16;  // from one player; more are dropped (a bad game can't flood us)
    constexpr uint32_t kMaxHostExplosionsPerSecond = 32;  // from the host: its own and those it passes on
    constexpr size_t kMaxQueuedExplosions = 64;

    // Host: someone asking to join (step 8.5: only who the host lets in joins).
    struct JoinAsk
    {
        uint32_t id = 0;
        std::string name;
        bool wantsFiles = false;  // their mods differ: they'd fetch some of ours first
    };  // a friend fixing their mods who asks for nothing this long is dropped
    constexpr uint32_t kGameTimeEveryMs = 5000;    // host: how often friends hear the host's clock
    constexpr uint8_t kHostPlayerId = 0;
    constexpr uint32_t kPoseIntervalMs = 50;       // send our position up to 20 times a second while moving
    constexpr uint32_t kIdlePoseIntervalMs = 1000;  // and once a second while standing still
    constexpr float kMaxHitDamage = 10000;  // more than this from one hit is a bad message
    constexpr size_t kMaxQueuedHits = 64;   // hits waiting for the game; more are dropped
    constexpr size_t kMaxQueuedAttacks = 32;  // others' attacks waiting for the game; more are dropped
    constexpr uint32_t kHealthIntervalMs = 200;  // send our health at most this often, when it changes
    constexpr uint32_t kBleedOutMs = 30000;  // downed this long: game over (DESIGN: downed, not dead)
    constexpr float kReviveDistance = 600;   // host's check on a revive, in game units (generous for lag and blasts)
    constexpr float kReviveHealth = 0.3f;    // revived players get back this share of their max health
    // Doors and fast travel (DESIGN: doors pull everyone). Game units; 64 is about a metre.
    constexpr float kTeleportDistance = 3000;   // moved further than this from one frame to the next: travelled
    constexpr float kSamePlaceDistance = 1500;  // this close, in the same worldspace or room: already there
    constexpr uint32_t kTravelCooldownMs = 5000;  // someone going elsewhere this soon after the group moved is
                                                  // brought back (two doors at once)
    // The host's NPCs (DESIGN rule 5): friends' copies are puppets.
    constexpr size_t kMaxNpcs = net::kMaxNpcsPerMessage;  // host: shared at once (the nearest)
    constexpr size_t kMaxKnownNpcs = 2 * kMaxNpcs;        // friend: kept track of at once; more are ignored
    constexpr uint32_t kNpcTimeoutMs = 3000;  // friend: not heard of for this long: back to its own AI
    constexpr uint32_t kNpcGearResendMs = 5000;  // host: an NPC's gear again (it may have reached a friend first)
    constexpr uint32_t kDoorResendMs = 2000;  // host: every door near the group again (changes go at once)
    constexpr size_t kMaxQueuedDoorUses = 32;  // host: friends' door uses not taken yet
    constexpr size_t kMaxQueuedItemChanges = 64;  // host: friends' container changes not taken yet
    constexpr size_t kMaxQueuedQuests = 4096;     // friend: quest updates not taken yet
    constexpr uint32_t kItemResendMs = 3000;  // host: every item near the group again (changes go at once)
    constexpr float kItemSendMoveDistance = 16;  // host: an item moved this far is sent again
    constexpr float kItemSendTurn = 0.1f;        // (radians) or turned this much (step 8.9)
    // Host: a friend's moves of items (step 8.9). More in a second from one friend are dropped; one taking an item
    // further than kMaxItemMoveReach from where it lies here too (a bad game can't send things across the map).
    constexpr uint32_t kMaxItemMovesPerSecond = 64;
    constexpr float kMaxItemMoveReach = 4096;
    constexpr size_t kMaxQueuedItemMoves = 128;

    // Same worldspace or room, and close.
    bool SamePlace(const Pose& a, const Pose& b);

    // At 0 health a player is downed, not dead (DESIGN). Max 0 means we haven't heard their health yet.
    inline bool IsDowned(float health, float maxHealth) { return maxHealth > 0 && health <= 0; }

    enum class Role { None, Host, Friend };
    // FixingMods: the host's mods aren't ours; we may fetch them (step 8.6), then restart and join again.
    enum class FriendState { Connecting, WaitingForWelcome, FixingMods, Joined, Rejected, Disconnected };

    struct Player
    {
        uint8_t id = 0;
        std::string name;
        bool joined = false;
        uint32_t connectedAt = 0;  // tick count, for the hello timeout
        bool fixingMods = false;   // not joined: their mods aren't ours, they may fetch ours (step 8.6)
        std::string from;          // who the connection is from (an IP address)
        bool awaitingApproval = false;  // the host is being asked to let them in (step 8.5)
        bool wantsFiles = false;   // their mods aren't ours: letting them in sends them ours first
        uint32_t askId = 0;
        uint32_t askedAt = 0;
        uint32_t explosionsSince = 0, explosions = 0;  // their explosions in this second (step 8.8: a limit)
        uint32_t itemMovesSince = 0, itemMoves = 0;    // their item moves in this second (step 8.9: a limit)
        net::DesyncReport desyncs;                     // what their game last said it disagrees with ours on
        ChatLimiter chatLimit;                         // their chat lines (a spam limit)
        VoiceLimiter voiceLimit;                       // their voice (a spam limit)
        bool fighting = false;                         // enemies are going for their character (as we told them)
        uint32_t playerHitsSince = 0, playerHits = 0;  // their hits on players in this second (friendly fire)
        uint32_t lastAsk = 0;      // tick count of their last ask for a piece (or of our ModsDiffer)
    };

    // Another player in the game, as we see them.
    struct RemotePlayer
    {
        std::string name;
        Smoother motion;
        std::vector<uint32_t> gear;  // base form ids, unchecked (the game side checks them)
        uint32_t gearChanges = 0;    // goes up each time gear changes
        float health = 0, maxHealth = 0;  // max 0: not heard yet
        bool Downed() const { return IsDowned(health, maxHealth); }
    };

    // One of the host's NPCs, as a friend sees it.
    struct RemoteNpc
    {
        Smoother motion;
        uint32_t heardAt = 0;  // our tick count
        uint32_t stateTime = 0;  // host's clock for the fields below (newest kept: messages can arrive late)
        bool dead = false, down = false, sitting = false;
        uint16_t limbs = 0;  // dead: body parts gone (a bit each)
        uint8_t pile = 0;    // dead: its body turned to goo or ash (net::kPile...)
        float health = 0, maxHealth = 0;
        uint32_t idle = 0;  // idle animation form id, unchecked (the game side checks it)
        uint32_t weapon = 0;  // base form id of the weapon it holds, unchecked
        uint32_t base = 0;    // made while playing: what to make our copy from, unchecked
        std::vector<uint32_t> gear;  // made while playing: what it wears (unchecked)
        uint32_t gearChanges = 0;    // goes up each time gear changes
    };

    // A friend's hit on something in the host's world (host only). Unchecked: the game side checks the target.
    struct HitReport
    {
        uint8_t playerId = 0;
        uint32_t targetRef = 0;
        float damage = 0;
    };

    // Someone else shot or swung: their character does too.
    struct AttackReport
    {
        uint8_t playerId = 0;
        uint8_t shots = 0;  // shots fired, or 1 for a swing or a throw's start
        bool release = false;  // a throw left their hand (step 8.8)
    };

    class Coop
    {
    public:
        // Our game's mod files (ScanMods): the host sends them to a joining friend, whose game compares; joining
        // needs the same plugins and archives (DESIGN rule 2). Before Host or Join. Empty: not checked.
        void SetMods(ModList mods) { m_mods = std::move(mods); m_modsPrint = ModsFingerprint(m_mods); }
        // Where mod files are (host: what it sends from Data) and go (friend). Before Host or Join.
        void SetModFolders(ModFolders folders) { m_modFolders = std::move(folders); }
        bool Host(uint16_t port, const std::string& myName);
        // checkModsOnly: only compare our mods with the host's (and fix them if they differ), from the main menu;
        // the host says whether they match and we leave (ModsMatched).
        bool Join(const std::string& address, uint16_t port, const std::string& myName, bool checkModsOnly = false);
        // The same through another connection (Steam, step 8.5), already started (as host, or towards the host).
        void HostOn(std::unique_ptr<net::Transport> started, const std::string& myName);
        // Host through the IP connection (port) and another (Steam) at once. False if the port is taken.
        bool HostBoth(uint16_t port, std::unique_ptr<net::Transport> started, const std::string& myName);
        void JoinOn(std::unique_ptr<net::Transport> started, const std::string& myName, bool checkModsOnly = false);
        bool ModsMatched() const { return m_modsMatched; }  // friend: the host said so (kept after Stop)
        const net::ModsMatch& MatchedHost() const { return m_matchedHost; }  // friend: who, and their save
        void SetCurrentSave(const std::string& save) { m_currentSave = save; }  // host: the save we're playing
        // Mods whose quests the host's game runs, besides the game's (step 8.6). Host: from its settings, before
        // Host (names that aren't plugin file names are dropped); friend: the host's, from its welcome.
        void SetQuestMods(const std::vector<std::string>& mods);
        // Host: whether everyone who asks is let in without asking (tools and tests; the plugin asks, but a
        // developer setting can turn it on for labs). Default on.
        void SetAutoApprove(bool on) { m_autoApprove = on; }
        std::vector<JoinAsk> TakeJoinAsks() { return std::move(m_joinAsks); }
        void AnswerJoin(uint32_t askId, bool yes);  // host: let them in (remembered for this session), or not
        bool StillAsking(uint32_t askId) const;     // host: they haven't left, nor been answered
        bool TakeAskingHost() { bool b = m_askingHost; m_askingHost = false; return b; }  // friend: once
        const std::vector<std::string>& QuestMods() const { return m_questMods; }
        void Stop();
        void Update();

        Role GetRole() const { return m_role; }
        FriendState GetFriendState() const { return m_friendState; }
        uint8_t MyPlayerId() const { return m_myId; }
        bool InGame() const { return m_role == Role::Host || m_friendState == FriendState::Joined; }

        // Where our own character is; call every frame. Sent to the others at a fixed rate.
        void SetMyPose(const Pose& pose) { m_myPose = pose; m_hasPose = true; }
        // What our character is wearing and holding (base form ids). Sent to the others when it changes.
        void SetMyGear(std::vector<uint32_t> items);
        // Friend: we hit something; the host applies it (DESIGN: the host decides damage). Host: does nothing,
        // the host's own hits are already real.
        void ReportHit(uint32_t targetRef, float damage);
        // Friend: a host's NPC turned to goo or ash in our game (our energy weapon's kill); host: those reports.
        void ReportPile(uint32_t hostRef, uint8_t kind) const;
        std::vector<net::NpcPile> TakePiles() { return std::move(m_piles); }
        // A mine placed in the world was disarmed, is gone or blew up here (net::kMine...): tell the others (a friend:
        // the host, who passes it on). TakeMines: the others' (made the same here; whose: the host's 0 on a friend).
        void ReportMine(uint32_t ref, uint8_t state);
        // Host: whether enemies are going for this friend's character (sent to them when it changes). Friend: what
        // the host last said about ours.
        void SetFighting(uint8_t playerId, bool fighting);
        bool HostSaysFighting() const { return m_hostSaysFighting; }
        struct MineFrom { uint8_t playerId; net::MineChange change; };
        std::vector<MineFrom> TakeMines() { return std::move(m_mines); }
        // Host: friends' hits since the last call.
        std::vector<HitReport> TakeHits() { return std::move(m_hits); }
        // We shot (shots fired) or swung (1): our character does it on everyone else's game. At most
        // net::kMaxShots at a time.
        void Attack(uint8_t shots, bool release = false);  // release: a throw left our hand (shots 0)
        // An explosion in our game (step 8.8): a friend sends its own player's, the host any; everyone else shows it.
        void ShareExplosion(const net::ExplosionAt& e);
        // The desync detector (DESIGN rule 8). Friend: something our game disagrees with the host's on right now
        // (the game side's checks of NPCs; items and doors are checked here). Lasting ones are logged and counted to
        // the host. Both: a line for the corner of the screen when one starts, to show if notices are on.
        void SawDesync(const Disagreement& d);
        // Companions (DESIGN: companions are the party's). Host: ours and who each follows (sent when it changes,
        // and every few seconds); friends' orders (checked: one of ours) to carry out.
        void SetCompanions(const std::vector<net::CompanionState>& companions);
        struct CompanionOrderFrom { uint8_t playerId; net::CompanionOrder order; };
        std::vector<CompanionOrderFrom> TakeCompanionOrders() { return std::move(m_companionOrders); }
        // Friend: whether one of the host's refs is a companion of the group, and who it follows; telling it to.
        bool CompanionLeader(uint32_t hostRef, uint8_t& leader) const;
        void OrderCompanion(uint32_t hostRef, net::CompanionOrderKind order);
        // Scripted sequences: host, what a quest script did to our player (every friend's game does it to theirs);
        // friend, those to do to ours.
        void SharePlayerCommand(const net::PlayerCommand& c);
        // A script killed our player (Dead Money's collar): no downed state, the game is over for everyone.
        void KilledByScript();
        // Synced radios. Host: a station started a line (sent now, and every few seconds again for whoever loaded
        // in since). Friend: the host's stations' lines, newest per station.
        void ShareRadioLine(net::RadioLine line, uint32_t startedAt);  // (its seq and elapsed are filled in)
        // Text chat: our player's line goes to everyone; the others' lines, as heard.
        struct ChatHeard { uint8_t player = 0; std::string name, text; VoiceStyle style; };
        void SendChat(const std::string& text, VoiceStyle style = {});
        std::vector<ChatHeard> TakeChat() { return std::move(m_chat); }
        // Voice chat: our player's voice goes to everyone; the others', as heard.
        struct VoiceHeard { uint8_t player = 0; net::Bytes data; };
        void SendVoice(const net::Bytes& data);
        std::vector<VoiceHeard> TakeVoice() { return std::move(m_voice); }
        // How players' characters look: ours goes to everyone when it changes (and to whoever joins); the others'
        // by player, with a count that goes up each time any of them changes.
        void SetMyLooks(const net::PlayerLooks& looks);
        const std::map<uint8_t, net::PlayerLooks>& Looks() const { return m_looks; }
        uint32_t LooksChanges() const { return m_looksChanges; }
        std::map<uint32_t, net::RadioLine> TakeRadioLines() { return std::move(m_radioLines); }
        std::vector<net::PlayerCommand> TakePlayerCommands() { return std::move(m_playerCommands); }
        std::vector<std::string> TakeDesyncNotices() { return std::move(m_desyncNotices); }
        std::vector<net::ExplosionAt> TakeExplosions() { return std::move(m_explosions); }  // others', to show
        // Everyone else's attacks since the last call.
        std::vector<AttackReport> TakeAttacks() { return std::move(m_attacks); }
        // Our health; call regularly. Sent to the others when it changes.
        void SetMyHealth(float health, float maxHealth);
        // Host: something in our world hurt that friend's character; they lose the health on their own game.
        void DamagePlayer(uint8_t playerId, float amount);
        // The save's mode (REBALANCE.md): the host's is the session's (sent in the welcome); friends have the host's.
        void SetSessionMode(const SaveMode& mode) { if (m_role != Role::Friend) m_mode = mode; }
        const SaveMode& SessionMode() const { return m_mode; }
        // Friendly fire (when the session has it on): our player hit another player's character.
        void ReportPlayerHit(uint8_t target, float damage);
        // Friend: health the host says we lost since the last call.
        float TakeDamage() { float d = m_damage; m_damage = 0; return d; }
        // We're reviving this downed player (held the interact key next to them). The host checks it.
        void Revive(uint8_t playerId);
        // A friend revived us since the last call.
        bool TakeRevived() { bool r = m_revived; m_revived = false; return r; }
        // Game over since the last call (the host decides: someone bled out, or everyone is down).
        bool TakeGameOver(net::GameOver& out);
        // Where the game should take our character now, and who led the way: the group went through a door or
        // fast travelled (or we just joined). Our own trips are noticed from our position and sent.
        bool TakeTravel(Pose& out, uint8_t& leader);
        // A save is loading: our next position starts fresh, it isn't a trip. Once it's in, a friend is brought
        // back to the group, and a host brings the group to them.
        void ForgetMyPlace();
        // Host: our NPCs near the group, in place `space`, nearest first (at most kMaxNpcs used); call regularly.
        // The ones that changed are sent at the position rate, the rest once a second.
        void SetNpcs(uint32_t space, std::vector<net::NpcState> npcs);
        // Friend: the host's NPCs we've heard of lately, by ref id.
        const std::map<uint32_t, RemoteNpc>& Npcs() const { return m_npcs; }
        // Host: one of the NPCs we share shot (shots fired), swung (1) or started reloading; friends' copies do it
        // too.
        void NpcAttacked(uint32_t ref, uint8_t shots, bool reloaded = false, bool released = false);
        // Host: what one of the NPCs we share, made while playing, wears (its copies get dressed the same). Call
        // now and then; sent when it changes, and again every kNpcGearResendMs.
        void SetNpcGear(uint32_t ref, std::vector<uint32_t> items);
        // Friend: the host's NPCs' attacks since the last call (only ones we've heard of).
        std::vector<net::NpcAttack> TakeNpcAttacks() { return std::move(m_npcAttacks); }
        // Host: our doors near the group, as they are now; call a few times a second. Changes are sent at once,
        // everything every kDoorResendMs.
        void SetDoors(std::vector<net::DoorState> doors);
        // Host: doors friends opened, closed, unlocked or locked since the last call (for our game to do too).
        std::vector<net::DoorState> TakeDoorUses() { return std::move(m_doorUses); }
        // Friend: our game's doors near us, as they are now; call a few times a second. Our player's own changes
        // go to the host; returns the doors to set to the host's state.
        std::vector<net::DoorState> ObserveDoors(uint32_t now, const std::vector<LocalDoor>& local);
        // Containers and bodies (step 6.2). Host: the ones friends have open (our refs), to read now and then.
        std::vector<uint32_t> ViewedContainers() const;
        // Host: what one of them holds now (sent to friends viewing it when it changes).
        void SetContainerContents(uint32_t ref, const std::vector<net::ItemCount>& items);
        // Host: friends' takes and puts since the last call. Apply each, then call ContainerChangeDone.
        struct ItemChangeFrom { uint8_t playerId; net::ContainerChange change; };
        std::vector<ItemChangeFrom> TakeContainerChanges() { return std::move(m_itemChanges); }
        // Host: a friend's change is done; `refund` (count 0: none) is what they must give back or get back,
        // because it couldn't happen in our world (someone took it first, say).
        void ContainerChangeDone(uint8_t playerId, uint32_t seq, const net::ItemCount& refund);
        // Friend: our player opened this container or body (the host's id; 0: closed it).
        void ViewContainer(uint32_t hostRef, bool open = true);
        // Friend: what it holds in our game now; call a few times a second while it's open. Our player's takes
        // and puts go to the host. Returns what to change in ours to match the host's; then call ContainerSettled.
        std::vector<net::ItemCount> LookAtContainer(const std::vector<net::ItemCount>& local);
        void ContainerSettled(const std::vector<net::ItemCount>& local) { m_friendContainer.Settle(local); }
        // Friend: what the host says must come out of (negative) or back into our inventory.
        std::vector<net::ItemCount> TakeItemRefunds() { return std::move(m_refunds); }
        // Items lying in the world (step 6.3). Host: ours near the group as they are now (held: the one our player
        // carries); call a couple of times a second. Changes go at once, everything every kItemResendMs.
        void SetWorldItems(const std::vector<net::ItemState>& items);
        // Host: friends' pick-ups and drops since the last call (do each in our world).
        struct ItemTakeFrom { uint8_t playerId; net::ItemTake take; };
        struct ItemDropFrom { uint8_t playerId; net::ItemDrop drop; };
        std::vector<ItemTakeFrom> TakeItemTakes() { return std::move(m_itemTakes); }
        std::vector<ItemDropFrom> TakeItemDrops() { return std::move(m_itemDrops); }
        // Host: friends' games moved these of our items (step 8.9; checked: ones we share, not ours carried, not far
        // off): ours go there too.
        struct ItemMoveFrom { uint8_t playerId; net::ItemMove move; };
        std::vector<ItemMoveFrom> TakeItemMoves() { return std::move(m_itemMoves); }
        // Host: something a friend has must come out of (negative) or back into their inventory.
        void RefundItems(uint8_t playerId, const net::ItemCount& refund);
        // Friend: our game's items near us as they are now; call a couple of times a second. Our player's pick-ups
        // and drops, and our game's moves of the host's items, go to the host; returns what to do in our game (call
        // ItemMade for each copy placed).
        ItemActions LookAtItems(const std::vector<LocalItem>& local, bool sameArea);
        void ItemMade(uint32_t hostRef, uint32_t ourRef) { m_friendItems.Made(hostRef, ourRef); }
        // Quests (step 7.1). Host: these quests changed (or are being sent again); they go to every friend.
        void SendQuestStates(const std::vector<net::QuestState>& quests);
        // Host: how many times a friend has joined so far (a new one needs every quest).
        uint32_t Joins() const { return m_joins; }
        // Host: things our scripts switched on or off; they go to every friend.
        void SendRefEnables(const std::vector<net::RefEnable>& changes);
        // Friend: what the host's scripts switched on or off, heard since the last call.
        std::vector<net::RefEnable> TakeRefEnables() { return std::move(m_refEnables); }
        // XP and karma (step 7.2): our player gained these; everyone else gets them too.
        void ShareProgress(int32_t xp, int32_t karma);
        // Late joining (Alex, 2026-10-19): the host's XP goes with its welcome; a friend whose character is behind
        // catches up to it (TakeHostXp: once, after joining).
        void SetMyXp(int32_t xp) { m_myXp = xp < 0 ? 0 : xp > net::kMaxXp ? net::kMaxXp : xp; }
        bool TakeHostXp(int32_t& xp)
        {
            if (!m_hostXpWaiting)
                return false;
            m_hostXpWaiting = false;
            xp = m_hostXp;
            return true;
        }
        // What other players gained since the last call (for our player to gain too).
        std::vector<net::Progress> TakeProgress() { return std::move(m_progress); }
        // Host: faction reputations as we have them (changed ones, or all); they go to every friend.
        void SendReputations(const std::vector<net::Reputation>& reps);
        // Friend: reputation changes our own game made (to the host, whose reputations are everyone's).
        void ShareReputationGains(const std::vector<net::ReputationGain>& gains);
        // Host: friends' reputation changes (player id, change) heard since the last call.
        std::vector<std::pair<uint8_t, net::ReputationGain>> TakeReputationGains() { return std::move(m_repGains); }
        // Friend: the host's reputations heard since the last call.
        std::vector<net::Reputation> TakeReputations() { return std::move(m_reputations); }
        // Host: another player's gain that our game also made (it ran their conversation's line): everyone but them
        // gets it from us.
        void ShareProgressFor(uint8_t playerId, int32_t xp, int32_t karma);
        // Conversations (step 7.4). Our player's conversation started (npc: the host's id for who with, maybe 0) or
        // ended; call when that changes.
        void SetMyConversation(bool talking, uint32_t npc);
        // Who is talking now, as the host decides (a friend hears it from the host).
        const TalkLock& Talk() const { return m_talk; }
        bool MayTalk() const { return m_talk.MayTalk(m_myId); }
        // Friend: a line of our conversation ran its script in our game; the host's game runs it too.
        void SendDialogueLine(const net::DialogueLine& line);
        // What the one we're talking to is saying now (or stop: they stopped): everyone else hears it.
        void ShareSubtitle(net::Subtitle s);
        // Other players' conversations' subtitles heard since the last call.
        std::vector<net::Subtitle> TakeSubtitles() { return std::move(m_subtitles); }
        // Host: friends' lines since the last call (only from whoever may talk).
        struct DialogueLineFrom { uint8_t playerId; net::DialogueLine line; };
        std::vector<DialogueLineFrom> TakeDialogueLines() { return std::move(m_dialogueLines); }
        // Sleep and wait (step 8.2): everyone must agree. We pressed Wait or Sleep (hours 0: we closed the menu).
        void AskToWait(uint8_t hours, bool sleep);
        void AnswerWait(bool yes);  // our answer to the vote we were asked (WaitEvent::Kind::Asked)
        struct WaitEvent
        {
            enum class Kind { Asked, Start, Refused } kind;  // asked: answer it; start: wait now; refused: it's off
            uint8_t requester = 0;
            uint8_t hours = 0;
            bool sleep = false;
            uint8_t refusedBy = net::kNobody;  // refused: who said no (kNobody: no answer in time, or called off)
        };
        std::vector<WaitEvent> TakeWaitEvents() { return std::move(m_waitEvents); }
        // Host: our clock now (call every so often); friends hear it every kGameTimeEveryMs.
        void SetMyGameTime(const net::GameTime& t) { m_myGameTime = t; m_hasGameTime = true; }
        // Friend: the host's newest clock since the last call (false: none).
        bool TakeGameTime(net::GameTime& out)
        {
            if (!m_hostTimeNew)
                return false;
            out = m_hostTime;
            m_hostTimeNew = false;
            return true;
        }
        // Loading (step 8.4): whether the world waits now for someone still loading (who: waitingFor).
        // (Not while we're on our way ourselves: the world waits for us, and frozen, we couldn't even close the Pip-Boy
        // to follow: playthrough test, 2026-10-16.)
        bool Frozen() const { return m_frozen && !m_expecting && !m_hasTravel; }
        uint8_t FrozenFor() const { return m_frozenFor; }
        // Friend: we've finished loading the save the host loaded (or found we had none to load).
        void LoadedHostSave() { SendArrived(); }
        // Linked saves (step 8.3). Host: our game saved, or is loading, this save file: friends save (load) theirs.
        void ShareSave(const std::string& file, bool load);
        // Friend: the host's saves and loads since the last call (names checked with SaveNameOf).
        std::vector<net::SaveOrder> TakeSaveOrders() { return std::move(m_saveOrders); }
        // Friend: we saved (everyone saves), or loaded our match of the host's save `hostSave` (the host is asked).
        void AskHostToSave(const std::string& ourSave);
        void AskHostToLoad(const std::string& hostSave);
        bool TakeLoadRefused() { bool r = m_loadRefused; m_loadRefused = false; return r; }  // friend: the host said no
        // Host: friends' saves (we save for everyone) and asks to load one of our saves, since the last call.
        struct SaveAsk { uint8_t playerId; bool load; std::string name; };
        std::vector<SaveAsk> TakeSaveAsks() { return std::move(m_saveAsks); }
        void RefuseLoad(uint8_t playerId);  // host: no to their ask
        // Friend: the host's quests heard since the last call.
        std::vector<net::QuestState> TakeQuestStates() { return std::move(m_questStates); }
        // Friend: why the host refused us, once ("" if it didn't, or if the mod report says why).
        std::string TakeRejectReason() { return std::move(m_rejectReason); }
        // Friend: what differs between our mods and the host's, for the screen, once ("" if nothing).
        std::string TakeModReport() { return std::move(m_modReport); }
        // Friend: the question whether to fetch the host's mods and fix ours, once ("" if none); answer it.
        std::string TakeModQuestion() { return std::move(m_modQuestion); }
        void AnswerModFix(bool yes);
        bool DownloadingMods() const { return m_downloading; }
        void CancelModFix() { if (m_downloading) m_download.Fail("you cancelled"); }  // (then as any failure)
        const ModDownload& Download() const { return m_download; }
        // Friend: how fixing our mods went (restart, or what failed), once ("" if not finished).
        std::string TakeModFixResult() { return std::move(m_modFixResult); }
        std::string NameOf(uint8_t playerId) const;
        void SetBleedOutMs(uint32_t ms) { m_bleedOutMs = ms; }  // host; for tests
        void SetTravelCooldownMs(uint32_t ms) { m_travelCooldownMs = ms; }  // host; for tests
        // Everyone else in the game, by player id.
        const std::map<uint8_t, RemotePlayer>& Remotes() const { return m_remotes; }
        const std::map<net::PeerId, Player>& Players() const { return m_players; }  // host only
        net::Session& GetSession() { return m_enet; }  // the IP connection (fake lag for tests and tools)

    private:
        void HostReceived(net::PeerId peer, const net::Bytes& data);
        void FriendReceived(const net::Bytes& data);
        void Kick(net::PeerId peer, const std::string& reason);
        void SendModList(net::PeerId peer);  // host
        void Admit(net::PeerId peer, Player& player);         // host: approved: their mods first, or welcome
        void WelcomePlayer(net::PeerId peer, Player& player);  // host
        void CheckHostMods();                // friend: the host's list is complete
        void OfferModFix();                  // friend: the host says our mods differ
        void UpdateDownload();               // friend
        void RemovePlayer(net::PeerId peer);
        void SendToJoined(const net::Bytes& data, bool reliable, net::PeerId except = 0);
        void SendMyPose(uint32_t now);
        void SendMyGear();
        void SendMyHealth(uint32_t now);
        void SendNpcs(uint32_t now);    // host
        void SendDoors(uint32_t now);   // host
        void SendItems(uint32_t now);   // host
        void ForgetOldNpcs(uint32_t now);  // friend
        void HeardNpcs(const net::NpcStates& m);  // friend
        void SetRemoteGear(uint8_t id, std::vector<uint32_t> items);
        void QueueAttack(uint8_t playerId, uint8_t shots, bool release);
        void CheckDowned(uint32_t now);  // host
        void EndGame(net::GameOverReason reason, uint8_t playerId);  // host
        void HostRevive(uint8_t reviver, uint8_t target);
        bool PoseOf(uint8_t playerId, Pose& out) const;
        void CheckMyTravel(uint32_t now);
        void GroupTravels(uint8_t leader, const Pose& to, uint32_t now);  // host
        void SendTravel(uint8_t toPlayer, uint8_t leader, const Pose& to);  // host
        void PullMe(uint8_t leader, const Pose& to);
        uint8_t FreePlayerId() const;

        net::Session m_enet;                // the IP connection (labs, the fake friend, tests)
        std::unique_ptr<net::Transport> m_other;  // another (Steam, step 8.5), when that's what's used
        net::Transport* m_net = &m_enet;    // what we're connected through
        Role m_role = Role::None;
        std::string m_myName;
        uint8_t m_myId = 0;
        FriendState m_friendState = FriendState::Disconnected;
        std::map<net::PeerId, Player> m_players;
        uint32_t m_lastPingLog = 0;
        std::map<uint8_t, RemotePlayer> m_remotes;
        Pose m_myPose, m_sentPose;
        bool m_hasPose = false;
        uint32_t m_lastPoseSent = 0;
        std::vector<uint32_t> m_myGear;
        bool m_gearToSend = false;
        std::vector<HitReport> m_hits;
        std::vector<AttackReport> m_attacks;
        float m_myHealth = 0, m_myMaxHealth = 0;
        bool m_healthToSend = false;
        uint32_t m_lastHealthSent = 0;
        float m_damage = 0;
        bool m_hostSaysFighting = false;                  // friend: enemies are going for our character
        SaveMode m_mode;  // the session's (the host's save's)
        bool m_revived = false;
        bool m_gameOver = false;
        net::GameOver m_gameOverInfo;
        std::map<uint8_t, uint32_t> m_downedSince;  // host: who's down, since when
        bool m_gameOverSent = false;                // host: until everyone is up again
        uint32_t m_bleedOutMs = kBleedOutMs;
        Pose m_place;  // our position last frame, to notice trips
        bool m_hasPlace = false;
        bool m_regroup = false;  // loaded a save in co-op: regroup at our next position
        Pose m_expect;  // where we were told to go: arriving there isn't our own trip
        bool m_expecting = false;
        Pose m_travel;  // for the game: go here
        uint8_t m_travelLeader = 0;
        bool m_hasTravel = false;
        Pose m_groupPlace;  // host: where the group last went, when, and who led
        uint8_t m_groupLeader = 0;
        uint32_t m_groupMovedAt = 0;
        bool m_hasGroupPlace = false;
        uint32_t m_travelCooldownMs = kTravelCooldownMs;
        struct SentNpc { net::NpcState state; uint32_t at = 0; };
        uint32_t m_npcSpace = 0;  // host
        std::vector<net::NpcState> m_myNpcs;
        std::map<uint32_t, SentNpc> m_sentNpcs;
        uint32_t m_lastNpcsSent = 0;
        uint32_t m_npcUpdatesSent = 0, m_npcBytesSent = 0;  // since the last log line
        std::map<uint32_t, net::NpcAttack> m_npcShots;  // host: attacks not sent yet, by ref
        struct NpcGearSent { std::vector<uint32_t> items; uint32_t sentAt = 0; bool changed = true; };
        std::map<uint32_t, NpcGearSent> m_npcGear;     // host: by ref
        std::vector<net::NpcAttack> m_npcAttacks;  // friend: not taken yet
        std::map<uint32_t, RemoteNpc> m_npcs;  // friend
        bool m_warnedNpcs = false;
        std::vector<net::DoorState> m_myDoors;            // host
        std::map<uint32_t, net::DoorState> m_sentDoors;   // host: as last sent, by ref
        uint32_t m_lastDoorsSent = 0;                     // host: everything, last time
        std::vector<net::DoorState> m_doorUses;           // host: from friends, not taken yet
        FriendDoors m_friendDoors;                        // friend
        struct HostView
        {
            uint32_t ref = 0;     // the container they have open (0: none)
            uint32_t seen = 0;    // their last change we've done
            bool sent = false;    // contents sent for ref since it was opened
            uint32_t sentSeen = 0;
            std::vector<net::ItemCount> sentItems;
        };
        std::map<uint8_t, HostView> m_views;              // host: by player id
        std::vector<ItemChangeFrom> m_itemChanges;        // host: not taken yet
        FriendContainer m_friendContainer;                // friend
        std::vector<net::ItemCount> m_refunds;            // friend: not taken yet
        std::vector<net::ItemState> m_myItems;            // host
        std::map<uint32_t, net::ItemState> m_sentItems;   // host: as last sent, by ref
        uint32_t m_lastItemsSent = 0;                     // host: everything, last time
        bool m_itemsNew = false;                          // host: a new list since SendItems last compared
        std::vector<ItemTakeFrom> m_itemTakes;            // host: not taken yet
        std::vector<ItemDropFrom> m_itemDrops;            // host: not taken yet
        std::vector<ItemMoveFrom> m_itemMoves;            // host: not taken yet
        std::map<uint32_t, uint32_t> m_lastMoveSent;      // friend: item -> when we last sent a move of it (logs)
        DesyncTracker m_desyncs;                          // friend
        std::vector<net::CompanionState> m_myCompanions;  // host
        bool m_companionsChanged = false;                 // host: not sent yet
        uint32_t m_lastCompanionsSent = 0;                // host
        std::vector<CompanionOrderFrom> m_companionOrders;  // host: not taken yet
        std::map<uint32_t, uint8_t> m_companions;         // friend: the host's companion -> who it follows
        std::vector<net::PlayerCommand> m_playerCommands; // friend: not taken yet
        std::map<uint32_t, net::RadioLine> m_radioLines;  // host: each station's line now; friend: not taken yet
        uint32_t m_nextRadioResend = 0;                   // host
        std::map<uint32_t, uint32_t> m_radioStartedAt;    // host: station -> when its line started (GetTickCount)
        std::vector<ChatHeard> m_chat;                    // others' chat lines not taken yet
        std::vector<VoiceHeard> m_voice;                  // others' voice not taken yet
        std::vector<net::NpcPile> m_piles;                // host: friends' goo and ash kills not taken yet
        std::vector<MineFrom> m_mines;                    // others' disarmed and gone mines not taken yet
        net::PlayerLooks m_myLooks;
        bool m_hasMyLooks = false, m_myLooksSent = false;
        std::map<uint8_t, net::PlayerLooks> m_looks;      // others' looks, by player
        uint32_t m_looksChanges = 0;
        int32_t m_myXp = 0;                               // host: our player's XP (sent with each welcome)
        int32_t m_hostXp = 0;                             // friend: the host's, from its welcome
        bool m_hostXpWaiting = false;
        void SendCompanions(uint32_t now);
        DesyncCounts m_desyncsSent;                       // friend: as last told to the host
        uint32_t m_nextDesyncSweep = 0;
        std::vector<std::string> m_desyncNotices;         // not taken yet
        void SweepDesyncs(uint32_t now);
        FriendItems m_friendItems;                        // friend
        uint32_t m_joins = 0;                             // host
        ModList m_mods;
        net::Hash m_modsPrint{};
        ModList m_hostMods;                               // friend: as heard so far
        bool m_hostModsDone = false;                      // friend: the host's list is complete
        bool m_modsDiffer = false;                        // friend: so we can't join
        bool m_checkOnly = false;                         // friend: from the main menu, not joining
        bool m_modsMatched = false;                       // friend: the host said so
        net::ModsMatch m_matchedHost;                     // friend
        std::string m_currentSave;                        // host
        std::vector<std::string> m_questMods;
        bool m_autoApprove = true;                        // host
        std::vector<JoinAsk> m_joinAsks;                  // host: not taken yet
        uint32_t m_nextAskId = 1;                         // host
        std::set<std::string> m_approved;                 // host: "<name>@<from>" let in this session
        bool m_askingHost = false;                        // friend: not taken yet
        std::vector<net::ExplosionAt> m_explosions;       // others' explosions, not taken yet
        uint32_t m_hostExplosionsSince = 0, m_hostExplosions = 0;  // friend: the host's in this second
        static bool WithinLimit(uint32_t& since, uint32_t& count, uint32_t most);  // at most this many a second
        std::string m_modReport;                          // friend: not taken yet
        ModFolders m_modFolders;
        ModServer m_modServer;                            // host
        std::vector<ModDownload::Wanted> m_modsWanted;    // friend: what fixing would fetch
        std::vector<std::string> m_modsExtra;             // friend: what fixing would put away
        std::string m_modQuestion;                        // friend: not taken yet
        ModDownload m_download;                           // friend
        bool m_downloading = false;                       // friend: said yes, fetching or fixing
        std::string m_modFixResult;                       // friend: not taken yet
        std::string m_rejectReason;                       // friend
        std::vector<net::QuestState> m_questStates;       // friend: not taken yet
        std::vector<net::RefEnable> m_refEnables;         // friend: not taken yet
        std::vector<net::Progress> m_progress;            // others' gains, not taken yet
        std::vector<net::Reputation> m_reputations;       // friend: not taken yet
        std::vector<std::pair<uint8_t, net::ReputationGain>> m_repGains;  // host: friends', not taken yet
        TalkLock m_talk;                                  // host: decides; friend: as the host last said
        void SendTalk(net::PeerId to);                    // host: who is talking, to one friend (0: all)
        std::vector<DialogueLineFrom> m_dialogueLines;    // host: not taken yet
        std::vector<net::Subtitle> m_subtitles;           // not taken yet
        LoadGate m_loads;                                 // host: who is still on their way
        bool m_frozen = false;                            // as the host says (host: decides)
        uint8_t m_frozenFor = 0;
        void ExpectLoads(uint8_t except, uint32_t now);   // host: everyone else (but `except`) is on their way
        void UpdateFreeze(uint32_t now);                  // host
        void SendArrived();                               // friend
        std::vector<net::SaveOrder> m_saveOrders;         // friend: not taken yet
        bool m_loadRefused = false;                       // friend
        std::vector<SaveAsk> m_saveAsks;                  // host: not taken yet
        uint32_t m_lastSaveAsk = 0;                       // host: when a friend's last ask came (one a second)
        WaitVote m_wait;                                  // host: the open sleep/wait vote
        std::vector<WaitEvent> m_waitEvents;              // not taken yet
        void StartWaitVote(uint8_t requester, uint8_t hours, bool sleep, net::PeerId from);  // host
        void CheckWaitVote(uint32_t now);                 // host: tell everyone once it's decided
        void SendWaitResult(const net::WaitResult& m);    // host: to everyone, and to us
        net::GameTime m_myGameTime;                       // host
        bool m_hasGameTime = false;
        uint32_t m_lastGameTimeSent = 0;
        net::GameTime m_hostTime;                         // friend
        bool m_hostTimeNew = false;
    };
}
