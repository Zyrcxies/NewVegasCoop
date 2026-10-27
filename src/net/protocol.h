#pragma once

// The messages players send each other, and safe reading/writing of them.
// Game-independent: tested in tests/.
// Every incoming message is untrusted: Reader never reads past the end, and
// decoding fails on anything malformed (wrong sizes, too-long text, leftover bytes).

#include "coop/chat.h"
#include "coop/game_mode.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace net
{
    // Bump whenever any message changes; players must match exactly.
    constexpr uint32_t kProtocolVersion = 62;
    constexpr size_t kMaxNameLength = 32;
    constexpr size_t kMaxReasonLength = 200;
    constexpr size_t kMaxMessageSize = 64 * 1024;

    enum class MsgType : uint8_t
    {
        Hello = 1,    // friend -> host: who I am
        Welcome = 2,  // host -> friend: you're in
        Reject = 3,   // host -> friend: you're not (then disconnect)
        PlayerJoined = 4,  // host -> friends: someone is in the game (sent for everyone already there too)
        PlayerLeft = 5,    // host -> friends: someone left
        PlayerState = 6,   // where a player's character is. Friend -> host (id ignored), host -> friends
        PlayerGear = 7,    // what a player's character is wearing and holding. Same routing as PlayerState
        Hit = 8,           // friend -> host: I hit this (id ignored). The host applies the damage
        PlayerHealth = 9,  // a player's health. Same routing as PlayerState
        Damage = 10,       // host -> friend: something in the host's world hurt your character
        Revive = 11,       // friend -> host: I'm reviving this player. host -> friend: you've been revived
        GameOver = 12,     // host -> friend: someone bled out or everyone is down; the group reloads
        Attack = 13,       // a player shot or swung (shown only: hits are sent separately). Same routing as PlayerState
        Travel = 14,       // friend -> host: I went somewhere (door, fast travel; id ignored). host -> friend: come here
        Regroup = 15,      // friend -> host: I loaded a save; where's the group? (answered with Travel)
        NpcStates = 16,    // host -> friends: where the host's nearby NPCs are (friends' copies copy them)
        NpcAttacks = 17,   // host -> friends: the host's NPCs shot or swung (shown only: the host sends damage)
        NpcGear = 18,      // host -> friends: what one of the host's NPCs made while playing is wearing
        DoorStates = 19,   // host -> friends: open, closed, locked of the doors near the group
        DoorUse = 20,      // friend -> host: I opened, closed, unlocked or locked this door
        ContainerView = 21,      // friend -> host: I opened this container or body (0: closed it)
        ContainerChange = 22,    // friend -> host: I took (or put) items from (into) the container I have open
        ContainerContents = 23,  // host -> friend: what the container you have open really holds
        ItemStates = 25,         // host -> friends: items lying near the group (there or taken, where, made ones)
        ItemTake = 26,           // friend -> host: I picked this item up
        ItemDrop = 27,           // friend -> host: I dropped this (the host places it; everyone gets a copy)
        QuestStates = 28,        // host -> friends: quests as they are in the host's game (stages, objectives, flags)
        RefEnables = 29,         // host -> friends: things the host's scripts switched on or off (Enable / Disable)
        Progress = 30,           // a player gained XP or karma (friend -> host, id ignored; host -> friends)
        Reputations = 31,        // host -> friends: faction fame and infamy as the host has them
        TalkState = 32,          // friend -> host: my conversation started or ended (id ignored). host -> friends:
                                 // who is talking now
        DialogueLine = 33,       // friend -> host: a line of my conversation ran its script here; run it there too
        Subtitle = 34,           // what the one I'm talking to is saying, or that they stopped (friend -> host, id
                                 // ignored; host -> friends)
        ItemRefund = 24,         // host -> friend: your change didn't happen: these come out of (or back into) your inventory
        WaitAsk = 35,            // friend -> host: I pressed Wait or Sleep (hours 0: I closed the menu, never mind)
        WaitVote = 36,           // host -> the others: someone asks to wait; answer with WaitAnswer
        WaitAnswer = 37,         // friend -> host: yes or no to the open vote
        WaitResult = 38,         // host -> everyone: the vote passed (everyone waits) or didn't
        GameTime = 39,           // host -> friends: the host's clock and calendar (every few seconds)
        SaveOrder = 40,          // host -> friends: I saved (or am loading) this save: save (load) your matching one.
                                 // friend -> host: I saved (save everyone), or loaded our matching save of yours (ask)
        LoadRefused = 41,        // host -> friend: no to loading your save for everyone
        Arrived = 42,            // friend -> host: I've finished loading after the group moved (or a save load)
        Freeze = 43,             // host -> friends: the world waits (for someone still loading), or goes on
        ModFiles = 44,           // host -> joining friend: a page of the host's mod list (step 8.6)
        ModsDiffer = 45,         // host -> joining friend: your mods aren't mine; you may fetch mine (ModFileAsk)
        ModFileAsk = 46,         // friend (not joined) -> host: send me this piece of one of your mod files
        ModPiece = 47,           // host -> friend: the piece asked for
        AskingHost = 49,         // host -> friend: the host is being asked to let you in (step 8.5)
        ExplosionAt = 50,        // an explosion went off here (friend -> host: our own; host -> friends: any)
        ItemMoves = 51,          // friend -> host: items our game moved (carried, knocked over, blown away)
        DesyncReport = 52,       // friend -> host: how many things our game disagrees with the host's on
        CompanionStates = 53,    // host -> friends: our companions and who each follows
        CompanionOrder = 54,     // friend -> host: a companion is to follow me, or wait
        PlayerCommand = 55,      // host -> friends: a quest script did this to the host's player; do it to yours
        KilledByScript = 56,     // friend -> host: a script killed my player (no payload): game over
        RadioLine = 57,          // host -> friends: a radio station started this line (a song, the DJ)
        ChatLine = 58,           // both ways: a line of text chat (friend -> host: theirs; host -> friends: whose)
        PlayerHit = 60,          // friend -> host: I hit this player (friendly fire, when the host's save has it on)
        VoiceData = 59,          // both ways: voice chat (Steam's squeezed voice; friend -> host: theirs; host -> friends: whose)
        PlayerLooks = 61,        // both ways: how a player's character looks (friend -> host: theirs; host -> friends: whose)
        NpcPile = 62,            // friend -> host: the host's NPC turned to goo or ash in my game (my energy weapon)
        MineChange = 63,         // both ways: a mine placed in the world was disarmed, or is gone, in my game
                                 // (friend -> host: theirs; host -> friends: anyone's)
        ModsMatch = 48,          // host -> friend who only checks (at the main menu): your mods are mine; bye
    };

    using Bytes = std::vector<uint8_t>;

    class Writer
    {
    public:
        explicit Writer(MsgType type) { U8(static_cast<uint8_t>(type)); }
        void U8(uint8_t v) { m_data.push_back(v); }
        void U16(uint16_t v);
        void U32(uint32_t v);
        void F32(float v);
        void String(std::string_view s, size_t maxLength);  // cut to maxLength
        void Block(const void* data, size_t size) { auto p = static_cast<const uint8_t*>(data); m_data.insert(m_data.end(), p, p + size); }
        Bytes Take() { return std::move(m_data); }

    private:
        Bytes m_data;
    };

    class Reader
    {
    public:
        Reader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}
        explicit Reader(const Bytes& b) : Reader(b.data(), b.size()) {}

        bool Type(MsgType& out);
        bool U8(uint8_t& out);
        bool U16(uint16_t& out);
        bool U32(uint32_t& out);
        bool F32(float& out);  // fails on NaN, infinity and huge values
        bool String(std::string& out, size_t maxLength);  // fails if too long or has control characters
        bool Block(void* out, size_t size) { return Raw(out, size); }
        bool AtEnd() const { return m_pos == m_size; }

    private:
        bool Raw(void* out, size_t n);
        const uint8_t* m_data;
        size_t m_size;
        size_t m_pos = 0;
    };

    using Hash = std::array<uint8_t, 32>;  // SHA-256
    struct Hello
    {
        uint32_t version = 0;
        std::string name;
        bool hasMods = false;  // false: not checked (a tool like the fake friend)
        Hash mods{};           // fingerprint of the friend's mod list (DESIGN rule 2: it must be the host's)
        bool checkOnly = false;  // only comparing mods (from the main menu), not joining
    };

    // Mod-list sync (step 8.6). A mod file the game loads: a plugin (.esm/.esp, in load order), an archive (.bsa)
    // that comes with one, or an xNVSE plugin (.dll: only listed, never sent).
    enum class ModKind : uint8_t { Plugin = 0, Archive = 1, Dll = 2 };
    struct ModFile
    {
        std::string name;     // a plain file name, no folders (in Data, or Data\NVSE\Plugins for a Dll)
        ModKind kind = ModKind::Plugin;
        uint64_t size = 0;
        bool hashed = false;  // the base game's own files are compared by size only
        Hash hash{};
    };
    constexpr size_t kMaxModFiles = 1024;
    constexpr size_t kModFilesPerMessage = 200;
    constexpr size_t kMaxModFileNameLength = 200;
    // A name that can only ever mean a file of that kind in its own folder (no folders, "..", devices or odd
    // characters), so a bad list can't point anywhere else.
    bool IsPlainModFileName(const std::string& name, ModKind kind);
    struct ModFiles
    {
        bool last = false;  // the list is complete
        std::vector<ModFile> files;
    };
    struct Welcome
    {
        uint8_t playerId = 0;
        std::string hostName;
        std::vector<std::string> questMods;  // mods whose quests the host runs, besides the game's (step 8.6)
        coop::SaveMode mode;                 // the host's save's (REBALANCE.md: it decides for everyone)
        int32_t hostXp = 0;                  // the host's player's XP: a friend behind catches up (0 to kMaxXp)
    };
    constexpr int32_t kMaxXp = 10000000;
    constexpr size_t kMaxQuestMods = 64;
    struct Reject { std::string reason; };
    struct PlayerJoined { uint8_t playerId = 0; std::string name; };
    struct PlayerLeft { uint8_t playerId = 0; };
    struct PlayerState
    {
        uint8_t playerId = 0;
        uint32_t timeMs = 0;  // sender's clock, for ordering and smoothing
        uint32_t space = 0;   // form id of the worldspace (outside) or interior cell the player is in
        float x = 0, y = 0, z = 0;
        float heading = 0;    // radians, 0 = north, clockwise
        uint8_t flags = 0;    // coop::Pose flags (weapon out, sneaking); unknown bits are rejected
        uint16_t moves = 0;   // the game's movement flags (coop::moves), all 16 (each puppet copies what it can)
        float pitch = 0;      // radians, looking down (+) or up (-), at most kMaxPitch either way
    };
    constexpr uint8_t kKnownPlayerStateFlags = 0x03;
    constexpr float kMaxPitch = 1.6f;  // a little over straight up or down (the game stops at about 1.5)
    constexpr size_t kMaxGearItems = 32;
    struct PlayerGear
    {
        uint8_t playerId = 0;
        std::vector<uint32_t> items;  // base form ids of equipped armour, clothing and weapon
    };

    struct Hit
    {
        uint8_t playerId = 0;
        uint32_t targetRef = 0;  // reference id of who was hit
        float damage = 0;        // health they lost
    };

    constexpr float kMaxHealth = 100000;  // health and max health outside +-this are bad messages
    struct PlayerHealth
    {
        uint8_t playerId = 0;
        float health = 0;
        float maxHealth = 0;  // above 0
    };
    struct Damage
    {
        uint8_t playerId = 0;  // who it's for (must be the receiver)
        float amount = 0;      // health to lose
    };
    struct Revive
    {
        uint8_t playerId = 0;  // who is revived
    };
    constexpr uint8_t kMaxShots = 20;  // in one Attack; 0 or more than this is a bad message
    struct Attack
    {
        uint8_t playerId = 0;
        uint8_t shots = 1;     // shots fired, or 1 for a swing or a throw's start
        bool release = false;  // a throw (grenade, mine) left the hand: shots 0 (step 8.8)
    };
    // Where the group goes (DESIGN: doors pull everyone): the spot the traveller arrived at.
    struct Travel
    {
        uint8_t playerId = 0;  // who led the way
        uint32_t space = 0;    // worldspace or interior cell form id (not 0)
        float x = 0, y = 0, z = 0;
        float heading = 0;
    };
    // The host's NPCs near the group (DESIGN rule 5: friends' copies are puppets). Only the ones that changed,
    // plus each one now and then. Refs are the game's reference ids, the same in every game with the same
    // mods. Ones made while playing (FF......) exist only in the host's game (their ids mean something else, or
    // nothing, in a friend's): they come with the form a friend's game makes its own copy from (step 5.4).
    constexpr size_t kMaxNpcsPerMessage = 40;
    struct NpcState
    {
        uint32_t ref = 0;  // a shared ref (IsSharedRef); an FF...... one needs a base
        float x = 0, y = 0, z = 0;
        float heading = 0;
        uint8_t flags = 0;  // coop::Pose flags and the kNpc... flags below (unknown bits are rejected)
        uint16_t moves = 0;
        float health = 0;     // within +-kMaxHealth
        float maxHealth = 0;  // above 0, at most kMaxHealth
        uint32_t idle = 0;    // idle animation it's playing (form id; 0 none, never FF......)
        uint32_t weapon = 0;  // weapon it holds (base form id; 0 none, never FF......)
        uint32_t base = 0;    // for a ref made while playing: the NPC, creature or levelled list it's made from
                              // (never FF......); 0 for any other
        uint16_t limbs = 0;   // dead only (sent only then): body parts gone, a bit each (0-14: the game's body parts)
        uint8_t pile = 0;     // dead only: its body turned to a pile (kPile...: energy weapons' kills)
    };
    // How a body went (Alex, 2026-10-19 session: a plasma kill on the host left a normal body on the friend's game):
    // the game's goo (plasma), ash (lasers) and alien ash, each made by a spell's script (AttachAshPile).
    constexpr uint8_t kPileNone = 0, kPileGoo = 1, kPileAsh = 2, kPileAlien = 3, kPileLast = 3;
    struct NpcPile
    {
        uint32_t ref = 0;  // a shared ref (the host's id for it)
        uint8_t kind = 0;  // kPileGoo to kPileLast
    };
    Bytes Encode(const NpcPile& m);
    bool Decode(Reader& r, NpcPile& m);
    // Mines placed in the world (Alex, 2026-10-19 session: a friend's disarming set off the host's copy of the mine,
    // walked into by their puppet): one disarmed (turned off) or gone (picked up, blew up) in one game is in all.
    // Blown up in a friend's game, the host sets off its own for real (its NPCs hurt, everyone sees it); elsewhere
    // it's just gone.
    constexpr uint8_t kMineOff = 1, kMineGone = 2, kMineBlown = 3;
    struct MineChange
    {
        uint32_t ref = 0;   // the mine (a ref from the mod files: the same in every game)
        uint8_t state = 0;  // kMineOff, kMineGone or kMineBlown
    };
    Bytes Encode(const MineChange& m);
    bool Decode(Reader& r, MineChange& m);
    constexpr uint8_t kNpcDead = 0x80;     // NpcState flag: it's dead (its pose is where its body is)
    constexpr uint8_t kNpcDown = 0x40;     // knocked out (essential NPCs at 0 health, for one)
    constexpr uint8_t kNpcSitting = 0x20;  // sitting, or sitting down or getting up
    constexpr uint8_t kKnownNpcFlags = kKnownPlayerStateFlags | kNpcDead | kNpcDown | kNpcSitting;
    struct NpcStates
    {
        uint32_t timeMs = 0;  // host's clock
        uint32_t space = 0;   // worldspace or interior cell they're in (the host's)
        std::vector<NpcState> npcs;  // at most kMaxNpcsPerMessage, no ref twice
    };
    bool IsSharedRef(uint32_t ref);   // a ref the NPC messages may carry: not 0 or the player
    bool IsMadeWhilePlaying(uint32_t id);  // FF......: exists only in the game that made it
    struct NpcAttack
    {
        uint32_t ref = 0;    // a shared ref
        uint8_t shots = 1;   // 0 to kMaxShots: shots fired, or 1 for a swing, bite or a throw's start
        bool reload = false; // it started reloading (after the shots)
        bool release = false;  // a throw left its hand (step 8.8); a message with none of these is bad
    };
    struct NpcAttacks
    {
        std::vector<NpcAttack> attacks;  // 1 to kMaxNpcsPerMessage, no ref twice
    };

    // Killed: a script killed someone (Dead Money's collar): no downed state, the game is over at once.
    enum class GameOverReason : uint8_t { BledOut = 0, AllDown = 1, Killed = 2 };
    struct GameOver
    {
        GameOverReason reason = GameOverReason::AllDown;
        uint8_t playerId = 0;  // who bled out (BledOut only)
    };

    Bytes Encode(const Hello& m);
    Bytes Encode(const Welcome& m);
    Bytes Encode(const Reject& m);
    Bytes Encode(const PlayerJoined& m);
    Bytes Encode(const PlayerLeft& m);
    Bytes Encode(const PlayerState& m);
    Bytes Encode(const PlayerGear& m);  // sends at most kMaxGearItems
    Bytes Encode(const Hit& m);
    Bytes Encode(const PlayerHealth& m);
    Bytes Encode(const Damage& m);
    Bytes Encode(const Revive& m);
    Bytes Encode(const GameOver& m);
    Bytes Encode(const Attack& m);
    Bytes Encode(const Travel& m);
    Bytes EncodeRegroup();  // no contents
    Bytes Encode(const NpcStates& m);  // sends at most kMaxNpcsPerMessage
    Bytes Encode(const NpcAttacks& m);  // sends at most kMaxNpcsPerMessage

    // Call after reader.Type(). Each fails unless the whole message was read exactly.
    bool Decode(Reader& r, Hello& m);
    bool Decode(Reader& r, Welcome& m);
    bool Decode(Reader& r, Reject& m);
    bool Decode(Reader& r, PlayerJoined& m);
    bool Decode(Reader& r, PlayerLeft& m);
    bool Decode(Reader& r, PlayerState& m);
    bool Decode(Reader& r, PlayerGear& m);
    bool Decode(Reader& r, Hit& m);
    bool Decode(Reader& r, PlayerHealth& m);  // fails outside the limits above
    bool Decode(Reader& r, Damage& m);
    bool Decode(Reader& r, Revive& m);
    bool Decode(Reader& r, GameOver& m);  // fails on an unknown reason
    bool Decode(Reader& r, Attack& m);    // fails outside 1 to kMaxShots shots (0 for a release)
    bool Decode(Reader& r, Travel& m);    // fails on place 0
    bool Decode(Reader& r, NpcStates& m);  // fails on too many, a ref twice, or a ref that isn't shared
    // What one of the host's NPCs made while playing wears (its copy is dressed the same; armour and clothing).
    struct NpcGear
    {
        uint32_t ref = 0;              // made while playing
        std::vector<uint32_t> items;   // base form ids, at most kMaxGearItems, none 0 or FF......
    };
    Bytes Encode(const NpcGear& m);   // sends at most kMaxGearItems
    bool Decode(Reader& r, NpcGear& m);
    bool Decode(Reader& r, NpcAttacks& m);  // fails on none, too many, a ref twice, a bad ref, shot count or reload

    // Doors (step 6.1). Only doors from the game's files: never 0 or made while playing.
    constexpr size_t kMaxDoorsPerMessage = 64;
    constexpr uint8_t kDoorOpen = 0x01;
    constexpr uint8_t kDoorLocked = 0x02;
    struct DoorState
    {
        uint32_t ref = 0;
        uint8_t flags = 0;      // kDoorOpen, kDoorLocked
        uint8_t lockLevel = 0;  // 0-100, 255 needs a key
        bool operator==(const DoorState& o) const
        {
            return ref == o.ref && flags == o.flags && lockLevel == o.lockLevel;
        }
        bool operator!=(const DoorState& o) const { return !(*this == o); }
    };
    struct DoorStates
    {
        std::vector<DoorState> doors;  // 1 to kMaxDoorsPerMessage, no ref twice
    };
    bool IsDoorRef(uint32_t ref);  // a ref a door message may carry
    Bytes Encode(const DoorStates& m);  // sends at most kMaxDoorsPerMessage
    bool Decode(Reader& r, DoorStates& m);  // fails on none, too many, a ref twice, a bad ref or unknown flags
    Bytes EncodeDoorUse(const DoorState& m);
    bool DecodeDoorUse(Reader& r, DoorState& m);  // fails on a bad ref or unknown flags

    // Containers and bodies (step 6.2). Refs and items are never 0 or the player (but may be made while playing:
    // bodies of NPCs the host's game made, and items like those).
    constexpr size_t kMaxItemKinds = 500;      // kinds of items in one container
    constexpr int32_t kMaxItemCount = 1000000; // of one kind
    // A kind of item: its base form, its condition and its weapon mods (Alex, October 2026: a worn, modded gun stays
    // worn and modded when it goes from a container to a friend), and how many.
    constexpr uint8_t kNoCondition = 255;  // items without condition (ammo, aid, misc), and plain stacks of them
    struct ItemCount
    {
        uint32_t item = 0;     // base form id
        int32_t count = 0;
        uint8_t health = kNoCondition;  // condition, percent (0 to 100), or kNoCondition
        uint8_t mods = 0;      // weapon mods fitted (bits 1, 2, 4: the weapon's mod slots)
        bool SameKind(const ItemCount& o) const { return item == o.item && health == o.health && mods == o.mods; }
        bool operator==(const ItemCount& o) const { return SameKind(o) && count == o.count; }
    };
    struct ContainerChange
    {
        uint32_t ref = 0;
        uint32_t seq = 0;   // the friend's count of changes sent, from 1
        ItemCount change;   // count: negative taken out, positive put in (never 0)
    };
    struct ContainerContents
    {
        uint32_t ref = 0;
        uint32_t seen = 0;  // the last of your changes (seq) already in it
        std::vector<ItemCount> items;  // counts 1 or more, no item twice, at most kMaxItemKinds
    };
    bool IsContainerRef(uint32_t ref);
    Bytes EncodeContainerView(uint32_t ref);
    bool DecodeContainerView(Reader& r, uint32_t& ref);  // 0 (closed) or a container ref
    Bytes Encode(const ContainerChange& m);
    bool Decode(Reader& r, ContainerChange& m);    // fails on a bad ref or item, no change or too big a one
    Bytes Encode(const ContainerContents& m);      // sends at most kMaxItemKinds
    bool Decode(Reader& r, ContainerContents& m);  // fails on a bad ref, item or count, or an item twice
    // Items lying in the world (step 6.3).
    constexpr size_t kMaxItemsPerMessage = 64;
    struct ItemState
    {
        uint32_t ref = 0;
        uint32_t base = 0;      // what it is
        int32_t count = 1;      // how many in the stack
        bool present = true;    // false: taken (or, for one made while playing, gone)
        float x = 0, y = 0, z = 0;
        float rx = 0, ry = 0, rz = 0;  // its angles, radians, -pi to pi (step 8.9: knocked over)
        bool held = false;      // the host's player is carrying it (held there till let go)
        // What switches it on and off, if anything (its enable parent's own, the top one: a quest's marker; the game
        // ignores switching such an item itself), and whether that's on in the host's game.
        uint32_t parent = 0;
        bool parentOn = false;
    };
    constexpr float kMaxItemAngle = 7;  // (radians: a little over a full turn)
    struct ItemStates
    {
        std::vector<ItemState> items;  // 1 to kMaxItemsPerMessage, no ref twice
    };
    struct ItemTake
    {
        uint32_t ref = 0;   // the host's id for it
        uint32_t base = 0;  // what we got (taken back out of our inventory if the host's is gone)
        int32_t count = 1;
    };
    struct ItemDrop
    {
        uint32_t base = 0;
        int32_t count = 1;
        float x = 0, y = 0, z = 0;
    };
    Bytes Encode(const ItemStates& m);  // sends at most kMaxItemsPerMessage
    bool Decode(Reader& r, ItemStates& m);  // fails on none, too many, a ref twice, a bad ref, base, count or place
    Bytes Encode(const ItemTake& m);
    bool Decode(Reader& r, ItemTake& m);
    Bytes Encode(const ItemDrop& m);
    bool Decode(Reader& r, ItemDrop& m);
    // Items a friend's game moved (step 8.9: carried, pushed, knocked over or blown away): the host's game moves its
    // own there, and everyone sees it in its next item states.
    constexpr size_t kMaxItemMovesPerMessage = 32;
    struct ItemMove
    {
        uint32_t ref = 0;  // the host's id for it
        float x = 0, y = 0, z = 0;
        float rx = 0, ry = 0, rz = 0;
        bool held = false;  // the friend's player is carrying it
    };
    struct ItemMoves
    {
        std::vector<ItemMove> moves;  // 1 to kMaxItemMovesPerMessage, no ref twice
    };
    Bytes Encode(const ItemMoves& m);  // sends at most kMaxItemMovesPerMessage
    bool Decode(Reader& r, ItemMoves& m);  // fails on none, too many, a ref twice, a bad ref, place or angle
    // Quests (step 7.1).
    constexpr size_t kMaxQuestsPerMessage = 32;
    constexpr size_t kMaxQuestStages = 255;
    constexpr size_t kMaxQuestObjectives = 255;
    struct QuestObjective
    {
        uint32_t id = 0;
        uint8_t status = 0;  // 1 shown, 2 completed (3: both)
        bool operator==(const QuestObjective& o) const { return id == o.id && status == o.status; }
    };
    struct QuestState
    {
        uint32_t quest = 0;
        uint8_t flags = 0;   // the game's: 1 running, 2 completed, 0x20 shown in the Pip-Boy, 0x40 failed, ...
        uint8_t stage = 0;   // current stage
        std::vector<uint8_t> done;                // stages done
        std::vector<QuestObjective> objectives;   // every objective it has, shown or not
        bool operator==(const QuestState& o) const
        {
            return quest == o.quest && flags == o.flags && stage == o.stage && done == o.done &&
                objectives == o.objectives;
        }
    };
    struct QuestStates
    {
        std::vector<QuestState> quests;  // 1 to kMaxQuestsPerMessage, no quest twice
    };
    Bytes Encode(const QuestStates& m);  // sends at most kMaxQuestsPerMessage (and of each, the limits above)
    bool Decode(Reader& r, QuestStates& m);  // fails on none, too many, a quest twice or 0, or a bad status
    // Things switched on or off by the host's scripts (step 7.1: quests make NPCs and objects appear this way).
    constexpr size_t kMaxEnablesPerMessage = 64;
    struct RefEnable
    {
        uint32_t ref = 0;      // from the game's files (never made while playing)
        bool enabled = true;
        bool operator==(const RefEnable& o) const { return ref == o.ref && enabled == o.enabled; }
    };
    Bytes Encode(const std::vector<RefEnable>& m);  // sends at most kMaxEnablesPerMessage
    bool Decode(Reader& r, std::vector<RefEnable>& m);  // fails on none, too many, a bad ref or flag
    // Shared XP, karma and reputation (step 7.2).
    constexpr int32_t kMaxXpGain = 100000;
    constexpr int32_t kMaxKarmaChange = 10000;
    struct Progress
    {
        uint8_t playerId = 0;  // who gained it (set by the host)
        int32_t xp = 0;        // 0 to kMaxXpGain
        int32_t karma = 0;     // -kMaxKarmaChange to kMaxKarmaChange; xp and karma not both 0
    };
    Bytes Encode(const Progress& m);
    bool Decode(Reader& r, Progress& m);  // fails outside the limits above
    constexpr size_t kMaxReputationsPerMessage = 64;
    constexpr float kMaxReputation = 100000;
    struct Reputation
    {
        uint32_t form = 0;   // a reputation (faction) form from the game's files
        float fame = 0, infamy = 0;  // 0 to kMaxReputation
        bool operator==(const Reputation& o) const { return form == o.form && fame == o.fame && infamy == o.infamy; }
    };
    Bytes Encode(const std::vector<Reputation>& m);  // sends at most kMaxReputationsPerMessage
    bool Decode(Reader& r, std::vector<Reputation>& m);  // fails on none, too many, a bad form or value
    // Conversations (step 7.4).
    struct TalkState
    {
        uint8_t playerId = 0;  // who (set by the host)
        bool talking = false;
        uint32_t npc = 0;      // who with: a shared ref, or 0 (unknown, or not talking)
    };
    Bytes Encode(const TalkState& m);
    bool Decode(Reader& r, TalkState& m);  // fails on a bad flag or ref, or an NPC while not talking
    struct DialogueLine
    {
        uint32_t info = 0;     // the line (a dialogue response, TESTopicInfo) from the game's files
        bool onEnd = false;    // its script for when the line ends (false: when it starts)
        uint32_t speaker = 0;  // the NPC saying it: a shared ref
    };
    Bytes Encode(const DialogueLine& m);
    bool Decode(Reader& r, DialogueLine& m);  // fails on a bad form, ref or flag
    constexpr size_t kMaxSubtitleLength = 300;
    constexpr uint8_t kMaxResponse = 255;
    struct Subtitle
    {
        uint8_t playerId = 0;  // whose conversation (set by the host)
        uint32_t speaker = 0;  // a shared ref
        bool stop = false;     // they stopped talking (skipped, or done): nothing else is set
        uint32_t topic = 0;    // the line said (both from the game's files, or both 0: text only)
        uint32_t info = 0;
        uint8_t response = 0;  // which of its responses: its number in the game's files (0: the first)
        std::string text;      // 1 to kMaxSubtitleLength characters, no control characters
    };
    Bytes Encode(const Subtitle& m);
    bool Decode(Reader& r, Subtitle& m);  // fails on a bad ref, form, response or text
    // Sleep and wait (step 8.2). Hours 1 to kMaxWaitHours (WaitAsk: 0 cancels).
    constexpr uint8_t kMaxWaitHours = 24;
    constexpr uint8_t kNobody = 0xFF;  // no player (the host is player 0)
    struct WaitAsk { uint8_t hours = 0; bool sleep = false; };
    struct WaitVote { uint8_t requester = 0; uint8_t hours = 0; bool sleep = false; };
    struct WaitAnswer { bool yes = false; };
    struct WaitResult
    {
        uint8_t requester = 0;
        bool passed = false;
        uint8_t hours = 0;      // passed: everyone waits this long
        bool sleep = false;
        uint8_t refusedBy = kNobody;  // not passed: who said no (kNobody: no answer in time, or called off)
    };
    struct GameTime
    {
        float daysPassed = 0;  // GameDaysPassed (0 to 100000)
        float hour = 0;        // GameHour, 0 to 24
        uint8_t day = 1;       // GameDay, 1 to 31
        uint8_t month = 0;     // GameMonth, 0 to 11
        uint16_t year = 2281;  // GameYear
    };
    Bytes Encode(const WaitAsk& m);
    bool Decode(Reader& r, WaitAsk& m);
    Bytes Encode(const WaitVote& m);
    bool Decode(Reader& r, WaitVote& m);
    Bytes Encode(const WaitAnswer& m);
    bool Decode(Reader& r, WaitAnswer& m);
    Bytes Encode(const WaitResult& m);
    bool Decode(Reader& r, WaitResult& m);
    Bytes Encode(const GameTime& m);
    bool Decode(Reader& r, GameTime& m);
    // Linked saves (step 8.3): the host's save's name (checked again by coop::SaveNameOf before use).
    constexpr size_t kMaxSaveOrderName = 100;
    struct SaveOrder { bool load = false; std::string name; };
    // Host -> friend comparing from the main menu: our mods match; who the host is and the save it's playing ("" if
    // none yet), so the friend's game can load its match (step 8.6).
    struct ModsMatch { std::string hostName; std::string save; };
    // The desync detector (DESIGN rule 8): how many NPCs, items and doors a friend's game has disagreed with the
    // host's on for a while (sent when it changes; the friend's log says what).
    struct DesyncReport { uint16_t npcs = 0, items = 0, doors = 0; };
    // Companions (DESIGN: companions are the party's): the host's, and who each follows; anyone can tell one to
    // follow them or wait.
    constexpr size_t kMaxCompanions = 16;
    constexpr uint8_t kCompanionWaiting = 0xFF;  // CompanionState::leader: told to wait
    constexpr uint8_t kMaxPlayerId = 8;          // (player ids are small: 0 the host, friends from 1)
    struct CompanionState
    {
        uint32_t ref = 0;    // the host's id for it
        uint8_t leader = 0;  // the player it follows (0 the host), or kCompanionWaiting
        bool operator==(const CompanionState& o) const { return ref == o.ref && leader == o.leader; }
        bool operator!=(const CompanionState& o) const { return !(*this == o); }
    };
    struct CompanionStates { std::vector<CompanionState> companions; };  // 0 to kMaxCompanions, no ref twice
    enum class CompanionOrderKind : uint8_t { FollowMe = 1, Wait = 2 };
    struct CompanionOrder { uint32_t ref = 0; CompanionOrderKind order = CompanionOrderKind::FollowMe; };
    Bytes Encode(const CompanionStates& m);  // sends at most kMaxCompanions
    bool Decode(Reader& r, CompanionStates& m);
    Bytes Encode(const CompanionOrder& m);
    bool Decode(Reader& r, CompanionOrder& m);
    // Scripted sequences (Alex, October 2026: friends follow them, like Dead Money taking everyone's gear): what a quest
    // script in the host's game did to the host's player; each friend's game does it to its own.
    enum class PlayerCommandKind : uint8_t
    {
        AddItem = 1,          // form: item, n[0]: count, n[1]: no message
        RemoveItem,           // form: item, n[0]: count, n[1]: no message
        RemoveAllItems,       // ref: the container it all goes into
        RemoveAllTypedItems,  // ref: container, n[0]: keep owner, n[1]: quest items too, n[2]: type
        GiveAllItems,         // ref: a container whose items all go to the player
        EquipItem,            // form: item, n[0]: can't be taken off, n[1]: no message
        UnequipItem,          // form: item, n[0]: can't be put back on, n[1]: no message
        AddPerk,              // form: perk
        RemovePerk,           // form: perk
        AddNote,              // form: note
        RemoveNote,           // form: note
        AddImageSpaceModifier,     // form: image space modifier (the screen fading, going dark)
        RemoveImageSpaceModifier,  // form: image space modifier
        DisableControls,      // flags: the seven DisablePlayerControls switches
        EnableControls,       // flags: the seven EnablePlayerControls switches
        CastOnSelf,           // form: spell (CastImmediateOnSelf)
        AddPlayerPackage,     // form: an AI package the player runs (waking up on the floor, say: no control meanwhile)
        RemovePlayerPackage,  // the player's AI package is taken away (control back)
        Last = RemovePlayerPackage
    };
    struct PlayerCommand
    {
        PlayerCommandKind kind = PlayerCommandKind::AddItem;
        uint32_t form = 0;  // 0 or a form from the plugins (never FF......)
        uint32_t ref = 0;   // 0 or a reference from the plugins (never FF......)
        int32_t n[3] = {};  // each within +-kMaxPlayerCommandNumber
        uint8_t flags = 0;
    };
    constexpr int32_t kMaxPlayerCommandNumber = 100000;
    // Text chat: a friend's line goes to the host's game, which sends it on to the others with whose it is.
    struct ChatLine
    {
        uint8_t player = 0;  // who said it (from a friend: ignored, the host knows who sent it)
        std::string text;    // printable, kMaxChatText at most
        coop::VoiceStyle style;  // how it's read out (Valid)
    };
    constexpr size_t kMaxChatText = 160;
    // Voice chat: a little of a player's voice, as Steam's voice squeezes it (sent unreliably: late voice is no use).
    constexpr size_t kMaxVoiceBytes = 2048;
    struct VoiceData
    {
        uint8_t player = 0;  // whose (from a friend: ignored, the host knows who sent it)
        Bytes data;          // 1 to kMaxVoiceBytes
    };
    // How a player's character looks (Alex, October 2026: faces for release): what the game keeps on the player's
    // base form, so each game can make that player's character look the same. Forms are from the plugins (checked
    // where they're used); the face's numbers are the game's three sets of sliders (50, 30, 50).
    constexpr size_t kFaceValues = 130;
    constexpr size_t kMaxHeadParts = 8;
    constexpr float kMaxFaceValue = 50;
    struct PlayerLooks
    {
        uint8_t player = 0;  // whose (from a friend: ignored, the host knows who sent it)
        bool female = false;
        uint32_t race = 0, hair = 0, eyes = 0;  // 0: none
        float hairLength = 0;                   // 0 to 10
        uint32_t hairColor = 0;                 // RGBA
        float face[kFaceValues] = {};           // each within +-kMaxFaceValue
        std::vector<uint32_t> headParts;        // kMaxHeadParts at most (beards and the like)
        std::string name;                       // the character's name (printable, kMaxNameLength at most; "": none)
        bool operator==(const PlayerLooks& o) const;
    };
    Bytes Encode(const PlayerLooks& m);
    bool Decode(Reader& r, PlayerLooks& m);
    // Friendly fire: a friend's hit on another player's character (the host passes it on, or takes it itself).
    struct PlayerHit
    {
        uint8_t target = 0;  // the player hit
        float damage = 0;    // more than 0, kMaxHitDamage at most (checked where it's used)
    };
    Bytes Encode(const PlayerHit& m);
    bool Decode(Reader& r, PlayerHit& m);
    Bytes Encode(const VoiceData& m);
    bool Decode(Reader& r, VoiceData& m);
    Bytes Encode(const ChatLine& m);
    bool Decode(Reader& r, ChatLine& m);
    Bytes Encode(const PlayerCommand& m);
    bool Decode(Reader& r, PlayerCommand& m);
    // Synced radios (Alex, October 2026: listening to music together, the Pip-Boy's too): what a station plays now.
    // A station builds a run of lines at once (Radio New Vegas: DJ clips, then a song) and plays them in turn,
    // each part by part (its responses).
    struct RadioPick
    {
        uint32_t topic = 0;  // the topic it was picked from (never FF......)
        uint32_t info = 0;   // the line (never 0 or FF......)
    };
    constexpr uint8_t kMaxRadioRun = 16;
    struct RadioLine
    {
        uint32_t station = 0;  // the station's speaker (a talking activator reference; never FF......)
        uint32_t info = 0;     // the line it plays now (never 0 or FF......)
        uint32_t seq = 0;      // goes up each time it goes on (the same again: sent once more)
        uint32_t elapsed = 0;  // ms since the host's station started this part (under an hour)
        uint8_t part = 0;      // which of the line's responses (under 64)
        std::vector<RadioPick> run;  // the run of lines it built (kMaxRadioRun at most)
    };
    Bytes Encode(const RadioLine& m);
    bool Decode(Reader& r, RadioLine& m);
    constexpr uint16_t kMaxDesyncCount = 1000;
    Bytes Encode(const DesyncReport& m);
    bool Decode(Reader& r, DesyncReport& m);
    // An explosion that went off in the sender's game (step 8.8): the others show a harmless one right there, wherever
    // their copy of the grenade (or missile) ended up.
    struct ExplosionAt
    {
        uint32_t form = 0;   // its BGSExplosion (not 0, never FF......)
        uint32_t space = 0;  // worldspace or interior cell (not 0)
        float x = 0, y = 0, z = 0;
    };
    Bytes Encode(const ExplosionAt& m);
    bool Decode(Reader& r, ExplosionAt& m);
    Bytes Encode(const ModsMatch& m);
    bool Decode(Reader& r, ModsMatch& m);
    Bytes Encode(const SaveOrder& m);
    bool Decode(Reader& r, SaveOrder& m);  // fails on an empty or too long name
    // Loading (step 8.4).
    struct Freeze { bool frozen = false; uint8_t waitingFor = 0; };
    Bytes Encode(const Freeze& m);
    bool Decode(Reader& r, Freeze& m);
    Bytes Encode(const ModFiles& m);  // at most kModFilesPerMessage files
    bool Decode(Reader& r, ModFiles& m);
    constexpr uint32_t kModPieceSize = 32 * 1024;
    struct ModFileAsk
    {
        uint16_t index = 0;   // in the host's list
        uint64_t offset = 0;  // a multiple of kModPieceSize
    };
    struct ModPiece
    {
        uint16_t index = 0;
        uint64_t offset = 0;
        bool ok = true;       // false: the host couldn't read it
        Bytes data;           // at most kModPieceSize
    };
    Bytes Encode(const ModFileAsk& m);
    bool Decode(Reader& r, ModFileAsk& m);
    Bytes Encode(const ModPiece& m);
    bool Decode(Reader& r, ModPiece& m);
    Bytes EncodeItemRefund(const ItemCount& m);
    bool DecodeItemRefund(Reader& r, ItemCount& m);  // count: negative out of, positive into your inventory; fails on 0
}
