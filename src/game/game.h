#pragma once

// Everything that reads or changes the game world. Most of it runs small scripts we compile once,
// using the game's own commands (PlaceAtMe, SetPos, ...) so other mods see normal game behaviour.
// Only call from the game thread.

#include "coop/smoothing.h"
#include "net/protocol.h"

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

struct NVSEInterface;
class TESForm;
class TESObjectREFR;

namespace coop { struct Balance; }

namespace game
{
    bool Init(const NVSEInterface* nvse);  // from NVSEPlugin_Load
    bool ListenForHits();                  // once all plugins are loaded
    bool ListenForEnables();               // once all plugins are loaded (xNVSE's OnEnable / OnDisable)
    bool CompileScripts();                // once game data is loaded; safe to call again

    TESObjectREFR* Player();
    TESForm* LookupForm(uint32_t formId);
    TESObjectREFR* LookupRef(uint32_t refId);
    uint32_t RefId(const TESObjectREFR* ref);
    bool GetPose(const TESObjectREFR* ref, coop::Pose& out);  // false while not in a cell (loading)

    uint8_t StanceFlags(TESObjectREFR* ref);  // coop::Pose flags (weapon out, sneaking) an actor really has
    // The same, and the game's GetSitting (0 not, 1 and 2 sitting down, 3 sitting, 4 getting up), in one look.
    void Stance(TESObjectREFR* ref, uint8_t& flags, int& sitting);
    std::vector<uint32_t> PlayerGear();  // base form ids of what the player has equipped
    // What an actor has equipped (base form ids; armourOnly: armour and clothing only). Twenty script calls:
    // not every frame.
    std::vector<uint32_t> Gear(TESObjectREFR* ref, bool armourOnly);
    bool Wear(TESObjectREFR* ref, uint32_t formId);  // adds and equips armour or clothing; false if it isn't
    void Unequip(TESObjectREFR* ref, uint32_t formId);  // takes it off (keeps it)
    bool IsPastIntro();  // character made and out of Doc Mitchell's house
    std::vector<std::string> LoadOrder();  // the loaded mods' file names, in load order (empty if unknown)
    std::vector<std::string> LoadedMods();  // the same, from the game's own list (also at the main menu)

    // A stand-in character next to the player: its own kind of character, with their name, no dialogue and no
    // items.
    TESObjectREFR* SpawnStandIn(const char* name, const net::PlayerLooks* looks = nullptr);

    void BringToPlayer(TESObjectREFR* ref);
    // A proper teleport onto another ref (the game's MoveTo): also drops a character's route, unlike Move.
    void MoveOnto(TESObjectREFR* ref, TESObjectREFR* to);
    // Companions (DESIGN: companions are the party's). Followers: those following our player now (JIP's
    // GetFollowers: the game's own list; one told to wait isn't on it). IsTeammate: the game's GetPlayerTeammate.
    std::vector<uint32_t> Followers();
    bool IsTeammate(TESObjectREFR* ref);
    std::vector<uint32_t> Teammates();                  // all of them (JIP's GetTeammates), waiting or not
    bool IsWaiting(TESObjectREFR* ref);                 // its script's Waiting is on (told to wait)
    void SetWaiting(TESObjectREFR* ref, bool waiting);  // the game's own way: its Waiting, then EVP
    // A copy of the game's plain follow package aimed at `target` (null if it couldn't be made); given to a companion
    // (AddScriptPackage) it follows them till it's taken away (RemoveScriptPackage, then EVP).
    TESForm* FollowPackage(TESObjectREFR* target);
    void GivePackage(TESObjectREFR* ref, TESForm* package);
    void DropPackage(TESObjectREFR* ref);
    uint32_t CurrentPackage(TESObjectREFR* ref);
    bool InCombat(TESObjectREFR* ref);
    bool CompanionWheelOpen();  // our player's companion wheel (menu 1075, JIP GameUI.h) is open

    // Scripted sequences (Alex, October 2026: friends follow them, like Dead Money taking everyone's gear). Host: once,
    // our watch on a few of the game's script commands (items, equipping, perks, notes, the screen's image space,
    // controls); while on, what quest scripts do with them to our player is kept for TakePlayerCommands. Not dialogue
    // lines' scripts (what a conversation gives stays the talker's: DialogueLineRunning), nor ours.
    bool WatchPlayerCommands();
    void MirrorPlayerCommands(bool on);
    void ScreenFromHost(bool on);
    // The DLCs' and packs' intro popups (their boxes when you come out of Doc Mitchell's, the packs' "added" notes)
    // skipped while on; what they start and give still happens. loadOrder: the mods' file names (LoadOrder).
    void SkipIntroPopups(bool on, const std::vector<std::string>& loadOrder);
    bool TakeScriptKill();  // a script (not ours) killed our player since the last call  // friend: our scripts can't change the screen's image space or our controls
    void DialogueLineRunning(bool running);
    std::vector<net::PlayerCommand> TakePlayerCommands();
    // Friend: does it to our player (checked: the host's message could say anything). False if it couldn't.
    bool ApplyPlayerCommand(const net::PlayerCommand& c);
    void Move(TESObjectREFR* ref, const coop::Pose& pose);  // teleport, and face pose.heading
    // The player goes to another place (JIP's MoveToCell), or far in this one (the game's MoveTo, which loads
    // the ground there; JIP's MoveToCell only slides the player over within a worldspace).
    // False if pose.space isn't a worldspace or interior cell in this game.
    bool TravelTo(const coop::Pose& pose);
    void Turn(TESObjectREFR* ref, float heading);           // radians
    // Looks up or down (radians, + down): an actor's upper body and weapon bend that way (lab coop pitch, step 8.7).
    void SetPitch(TESObjectREFR* ref, float pitch);
    // Lab: logs the actor's weapon animations (level, up, down) and the three the game tilts by its pitch.
    void LogAimSlots(TESObjectREFR* ref);
    void SetWeaponOut(TESObjectREFR* ref, bool out);        // an actor draws or puts away its weapon
    void SetAlert(TESObjectREFR* ref, bool alert);          // alert: its AI keeps its weapon out (game's SetAlert)
    void SetRestrained(TESObjectREFR* ref, bool on);        // on: its AI runs no routine (game's SetRestrained)
    // Off: it never starts fighting (JIP SetCombatDisabled), and never runs out of ammo or reloads by itself (JIP
    // SetTeammateUsingAmmo): for copies of the host's NPCs, whose shots are only shown.
    void SetCombatOff(TESObjectREFR* ref, bool off);
    void EvaluatePackage(TESObjectREFR* ref);               // its AI picks its routine afresh (game's EvaluatePackage)
    bool Jump(TESObjectREFR* ref);                          // an actor jumps (false if it can't now: far away)
    // An actor's movement flags (coop::moves): which way it moves and how. The game picks its walk, run and
    // sneak animations from these. 0 for anything else.
    uint16_t MovementFlags(TESObjectREFR* ref);
    void SetMovementFlags(TESObjectREFR* ref, uint16_t flags);
    void RemoveAllItems(TESObjectREFR* ref);
    // Gives and equips one item; weapons as a harmless copy (no damage: its player's real hits count instead).
    // False if the id isn't armour, clothing or a weapon in this game.
    bool AddAndEquip(TESObjectREFR* ref, uint32_t formId);
    void RemoveItem(TESObjectREFR* ref, uint32_t formId);  // (for weapons: the harmless copy)
    // Our player's attacks since the last call: shots fired (rounds that left the gun), or 1 when a swing or
    // throw starts; release: a throw (grenade, mine) left our hand (step 8.8). Call every frame.
    struct MyAttack
    {
        uint8_t shots = 0;
        bool release = false;
    };
    MyAttack TakeMyAttacks();
    // A stand-in attacks with its weapon (from `worn`, given with AddAndEquip): the attack animation (with its
    // sound; a throw's or a mine's for those), and for guns and launchers the shots, from the harmless copy (its
    // projectile's explosion harmless too: step 8.8). ReleaseThrow: the throw leaves its hand (the harmless
    // grenade or mine flies).
    void PlayAttack(TESObjectREFR* ref, const std::vector<uint32_t>& worn, uint8_t shots);
    void ReleaseThrow(TESObjectREFR* ref, const std::vector<uint32_t>& worn);
    void Remove(TESObjectREFR* ref);  // disable and delete
    void Notify(const char* text);    // corner message on screen
    // A Yes/No question in the game's message box (they wait in the game's queue of boxes). id (0 to 15): whose
    // question (TakeAnswer); asking the same id again forgets the earlier one. False if it couldn't be shown.
    bool AskYesNo(const std::string& text, int id);
    void ShowMessage(const std::string& text);  // in the game's message box, with an OK button
    void RetryMessageBoxes();  // every frame: our boxes wait for no box to be open, then show (one a frame)
    // Once, before the main menu first opens: adds Host and Join to it (after Load); clicking calls these.
    bool AddCoopToMainMenu(void (*onHost)(), void (*onJoin)());
    bool MainMenuMade();  // the main menu's options were made (it's up, or has been)
    // Once: every load of a save asks `mayLoad` first (the save's name, without folder or .fos); false: not loaded.
    bool InstallLoadCheck(bool (*mayLoad)(const std::string& save));
    int TakeAnswer(int id);  // the answer to question id, once: 1 yes, 0 no, -1 none (yet)
    // A question with up to 10 buttons (the first ones of `buttons`); TakeChoice: the button clicked (0 the first),
    // once, -1 if none (yet), or kNeverShown once if the game wouldn't show it for 10 s. Shared ids with AskYesNo.
    // (Ours wait in our own queue till no box is open, shown by RetryMessageBoxes; so AskChoice is always true.)
    constexpr int kNeverShown = -2;
    bool AskChoice(const std::string& text, const std::vector<std::string>& buttons, int id);
    int TakeChoice(int id);
    // Question id was answered another way (the lab): its box closes if it's on screen, or leaves our queue.
    void DropQuestion(int id);
    bool QuestionOpen();  // one of ours is on screen or waiting (or answered, not yet taken); ids are 0 to 15
    // The message box on screen now: whether it says `text` (another plugin's may be on screen, ours waiting behind
    // it: 2026-10-16, the laptop's progress went into a "Requirements Checker" box), its text (it grows taller, never
    // wider), a button's label. False if there's none.
    bool MessageShows(const std::string& text);
    std::string MessageText();  // what it says ("" if none)
    bool SetMessageText(const std::string& text);
    bool SetMessageButton(int index, const std::string& label);
    // The main menu's Load list, as if Load were clicked (only while the main menu is open, with saves).
    void OpenLoadMenu();
    void QuitGame();  // as the main menu's Exit Game does
    // Whether the game keeps running when it isn't the active window (co-op: others play on in it). Returns what it
    // was.
    bool RunInBackground(bool on);
    // Shown for this long (JIP's MessageExAlt). Corner messages queue: it waits for any still showing.
    void Notify(const char* text, float seconds);

    // Developer tools. The inspector's AI bits (dev::AiBits, packed); false if not an actor or not ready.
    bool AiStateBits(TESObjectREFR* ref, uint32_t& out);
    int AnimAction(TESObjectREFR* ref);  // what its animation is doing (attack, reload...), -1 none or not known
    void ConsolePrint(const char* text);  // a line in the game's console (~)

    bool IsActor(const TESObjectREFR* ref);  // a person or creature
    // Actors the game fully simulates right now (its high-process list: the ones near the player), player
    // included. Cheap: reads the game's own list.
    void NearbyActors(std::vector<TESObjectREFR*>& out);
    const char* Name(const TESObjectREFR* ref);  // an actor's name, or "" if it has none
    // What a ref is made from (step 5.4 lab): its base form, and for a levelled actor (base made while playing)
    // the two forms its ExtraLeveledCreature holds. Ids 0 when there are none; types are TESForm::typeID.
    struct BaseInfo
    {
        uint32_t base = 0, leveledBase = 0, leveledActor = 0;
        uint8_t baseType = 0, leveledBaseType = 0, leveledActorType = 0;
    };
    BaseInfo DescribeBase(TESObjectREFR* ref);
    // A new actor next to the player, made from an NPC, creature or levelled list (null for anything else).
    TESObjectREFR* PlaceActor(uint32_t baseId);
    bool Is3DLoaded(TESObjectREFR* ref);     // its body (3D model) is in the world
    float Health(TESObjectREFR* ref);
    float MaxHealth(TESObjectREFR* ref);
    void ChangeHealth(TESObjectREFR* ref, float amount);  // negative hurts, positive heals
    // Put a stand-in in the player's faction (DESIGN rule 7: friends are companions), ignoring friendly hits
    // and not starting or joining fights, with lots of health so a big hit can't kill them before we undo it.
    void MakeCompanion(TESObjectREFR* ref);
    // If they're fighting, stop (enemies can still attack them). A stand-in's player does its fighting.
    void StopFighting(TESObjectREFR* ref);
    // Downed, not dead (DESIGN): while on, 0 health knocks the player out instead of killing them.
    // Only changes the flag in memory; call with false when co-op ends.
    void SetPlayerEssential(bool on);
    // Same for an actor's base form (shared by every actor of that kind). Memory only, like the player's.
    void SetEssential(TESObjectREFR* ref, bool on);
    void KnockOut(TESObjectREFR* ref);  // to below 0 health: an essential actor falls and lies there
    void KillPlayer();  // game over: the normal death screen, then the player reloads
    // The game's Kill: not essential first, and with those body parts cut off (a bit each; 0: none). On a body, cuts
    // those off.
    void Kill(TESObjectREFR* ref, uint16_t limbs = 0);
    uint16_t LimbsGone(TESObjectREFR* ref);  // body parts cut off (a bit each, 0-14: IsLimbGone)
    void SetMaxHealth(TESObjectREFR* ref, float health);  // its base health (game's SetActorValue)
    uint32_t PlayedIdle(TESObjectREFR* ref);  // idle animation it's playing (JIP GetPlayedIdle), 0 none
    bool PlayIdle(TESObjectREFR* ref, uint32_t idleId);  // JIP PlayIdleEx; false if the id isn't an idle here
    // A copy of a host's NPC attacks: like PlayAttack, with a harmless copy of the gun it holds.
    void PlayNpcAttack(TESObjectREFR* ref, uint8_t shots);
    void ReleaseNpcThrow(TESObjectREFR* ref);
    // Base form id of the weapon it holds (for a harmless copy, the weapon it copies); 0 none.
    uint32_t HeldWeaponId(TESObjectREFR* ref);
    bool HoldsHarmless(TESObjectREFR* ref);  // its weapon is one of our harmless copies
    // An NPC's gun, followed from call to call: shots since the last call (rounds that left its magazine), or -1
    // if it holds no gun (melee, unarmed or thrown). Also notes the weapon it holds (as HeldWeaponId).
    struct GunState { uint32_t weapon = 0; int rounds = -1; };
    int CountShots(TESObjectREFR* ref, GunState& state);
    void StopIdle(TESObjectREFR* ref);   // JIP StopIdle
    void Resurrect(TESObjectREFR* ref);  // the game's ResurrectActor (it gets up)
    // The characters we made (friends', copies of the host's NPCs): kept in the save (a JIP auxiliary variable
    // on the player), so loading it can delete them. Note them when they change; read them after a load.
    void NoteMadeCharacters(const std::vector<uint32_t>& refIds);
    // The lab channel (dev/lab_channel.h): a line run as if typed in the game's console (false if it wasn't
    // taken), and an expression's value worked out by a script, as text.
    bool RunConsoleLine(const char* line);
    std::string Evaluate(const char* expression);
    std::vector<uint32_t> SavedMadeCharacters();
    // Out of a leftover animation: 1 JIP StopIdle, 2 kNVSE PlayGroupAlt Idle, 3 kNVSE ForceStopIdle (stand lab).
    void ResetAnimation(TESObjectREFR* ref, int how);
    // Playing an idle or script animation: it doesn't draw or attack until that ends (step 5.3 test: copies stuck
    // in a looping "TalkCombat" idle never drew).
    bool InScriptAnimation(TESObjectREFR* ref);
    // The game's reload (addr::kActorReload) of the weapon it holds; animType 1 plays the animation. False if it
    // holds none, or its function table points elsewhere (ReloadFunction says where).
    bool Reload(TESObjectREFR* ref, int animType);
    uintptr_t ReloadFunction(TESObjectREFR* ref);
    bool IsAttackAction(int action);  // an AnimAction value: its attack animation is playing
    bool IsReloadAction(int action);  // an AnimAction value: it's reloading
    bool IsThrowRelease(int action);  // an AnimAction value: a throw (grenade, mine) is leaving its hand

    // Real explosions starting in our game (step 8.8: everyone shows them where they really went off), since the
    // last call; our harmless ones aren't among them. Once: WatchExplosions.
    struct SeenExplosion
    {
        uint32_t form = 0;      // its BGSExplosion
        uint32_t space = 0;     // worldspace or interior cell
        float x = 0, y = 0, z = 0;
        bool ours = false;      // our player caused it
    };
    bool WatchExplosions();
    std::vector<SeenExplosion> TakeExplosions();
    // A harmless explosion of that kind at that spot (another player's game's real one).
    bool ShowExplosion(uint32_t form, uint32_t space, float x, float y, float z);
    bool IsAttackPhase(int action);   // attacking, following through, waiting between shots or throwing
    void SetUnconscious(TESObjectREFR* ref, bool out);  // freeze (doesn't make them fall)
    // Out of the game's knock-out: the player's essential knock-out never ends by itself.
    void WakeUp(TESObjectREFR* ref);
    void SetPlayerControls(bool on);  // off: no moving, fighting or Pip-Boy; looking around still works
    bool IsControlPressed(int control);  // follows the player's key bindings
    int LifeState(const TESObjectREFR* ref);  // Actor::lifeState (0 alive), or -1 if not an actor
    bool IsKnockedOut(const TESObjectREFR* ref);
    bool IsDead(const TESObjectREFR* ref);  // dying or dead
    bool InMenu();  // a menu or dialogue is open
    bool IsKeyPressed(int key);  // a DirectInput key code, down outside menus
    // Text chat's line on the HUD: OpenChat starts typing into it (the keys are kept from the game till Enter or
    // Esc); UpdateChat (every frame) draws it and gives the lines sent with Enter. TypeChat: as if typed (lab).
    bool OpenChat();
    bool ChatTyping();
    void CancelChat();  // the line closes unsent (chat isn't available now: a load, out of co-op)
    bool KeyDown(int key);
    // Co-op balanced (REBALANCE.md step 2): the game's damage multipliers, base carry weight and Stimpaks set to the
    // balance's; null puts the game's own back (read before the first change).
    bool ApplyBalance(const coop::Balance* balance);
    // Quick start (DESIGN build order 15): the tutorial in Doc Mitchell's house skipped by its own steps. QuestStage:
    // -1 if the quest isn't running. LeaveTutorial: what Doc's farewell does (Pip-Boy and jumpsuit on, the note, the
    // door, the tutorial done, Sunny's quest). ActivateByPlayer: a door walked through, or (moveHereFirst) an
    // activator brought to the player and set off. GiveStarterGear: Doc's starting gear, by tag skills.
    int QuestStage(uint32_t quest);
    bool LeaveTutorial();
    void RemoveTutorialFades();  // the tutorial's screen effects (the waking-up fade) off
    void HoldTutorial(bool hold);  // its timer held (Doc doesn't start talking) or let go on
    bool ActivateByPlayer(uint32_t ref, bool moveHereFirst);
    bool GiveStarterGear();
    // Voice chat and speech: an actor's mouth open this much (0 to 1; the game's "BigAah" mouth shape, in each of its
    // face's phoneme groups). Set every frame while it should move; false: no face (not loaded, too far).
    bool SetMouth(TESObjectREFR* actor, float opening);           // a DirectInput key code held, menus or not, while the game's window is in front
    void ShowTalking(bool talking);  // voice chat: "Talking" where the chat line is
    void TypeChat(unsigned char c);
    std::vector<std::string> UpdateChat();
    // Text chat: a player's line floating above their character (slot: one per player) for a few seconds while
    // they're on screen and near; UpdateChatAbove (every frame) keeps it above their head.
    void ShowChatAbove(int slot, const TESObjectREFR* who, const std::string& text);
    void UpdateChatAbove();
    // Friends' markers (a chevron and their name above them, on the HUD): the friends' characters to mark, with
    // their names (4 at most; none hides them). Call every frame.
    void ShowFriendMarkers(const std::vector<std::pair<TESObjectREFR*, std::string>>& friends);
    float DistanceToPlayer(const TESObjectREFR* ref);  // (huge without one)
    bool InLoadingScreen();  // the loading screen is up (MenuMode 1007)

    // How far from our player the game's loaded cells reach everywhere (outdoors): the game loads uGridsToLoad cells
    // across around the player's (5 by default: 2 each way, 8192 units, ~117 m; 7: 12288). Our ranges follow it, so a
    // group that raises uGridsToLoad gets the reach (DESIGN build order 9).
    float LoadedReach();
    int LoadedGrid();  // uGridsToLoad as the game uses it (5 if it can't be read)
    int ExteriorCellBuffer();  // uExterior Cell Buffer (-1 if it can't be read)
    bool InMenuMode();  // the game's own "a menu is up" (the main menu too)
    // The menus open now, as text ("inventory, 1004"; "" none), into `out` (safe in a crash: no memory taken, and a
    // bad read gives "?"). For the log: which menu was up when the renderer crashed (two friends, 2026-10-19).
    void OpenMenus(char* out, size_t size);
    // The world's field of view (the console's fov: the world's, then the hands' and weapon's, kept as they are);
    // 0 puts back what the game had. Gives false if it couldn't (no player yet).
    bool SetFieldOfView(int degrees);
    // How our player's character looks (false if it can't be read), and another player's character made to look
    // like theirs (its own base form, a copy made for them; forms that aren't there or aren't the right kind are
    // left as they were).
    bool ReadMyLooks(net::PlayerLooks& out);
    bool ApplyLooks(TESObjectREFR* standIn, const net::PlayerLooks& looks);

    // Doors (step 6.1).
    // The doors near the player: indoors the whole room; outdoors those within `range` (game units) in the loaded
    // cells. Walks every object in those cells: call when the player changes cell, not every frame.
    void DoorsNear(std::vector<TESObjectREFR*>& out, float range);
    struct DoorInfo
    {
        int open = 0;          // the game's GetOpenState: 1 open, 2 opening, 3 closed, 4 closing (0: not a door; a
                               // container with a lock: always 3, only its lock counts)
        bool locked = false;
        int lockLevel = 0;     // 0-100, or more for "needs a key"
        bool leadsAway = false;  // it takes you to another place (doors that pull everyone, step 3)
    };
    DoorInfo ReadDoor(TESObjectREFR* ref);
    int GameSaysLocked(TESObjectREFR* ref);  // the game's GetLocked (dev: to check ReadDoor's)
    // Energy weapons' kills: which pile a dying body is turning into (net::kPile...; 0: none, or the effect is over),
    // and that pile made on a body (the game's own spell for it).
    uint8_t PileKind(TESObjectREFR* actor);
    void MakePile(TESObjectREFR* actor, uint8_t kind);
    void SetDoorOpen(TESObjectREFR* ref, bool open);  // the game's SetOpenState
    void SetDoorLocked(TESObjectREFR* ref, bool locked, int level);  // the game's Lock / Unlock

    // Containers and bodies (step 6.2).
    void ContainersNear(std::vector<TESObjectREFR*>& out, float range);  // like DoorsNear (lab)
    // Mines (Alex, 2026-10-19 session): those lying near (as DoorsNear; placed in the world or thrown down), and
    // one's state: 0 armed, net::kMineOff disarmed, net::kMineGone blown up or picked up; -1 not a mine.
    void MinesNear(std::vector<TESObjectREFR*>& out, float range);
    int MineState(const TESObjectREFR* ref);
    bool TurnOffMine(TESObjectREFR* mine);  // the game's disarming, quietly (no sound, XP or stat): true if done
    // A mine's own explosion where it lies (owned by no one, as a mine going off by itself). Take the mine away first:
    // the blast set the mine itself off as well (lab 2026-10-19: a second, MediumExplosion). False if not a mine.
    bool MineExplosion(TESObjectREFR* mine);
    // Which actors near a mine don't set it off: friends' characters (`puppets`, up to 8: each game's mines go off
    // for its own player only, never for someone's puppet), and in a friend's game (`friendGame`) none at all for
    // mines placed in the world (the host's NPCs set off the host's; ours are copies). Every frame (the mines' check
    // reads these as it goes). False if the hook couldn't go in.
    bool SetMineRules(TESObjectREFR* const* puppets, size_t count, bool friendGame);
    // What the player's container screen shows (box or body), or while trading the merchant's container; or null.
    TESObjectREFR* OpenContainer();
    // A kind of item (base form id, condition percent or net::kNoCondition, weapon mods) and how many.
    struct Item { uint32_t id; int32_t count; uint8_t health = net::kNoCondition; uint8_t mods = 0; };
    constexpr size_t kMaxItemKinds = 500;
    // Everything it holds, as the game counts it; false if it can't be read (or holds more than kMaxItemKinds).
    bool Contents(TESObjectREFR* ref, std::vector<Item>& out);
    void ChangeItem(TESObjectREFR* ref, uint32_t itemId, int32_t delta);  // AddItem / RemoveItem, no message
    // The same for one kind: added at that condition with those mods (JIP AddItemAlt, SetWeaponRefModFlags), or taken
    // from the stacks of that kind (xNVSE inventory references).
    void ChangeItem(TESObjectREFR* ref, const net::ItemCount& kind);
    void Activate(TESObjectREFR* ref);  // the player uses it (a container opens its screen)
    void RefreshItemList();  // the open container screen shows its contents afresh (JIP RefreshItemsList)
    bool IsItem(uint32_t formId);  // a form that can be in an inventory (the game's own test)
    bool IsLootable(TESObjectREFR* ref);  // a container, or a dead person or creature

    // Quests (step 7.1), read and written directly: no quest script runs.
    struct QuestInfo
    {
        uint32_t id = 0;
        std::string name;      // editor id
        uint8_t flags = 0;     // 1 running, 2 completed, 0x20 shown in the Pip-Boy, 0x40 failed
        uint8_t stage = 0;     // current stage
        float delay = 0;       // seconds between its script's runs
        std::vector<uint8_t> done;  // stages done
        std::vector<std::pair<uint32_t, uint32_t>> objectives;  // id, status (1 shown, 2 completed)
    };
    // Every quest the game has (started ones only unless all).
    void ReadQuests(std::vector<QuestInfo>& out, bool all);
    bool ReadQuest(uint32_t id, QuestInfo& out);
    void SetQuestFlags(uint32_t id, uint8_t flags);
    void SetQuestStageDone(uint32_t id, uint8_t stage, bool done);
    void SetQuestObjective(uint32_t id, uint32_t objective, uint32_t status);
    uint32_t TrackedQuest();  // the quest the player tracks, 0 none
    // The game's own SetObjectiveDisplayed / SetObjectiveCompleted / CompleteQuest: they also update the player's
    // objective list (Pip-Boy, map markers, the corner messages), which writing the quest directly doesn't.
    bool ShowObjective(uint32_t quest, uint32_t objective, bool on);
    bool CompleteObjective(uint32_t quest, uint32_t objective, bool on);
    bool CompleteQuest(uint32_t quest);
    // Things a script switched on (Enable) or off (Disable) since the last call (refs from the game's files only).
    struct EnableChange { uint32_t ref; bool enabled; };
    std::vector<EnableChange> TakeEnables();
    void SetEnabled(TESObjectREFR* ref, bool enabled);  // the game's Enable / Disable
    bool InDialogue();  // a conversation is open (MenuMode 1009)
    // XP and karma (step 7.2).
    int32_t PlayerXp();      // the player's XP actor value (all ever gained)
    int32_t PlayerKarma();
    void GiveXp(int32_t xp);        // JIP RewardXPExact (the game's XP sound; levels up as usual)
    void GiveKarma(int32_t karma);  // the game's RewardKarma (its karma message)
    struct ReputationInfo { uint32_t form; std::string name; float fame, infamy; };
    std::vector<ReputationInfo> ReadReputations();
    void WriteReputation(uint32_t form, float fame, float infamy);
    void CloseMenu();   // JIP CloseActiveMenu
    // Conversations (step 7.4).
    TESObjectREFR* DialoguePartner();  // who the open conversation screen is with, or null
    // What the one we're talking to is saying now. Cheap.
    struct Spoken
    {
        uintptr_t id = 0;        // changes with each response (0: none)
        const char* text = "";
        uint32_t topic = 0, info = 0;
        int response = 0;        // which of the line's responses (its number in the game's files, from 1)
    };
    Spoken SpokenNow();
    bool IsTalking(const TESObjectREFR* actor);  // saying something now
    // Once: our hooks that let an NPC say an exact line (see SayLine).
    bool InstallSayHooks();
    // Once: VATS stops changing the game's speed (no kill slow motion). Returns how many of its calls it skips.
    int KeepVatsAtFullSpeed();
    // The game's speed (1 normal; the loading freeze uses 0.0001, as the game's own VATS does).
    void SetWorldSpeed(float speed);
    // Lab: logs each change to the game's speed and the game code that asked for it.
    bool WatchSpeedChanges();
    // `speaker` says response number `response` of line `info` (topic `topic`; 0: the first) as the game's Say
    // command does: its voice, mouth and subtitle (what Floating Subtitles and the like show), following the
    // "dialogue subtitles" option; its scripts don't run. False if it couldn't. `sound` gets its voice.
    struct Sound { uint32_t id = 0xFFFFFFFF; uint8_t assumeSuccess = 0; uint32_t state = 0; };
    bool SayLine(TESObjectREFR* speaker, uint32_t topic, uint32_t info, int response, Sound& sound);
    void StopSound(Sound& sound);
    // Stops what this NPC is saying (a line said through the game's own speech); true if it was still talking.
    bool StopSpeaking(const TESObjectREFR* speaker);
    bool IsPlaying(Sound& sound);
    // A subtitle with this text, shown or waiting, ends now (a line someone skipped).
    void EndSubtitle(const std::string& text);
    // Said here by SayLine lately (for someone else's conversation): its scripts aren't ours to run.
    bool SaidForOthers(const TESObjectREFR* speaker, const void* info);
    // Synced radios. What our radio stations went on to (a line and its part: Radio New Vegas' DJ intro, then
    // the song) since the last call; fromHost: a friend's station got to the host's line.
    struct RadioLineStart
    {
        uint32_t station = 0, info = 0, part = 0;
        size_t line = 0;                   // which of the run
        std::vector<net::RadioPick> run;   // the run of lines the station built
        bool fromHost = false;
    };
    std::vector<RadioLineStart> TakeRadioLineStarts();
    // Radio lab (coop radio N): the Pip-Boy's radio on, tuned to this station.
    bool TunePipboyRadio(TESObjectREFR* station);
    void RadioFromHost(bool on);
    // Friend, while on: a station of ours goes on to the host's line and part (handed the line when it picks
    // one), then to the same spot (`elapsed`: ms since the host's started the part).
    void HostRadioLine(const net::RadioLine& line);
    void UpdateRadios();  // (every frame: reads what our stations play)
    // A subtitle on our HUD, as if `speaker` said it near us (the game's own, for NPCs talking nearby).
    void ShowSubtitle(TESObjectREFR* speaker, const char* text, bool always = false);  // always: even if subtitles are off
    void* TopicInfo(uint32_t id);      // a dialogue line (TESTopicInfo) by form id, or null if it isn't one
    uint32_t TopicInfoId(const void* info);
    std::string TopicInfoPrompt(const void* info);  // what the player says to pick it ("" none)
    bool HasResultScript(void* info, bool onEnd);  // the game's GetResultScript (may read the game's files)
    // Its result script uses a command that opens a screen (opcodes::kOpensScreen): that one, else 0.
    // (statements: set to how many it read, or -1 if they didn't add up; lab.)
    uint16_t ScreenCommandIn(void* info, bool onEnd, int* statements = nullptr);
    void Greet(TESObjectREFR* actor);  // it starts a conversation with the player (lab)
    bool IsEnabled(const TESObjectREFR* ref);
    // The player's own list of shown objectives (quest id, objective id, status), and how many targets it has.
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> PlayerObjectives(size_t& targets);
    // A fingerprint of every quest's state (flags, stage, stages done, objectives): cheap, nothing copied.
    void QuestFingerprints(std::vector<std::pair<uint32_t, uint32_t>>& out);
    // Sets a quest as given: flags, current stage, which of its stages are done, its objectives' status.
    void WriteQuest(const QuestInfo& q);
    std::string QuestName(uint32_t id);                         // as the Pip-Boy shows it
    std::string ObjectiveText(uint32_t id, uint32_t objective);  // as the Pip-Boy shows it
    // Every quest's script detached (none runs) or put back.
    // (Only quests whose form id the filter accepts.)
    std::vector<std::pair<uint32_t, void*>> DetachQuestScripts(bool (*accept)(uint32_t id, const void* context),
        const void* context);
    void PutBackQuestScripts(const std::vector<std::pair<uint32_t, void*>>& scripts);
    std::vector<std::pair<uint32_t, double>> QuestVariables(uint32_t id);  // its script's variables: id, value
    // Detaches (or puts back) its script: a detached quest's script never runs. Returns the script it had.
    void* SwapQuestScript(uint32_t id, void* script);
    void* QuestScript(uint32_t id);

    // Items lying in the world (step 6.3).
    void ItemsNear(std::vector<TESObjectREFR*>& out, float range);  // like DoorsNear, for anything you can pick up
    // Things physics can move that aren't items (step 8.9, 2026-10-16 test: Goodsprings' crates are containers, its
    // tumbleweeds movable statics): containers and movable statics whose 3D can move (JIP's IsMobile, asked once
    // each), not made while playing (only that game has those).
    void MovablesNear(std::vector<TESObjectREFR*>& out, float range);
    bool Get3DPosition(TESObjectREFR* ref, float& x, float& y, float& z);  // where its 3D is (false: not loaded)
    struct WorldItem
    {
        uint32_t base = 0;
        int count = 1;         // how many in this stack
        uint32_t flags = 0;    // TESForm::flags as they are (lab)
        bool inCell = false;   // it has a parent cell
        float x = 0, y = 0, z = 0;
        float rx = 0, ry = 0, rz = 0;  // its angles, radians, -pi to pi (they follow physics: 2026-10-16 lab)
    };
    WorldItem ReadWorldItem(TESObjectREFR* ref);
    bool IsThere(const WorldItem& item);  // not taken, deleted or disabled
    // What switches a ref on and off: the top of its enable parents (null: none). Switching the ref itself does
    // nothing then (lab 2026-10-19: Sunny's bottles stayed off; their marker switched them).
    TESObjectREFR* TopEnableParent(TESObjectREFR* ref);
    // A new stack of `count` of item `base` lying at x, y, z (null if it couldn't be placed).
    TESObjectREFR* PlaceItem(uint32_t base, int count, float x, float y, float z);
    // Puts it there, turned so (radians; its physics starts afresh from there).
    void MoveItem(TESObjectREFR* ref, float x, float y, float z, float rx, float ry, float rz);
    uint32_t GrabbedItem();  // what our player carries (Z), 0 if nothing
    void RemoveWorldItem(TESObjectREFR* ref);  // disabled; one made while playing is deleted too
    // Hits since the last call that our player landed, that landed on a stand-in, or that another character
    // landed on our player, and how much health each cost. Call every frame.
    struct Hit { uint32_t targetRef; uint32_t attackerRef; float damage; };
    std::vector<Hit> TakeHits();
    // A friend's hit: damage `target`, and make `by` (the friend's character) one of the characters it fights (it
    // starts fighting them if it wasn't fighting; the game picks which of its targets to go for).
    void ApplyHit(TESObjectREFR* target, TESObjectREFR* by, float damage);
    uint32_t CombatTarget(TESObjectREFR* actor);  // who it's going for (0: nobody)
    // Hold who `actor` fights to `target` (null: let go, the game picks again). The game keeps a pointer to `target`:
    // let go before it's removed.
    void ForceTarget(TESObjectREFR* actor, TESObjectREFR* target);
    // A generic enemy: made from a levelled list (not a named character). PlaceExtraEnemy: another of its kind,
    // behind it and going for `target` (Co-op balanced's bigger fights); null if it isn't generic.
    bool IsGeneric(TESObjectREFR* actor);
    TESObjectREFR* PlaceExtraEnemy(TESObjectREFR* like, TESObjectREFR* target);
    int ThinAmmo(TESObjectREFR* body, int keptPercent);  // it keeps that % of each kind of ammo; gives what went
}
