#pragma once

// Every game memory address and game data id the mod uses, in one place (CLAUDE.md: mod compatibility).
// All are for the Steam game version 1.4.0.525.

#include <cstdint>

namespace game::addr
{
    // PlayerCharacter** : the player's character. Same as xNVSE's g_thePlayer.
    constexpr uintptr_t kThePlayer = 0x011DEA3C;
    // NiTPointerMap<TESForm>** : every loaded form by id. xNVSE's LookupFormByID reads this map.
    // (Careful: xNVSE's 0x004F9620 is the editor's address, not the game's.)
    constexpr uintptr_t kFormMap = 0x011C54C0;
    // void (__cdecl*)(void*) : free game memory. Same as xNVSE's FormHeap_Free (its headers need it defined).
    constexpr uintptr_t kFormHeapFree = 0x00401030;
    // void* (__cdecl*)(size_t) : allocate game memory (xNVSE's FormHeap_Allocate).
    constexpr uintptr_t kFormHeapAllocate = 0x00401000;
    // void (__thiscall Actor::*)(bool out) : an actor draws (true) or puts away its weapon. Used by JIP LN
    // (SetWeaponOut), JohnnyGuitar and ShowOff; read in Ghidra (asks its process, offset 0x450, to change).
    constexpr uintptr_t kActorSetWeaponOut = 0x008A6840;
    // int (__thiscall MobileObject::*)() : make an actor jump. Read in Ghidra: gets its character controller
    // (process function 0x28C) and sets the controller's wanted state to 1, jumping (+0x520), and its jump height
    // (+0x540) from the game setting fJumpHeightMin (0x011D0274). Field names: JIP LN havok.h
    // bhkCharacterController, settings_enum.h. Returns the controller, or 0 if it has none (far away).
    constexpr uintptr_t kMobileObjectJump = 0x00930640;
    // Actor function table slot 250 (offset 0x3E8): Reload(TESObjectWEAP*, UInt32 animType, UInt8 extendedClip)
    // (JIP LN GameObjects.h). Read in Ghidra: the Actor, Character and Creature tables (0x01084254, 0x01086A6C,
    // 0x010870AC) all point it at 0x008A8420; with animType 1 it plays the weapon's reload animation (its group
    // from the weapon's reload type) and refills the magazine. JIP's ammo hook skips its call from there
    // (0x008A86B5). Confirmed in game 2026-10-10 (coop reload on a settler with its gun out: type 1 played
    // ReloadL for 1.5 s, starting 0.2-2 s later, and refilled 1 -> 13 rounds; types 2 and 3, or the gun away:
    // nothing).
    constexpr int kActorReloadSlot = 0x3E8 / 4;
    // BSExtraData* (__thiscall BaseExtraList::*)(UInt8 type): a ref's extra data of one type, or null. xNVSE
    // GameBSExtraData.h GetByType; ends "ret 4" (one argument, checked).
    constexpr uintptr_t kExtraListGetByType = 0x00410220;
    // ContainerMenu* while the container screen is open, null otherwise (JIP GameUI.h ContainerMenu::Get).
    // Confirmed in game (coop box, 2026-10-14).
    constexpr uintptr_t kContainerMenu = 0x011D93F8;
    // BarterMenu* while the trading screen is open, null otherwise; +0x80 the merchant (JIP GameUI.h BarterMenu::Get,
    // merchantRef). A merchant's goods and caps are in its merchant container (extra data 0x3C, ExtraMerchantContainer:
    // the container ref at +0x0C; JIP GameExtraData.h, GameObjects.cpp GetMerchantContainer). Confirmed in game (lab
    // coop open, 2026-10-15: trading with Chet, his container 00104C7A, 36 kinds of items and 750 caps).
    constexpr uintptr_t kBarterMenu = 0x011D8FA4;
    // Sleep and wait (step 8.2). SleepWaitMenu* (JIP and Johnny Guitar GameUI.h: 0x11DA920; +0x2C the hours slider
    // tile, +0x44 started, +0x45 sleeping rather than waiting). Its click handler (function table slot at 0x10763B8,
    // Johnny Guitar hooks the same slot; 0x7C0220, thiscall (tile id, tile), ret 8, Ghidra): tile 4, the Wait or
    // Sleep button, reads the hours from the slider (Tile::GetValueFloat 0xA011B0, thiscall (value id), ret 4, with
    // the value id kept at 0x11DA918) and asks the player to wait those hours + 1 (0x5C1A00: player +0x654 hours,
    // +0x658 sleeping); the menu does the waiting (setting those alone didn't, lab coop waittest). 0x7BFC30
    // (cdecl (bool sleep), plain RET) builds and shows the menu, as the ShowSleepWaitMenu command (0x5E0090 ->
    // 0x7054F0) does after checking the player may wait. Tile::SetFloatValue 0xA012D0 (xNVSE GameTiles.h), ret 0xC.
    // The slider counts from 0 (shows hours - 1). Confirmed in game (lab coop sleepwait 3, 2026-10-15: the menu
    // opened, our hook asked, and the game waited 3 hours).
    constexpr uintptr_t kSleepWaitMenu = 0x011DA920;
    constexpr uintptr_t kSleepWaitClickSlot = 0x010763B8;
    constexpr uintptr_t kSleepWaitClick = 0x007C0220;
    constexpr uintptr_t kSleepWaitHoursValue = 0x011DA918;
    constexpr uintptr_t kShowSleepWaitMenu = 0x007BFC30;
    constexpr uintptr_t kTileGetValueFloat = 0x00A011B0;
    constexpr uintptr_t kTileSetValueFloat = 0x00A012D0;
    // The game's message box (xNVSE GameAPI.cpp ShowMessageBox 0x703E80; JIP MSGBOX_ARGS for the arguments). The
    // button pressed: the game's own callback (0x5B4A70, Ghidra) takes it with 0x703FA0 (cdecl: InterfaceManager
    // +0xE4, JIP GameUI.h msgBoxButton, 0 the first; then sets it to 0xFF) and keeps it at 0x118C684, which is stale
    // when our callback replaces the game's (second PC, 2026-10-15: every answer read as no). Confirmed in game
    // (second PC, 2026-10-15: yes and no both ways).
    // Loading a save by name (Ghidra): BGSSaveLoadManager 0x850760, thiscall on the manager (0x11DE134) with (char*
    // save name, then three settings), ret 0x10; the quickload, the load menu and the console's load all come
    // through it, and it calls TESSaveLoadGame::LoadGame (0x847DF0, xNVSE's load hooks are inside that). Starts
    // 55 8B EC 81 EC 2C 02 00 00. Confirmed in game (second PC, 2026-10-15: the laptop's console load of a linked
    // save was held by our hook and the host asked; the host's own loads went through).
    constexpr uintptr_t kLoadSave = 0x00850760;
    // The main menu's options (Ghidra; ShowOff decoding.h StartMenu::GetStartMenuOptionsArray 0x11DAB88): every
    // StartMenu::Option (main and pause menus and their submenus) is in one BSSimpleArray<Option*> (+4 the items,
    // +8 how many). They're made once, the first time the start menu opens (0x7CB7D0 calls 0x7CC6E0, plain ret,
    // when the list is empty); the menu then shows those flagged for it (1: main menu, JIP GameUI.h
    // kOptFlag_MainMenu), in list order. An option (0x10 bytes, from the game's heap): vtable, name, the function
    // called when it's clicked (cdecl, no arguments), flags; made by 0x7CE620 (thiscall: name, function, flags;
    // ret 0xC) and added with 0x7CB2E0 (the list's append, thiscall: Option**; ret 4). Load's function is 0x7D0680
    // (only shown when there are saves). Confirmed in game (2026-10-16: the logged list was Continue, New, Save, Load,
    // ..., Quit, with Load's function 0x7D0680; Host Co-op and Join Co-op showed after Load and worked).
    constexpr uintptr_t kStartMenuOptions = 0x011DAB88;
    constexpr uintptr_t kMakeStartMenuOptionsCall = 0x007CB886;
    constexpr uintptr_t kMakeStartMenuOptions = 0x007CC6E0;
    constexpr uintptr_t kStartMenuOptionInit = 0x007CE620;
    constexpr uintptr_t kSimpleArrayAppend = 0x007CB2E0;
    constexpr uintptr_t kLoadOption = 0x007D0680;
    // The main menu's Exit Game (the "Yes, quit" behind Quit; flags 0x3000 in the logged list): 0x7D0BF0, plain ret,
    // no arguments (Ghidra). Confirmed in game (2026-10-16: called from the main menu, the game closed cleanly).
    constexpr uintptr_t kExitGameOption = 0x007D0BF0;
    // An explosion starting (step 8.8): Explosion's own first virtual, CheckInit3D (JIP GameObjects.h; its class
    // table JIP class_vtbls.h kVtbl_Explosion 0x108EE04), is 0x9ACD60 (fastcall on the explosion, plain ret: Ghidra),
    // in the table at 0x108F108; it builds the explosion's parts the first time. An Explosion (JIP, 0x104 bytes): a
    // reference (its BGSExplosion at +0x20, position at +0x30, cell at +0x40), and at +0xC8 sourceRef, who caused it.
    // Confirmed in game 2026-10-16: a fired frag grenade and missile each logged once, where they went off, by the
    // player (source 0x14); grenades on both PCs showed at the same spots on the other.
    constexpr uintptr_t kExplosionInitSlot = 0x0108F108;
    constexpr uintptr_t kExplosionInit = 0x009ACD60;
    constexpr uint32_t kExplosionSource = 0xC8;
    // Its process (MobileObject::baseProcess, JIP GameObjects.h): null until that first start makes one (Ghidra:
    // 0x9ACD60 makes it when it's null), so a null one means a new explosion (2026-10-16 lab: the game gave a new
    // explosion the id of one gone, and telling them apart by id missed it). Confirmed in game 2026-10-16: three
    // explosions in a row, all given the same id, each seen once and shown on the laptop.
    constexpr uint32_t kExplosionProcess = 0x68;
    // The INI setting bAlwaysActive:General (JIP settings_enum.h kINIS_bAlwaysActive_General 0x11DEED8; a Setting:
    // vtable, value at +4, name at +8, Ghidra). The window's focus handler (0x871C90) counts the game as active
    // whenever it's on; otherwise the game stops when it isn't the active window (playthrough test, 2026-10-16: the
    // host alt-tabbed? 60 s without a frame, and the friend's game gave up on it). Confirmed in game (2026-10-16: was
    // 0, set to 1; 30 s in the background, the frames went on).
    constexpr uintptr_t kAlwaysActiveSetting = 0x011DEED8;
    // GameTimeGlobals (JIP GameData.h, 0x11DE7B8: pointers to the year, month, day, hour, days-passed and timescale
    // globals, then at +0x1C a flag set after a load). The clock's update (0x867A40, which JIP hooks) keeps its own
    // hour and day count and only reads the globals afresh when the flag is set (JIP patches_game.h
    // UpdateTimeGlobalsHook), so setting the globals alone was undone each frame when the hour barely changed
    // (playthrough test, 2026-10-16: the friend's day jumped back a whole day every 5 s). Not confirmed in game yet.
    constexpr uintptr_t kGameTimeGlobals = 0x011DE7B8;
    constexpr uint32_t kGameTimeReload = 0x1C;
    // The message box on screen (JIP GameUI.h MessageMenu::Get, 0x11DA4F0; null when none): its text, a TileText*
    // at +0x34 (+0x2C is another, not shown in ours), then its buttons, a ListBox<int> at +0x40 (+4 its first item; an
    // item starts with its Tile*). A tile's text is set with 0xA01350 (thiscall: value id, text, propagate; ret 0xC;
    // the start menu names its options with it, id 0xFC4: JIP GameTiles.h kTileValue_string). Confirmed in game
    // (2026-10-16: the text and the first button's label changed on screen; the box grows taller for more lines,
    // never wider).
    constexpr uintptr_t kMessageMenu = 0x011DA4F0;
    constexpr uint32_t kMessageMenuText = 0x34;
    constexpr uint32_t kMessageMenuButtons = 0x40;
    constexpr uintptr_t kTileSetString = 0x00A01350;
    // A tile's value by id (JIP GameTiles.h Tile::GetValue 0xA01000, thiscall, ret 4; null if it has none): its text
    // at +0xC (JIP Tile::Value::str). Not confirmed in game yet.
    constexpr uintptr_t kTileGetValue = 0x00A01000;
    constexpr uint32_t kTileValueString = 0xFC4;
    // When the game shows a box (Ghidra): 0x703E80 drops it, returning false, while InterfaceManager's first byte is
    // 0 (0x4B7210 returns InterfaceManager, 0x11D8A80; 0x9373F0 reads that byte); its queue (0x7A8E60) shows it at
    // once only with no box open (0x11DA4F0) and 0x11DA4EC clear, else marks one waiting (0x11DA4EF) for later. A box
    // asked for as another closed was never shown (2026-10-16), so ours wait in our own queue until no box is open.
    constexpr uintptr_t kInterfaceManager = 0x011D8A80;
    // InterfaceManager +0x114: the menus open, by id, 10 slots (xNVSE GameUI.h menuStack; ids kMenuType_...: 1002 the
    // inventory, 1013 the pause menu, ...). Confirmed in game (lab 2026-10-19: the main menu read 1007 and 1013, the save's
    // mode question 1001, gone once answered).
    constexpr uint32_t kMenuStack = 0x114;
    constexpr int kMenuStackSlots = 10;
    constexpr uintptr_t kMessageBoxBusy = 0x011DA4EC;
    constexpr uintptr_t kShowMessageBox = 0x00703E80;
    constexpr uintptr_t kTakeMessageBoxButton = 0x00703FA0;
    // DataHandler* (JIP GameData.h DataHandler::GetSingleton): the game's loaded forms. Not yet confirmed in game
    // (lab: coop quests).
    constexpr uintptr_t kDataHandler = 0x011C3F2C;
    // BGSQuestObjective's class table (JIP internal/class_vtbls.h kVtbl_BGSQuestObjective): a quest's objectives
    // share a list with its script variables, told apart by this (JIP TESQuest::GetObjective). Not yet confirmed.
    constexpr uintptr_t kQuestObjectiveVtbl = 0x01047088;
    // bool (__cdecl*)(UInt32 formType): the game's "can be in an inventory" test (xNVSE GameForms.cpp
    // TESForm::IsInventoryObject, used by AddItem's argument check). Confirmed in game (two-PC container lab,
    // 2026-10-14: stimpaks passed it).
    constexpr uintptr_t kIsInventoryObjectType = 0x00481F30;
    constexpr uintptr_t kActorReload = 0x008A8420;
    // AnimGroupInfo[245], 0x24 bytes each, name (const char*) first: the names of the animation groups, indexed
    // by the low byte of a group id. (JIP LN GameData.h AnimGroupInfo; read in Ghidra: 0 Idle, 3 Forward, ...)
    constexpr uintptr_t kAnimGroupInfos = 0x011977D8;
    constexpr uint32_t kAnimGroupCount = 245;
    constexpr uint32_t kAnimGroupInfoSize = 0x24;
    // ProcessManager : every moving object (actors, projectiles...), grouped by how much the game simulates
    // them. Same address as JIP LN's ProcessManager::Get() and xNVSE's g_actorProcessManager. Layout below from
    // JIP LN GameProcess.h (struct ProcessManager); read in Ghidra: its constructor (0x0096D020) sets JIP's
    // toggle bytes at +0x103A0..., and its object list's setup (0x0096A850) makes 1000 slots and clears the
    // three 4-entry tables after the list.
    constexpr uintptr_t kProcessManager = 0x011E0E80;
    // void (__thiscall TESTopicInfo::*)(UInt32 onEnd, Actor* speaker): runs a dialogue line's result script
    // (its begin or end one) with the speaker as its "self" (JIP GameForms.h RunResultScript). Read in Ghidra: ends
    // "ret 8"; starts 55 8B EC 83 EC 0C (push ebp / mov ebp,esp / sub esp,0xC: 6 bytes, moved as they are to our
    // hook's way back). JIP's dialogue event hooks inside it (0x61F184) and Johnny Guitar replaces the call at
    // 0x61F18B; our hook (step 7.4) is at its start, so all chain.
    constexpr uintptr_t kTopicInfoRunResultScript = 0x0061F170;
    // Script* (__thiscall TESTopicInfo::*)(UInt32 onEnd): a dialogue line's result script, or null (JIP
    // GameForms.h GetResultScript). Read in Ghidra: ends "ret 4"; loads it from the game's files, keeping the last.
    constexpr uintptr_t kTopicInfoGetResultScript = 0x0061EB60;
    // DialogMenu* while a conversation's screen is open (JIP GameUI.h DialogMenu::Get); +0x80 who it's with
    // (partnerRef). Confirmed in game (lab, 2026-10-15: Doc Mitchell, the speaker of his greeting).
    constexpr uintptr_t kDialogMenu = 0x011D9510;
    // bool (__cdecl*)(const char* text, BSSoundHandle sound (12 bytes, by value), NiPoint3 pos (by value),
    // TESObjectREFR* speaker, bool instant): Interface::ShowText, a subtitle on the HUD (the one NPCs talking
    // near you get). Johnny Guitar JohnnyEvents.cpp (its call to HUDMainMenu::AppendSubtitleData, 0x7052B8, and
    // that function's arguments); read in Ghidra: plain "ret", arguments as above, the sound handle copied
    // (0x418900) and destroyed (0x483710). Not yet confirmed in game.
    constexpr uintptr_t kShowSubtitle = 0x00705210;
    // An NPC saying one exact line elsewhere (step 7.4: the talker's line, said by our copy of the NPC with its
    // voice, mouth and subtitle), the way the game's Say command has an actor say a topic (read in Ghidra, 0x5C8E1F):
    // its AI process (Actor +0x68) function table slot 0x2A4 (0x8DBE30; thiscall: actor, topic, byte, 0, 1, 1;
    // "ret 0x18") picks the line and starts saying it at once. That way it is the NPC's own speech, which other
    // plugins see (lab 2026-10-15: Floating Subtitles shows it; it showed nothing for the voice functions called
    // ourselves). To make it our line: the picker 0x61A790 is called at 0x61B361 (in TESTopic::CreateDialogueItem;
    // thiscall, 6 arguments, the speaker second; "ret 0x18"; returns the line) and that call is hooked; and every
    // way the game says a line starts with DialogueItem::FirstResponse (0x83C7B0, thiscall, plain "ret"; starts 55
    // 8B EC 51 89 4D FC, 7 bytes moved as they are to our hook's way back), hooked at its start to go on to our
    // response with NextResponse (0x83C7E0) and GetCurrentItem (0x83C820) (Johnny Guitar DialogueItem.cpp;
    // DialogueItem +0x0C its TESTopicInfo, DialogueItem.hpp). All confirmed in game (lab coop sayagain, 2026-10-15:
    // Chet said his greeting's second response, floating above him with Floating Subtitles).
    constexpr int kProcessSaySlot = 0x2A4 / 4;
    constexpr uintptr_t kPickLineCall = 0x0061B361;
    // The Pip-Boy's radio: on/off 0x8324E0 (cdecl: bool; plain "ret"; lab use) and tune to a station 0x832240
    // (cdecl: station ref, bool; plain "ret"; it clears the song it played, so the Pip-Boy starts the station's song
    // again from the station's start time: the music player call at 0x83398D passes station +0x0C), whether it's
    // on: the byte at 0x11DD434, the station it's tuned to (an FORadioStation*) at 0x11DD42C (ShowOff
    // StewieMagic.cpp, JIP jip_core.h; rets and the music call from Ghidra, 2026-10-17; lab 2026-10-17: the laptop
    // tuned in again caught up 11.9 s into the host's line).
    constexpr uintptr_t kRadioSetOn = 0x008324E0;
    constexpr uintptr_t kRadioSetStation = 0x00832240;
    constexpr uintptr_t kRadioOn = 0x011DD434;
    constexpr uintptr_t kPipboyRadioStation = 0x011DD42C;
    // The radio stations playing: a BSSimpleList of FORadioStation* at 0x11DD554 (Johnny Guitar FalloutRadio.hpp:
    // station +0x04 its Conversation (a list of DialogueItems, +0x08 the current one; a DialogueItem has its
    // responses, +0x08 the current one, +0x0C its TESTopicInfo and +0x10 its topic: Conversation.hpp,
    // DialogueItem.hpp), +0x0C when its part started, +0x10 its length, +0x1C its listeners, each with a
    // BSSoundHandle at +4; the game's "FalloutRadio::UpdateStation" 0x834260 goes on when start + length is past,
    // to the run's next line, or builds a new run after its last; read in Ghidra 2026-10-17, confirmed in game the
    // same day: lines read as they played). BSSoundHandle::Seek 0xAD8F70 (thiscall, ms; "ret 4") (Johnny Guitar
    // BSSoundHandle.cpp; ret from Ghidra).
    constexpr uintptr_t kActiveRadioStations = 0x011DD554;
    // An actor's BSFaceGenAnimationData (thiscall on the actor, no arguments: its process's GetFaceGenAnimData,
    // vtable +0x144; JIP GameProcess.h; 0x8ADCB0 read in Ghidra 2026-10-17), or null. Its constructor (0x649680)
    // makes BSFaceGenKeyframeMultiple groups (0x14 bytes: +0x04 kind, +0x0C float* values, +0x10 count): 15
    // expressions at +0x10, +0x24, +0x74; 17 modifiers at +0x38, +0x94, +0xA8; 16 phonemes at +0x4C, +0xC8, +0xDC
    // (the game's phoneme names, 0x119B4A0 on: Aah, BigAah, ...); 1 custom at +0x60, +0xFC, +0x110. Confirmed in game
    // (lab 2026-10-17: BigAah written in the three phoneme groups every frame opened a friend's character's mouth).
    constexpr uintptr_t kActorFaceAnimData = 0x008ADCB0;
    constexpr uintptr_t kSoundHandleSeek = 0x00AD8F70;
    constexpr uintptr_t kDialogueItemFirstResponse = 0x0083C7B0;
    constexpr uintptr_t kDialogueItemNextResponse = 0x0083C7E0;
    constexpr uintptr_t kDialogueItemCurrentResponse = 0x0083C820;
    // BSSoundHandle 0xAD8E60 (thiscall: function, argument; "ret 8"; starts 55 8B EC 83 EC 0C) has a function called
    // when a sound ends: an NPC's speech gives it 0x936A20 (from 0x8DC480) or 0x5790F0 (the Say command) with the
    // speaker's ref id. Hooked at its start, so the voice an NPC says a line with is kept, to stop it (lab
    // 2026-10-15: Chet's voice stopped).
    constexpr uintptr_t kSoundHandleSetEndCallback = 0x00AD8E60;
    constexpr uintptr_t kSpeechEnded = 0x00936A20;
    constexpr uintptr_t kSayEnded = 0x005790F0;
    // BSSoundHandle::Stop / IsPlaying (thiscall, plain "ret": Johnny Guitar BSSoundHandle.cpp, read in Ghidra).
    constexpr uintptr_t kSoundHandleStop = 0x00AD88F0;
    constexpr uintptr_t kSoundHandleIsPlaying = 0x00AD8930;
    // HUDMainMenu* (the one ShowText checks before adding a subtitle, read in Ghidra at 0x705272). Its subtitles
    // (JIP GameUI.h HUDMainMenu): +0x198 BSSimpleArray (data +0x04, size +0x08) of SubtitleData*, +0x1A8 the one
    // shown. SubtitleData (0x11C bytes, read in Ghidra: HUDMainMenu::AppendSubtitleData 0x774FD0 fills it): text
    // char[0x104], +0x104 start time, +0x108 how long it shows (float, seconds). Not yet confirmed in game.
    constexpr uintptr_t kHudMainMenu = 0x011D96C0;
    // Settings (JIP settings_enum.h; Setting: name at +0x08, value at +0x04, JIP GameSettings.h): the game's
    // "general subtitles" (NPCs talking near you) and "dialogue subtitles" (conversations) options. ShowText shows
    // nothing while general subtitles are off (lab, 2026-10-15: on, a test subtitle showed; off, none).
    constexpr uintptr_t kSettingGeneralSubtitles = 0x011D8928;
    constexpr uintptr_t kSettingDialogueSubtitles = 0x011D8934;
    // "uExterior Cell Buffer:General" (JIP settings_enum.h kINIS_uExterior_Cell_Buffer_General; its name is checked
    // before its value is used): how many outdoor cells the game keeps room for. It must be at least
    // (uGridsToLoad + 1)^2 (36 for the default 5), or a raised uGridsToLoad can crash the game. Confirmed in game
    // (range lab, 2026-10-17: its name read back, value 36, and 64 with Fallout.ini set to 64).
    constexpr uintptr_t kSettingExteriorCellBuffer = 0x011C3C90;
    // How an NPC looks (players' faces; JIP and xNVSE GameForms.h agree on TESNPC): +0x34 its flags (bit 0 female;
    // changed through 0x47DD50, TESActorBaseData's flag setter, thiscall on +0x30 with (1, on, 1), ret 0xC: JIP's
    // TESNPC::SetSex), +0x110 its race (JIP's SetRace writes it there for anyone but the player), three FaceGenData
    // at +0x134 (0x20 each: the values at +0x0C, how many at +0x18: 50, 30, 50; JIP copies them so), +0x1B8 hair,
    // +0x1BC hair length (float), +0x1C0 eyes, +0x1D8 hair colour (RGBA), +0x1DC head parts (tList: data, next).
    // 0x8D3FA0 remakes a character's head (thiscall on it, no arguments, ret: Rets.java), with 0x11D5AE0
    // (bLoadFaceGenHeadEGTFiles) on meanwhile (JIP's CopyFaceGenFrom; Johnny Guitar puts it back after). Confirmed in
    // game (2026-10-19, two PCs: the laptop's player's face, hair and head part on its character here; after the
    // console's SexChange there, a female head, and with Update3D a female body).
    constexpr uintptr_t kSetActorBaseFlag = 0x0047DD50;
    // TESRace +0x70 its flags: 0x01 playable, 0x04 a child's (JIP and xNVSE GameForms.h). Another player's race is
    // only taken if playable and not a child's (a message could name any race).
    constexpr uint32_t kRaceFlags = 0x70;
    constexpr uint32_t kRacePlayable = 0x01;
    constexpr uint32_t kRaceChild = 0x04;
    constexpr uintptr_t kRemakeHead = 0x008D3FA0;
    constexpr uintptr_t kLoadFaceGenEgt = 0x011D5AE0;
    // The game's time speed (Ghidra): TimeGlobal::SetGlobalTimeMultiplier 0xAA4DB0, thiscall (float speed, bool now)
    // on 0x11F6394, ret 8 (Rets.java). Real Time Menus overrides some of its calls (its Hook_RestoreTimeMultiplier
    // list: 0x9C6F3A, 0x9C7F27, 0x9C83A0, 0x9C8579, 0x9C8A82). These five, in VATS playback (0x9C7240), it leaves:
    // the camera shot's own speed (the kill slow motion: camera shot +0x50, read by 0x9D1B30) at 0x9C7EC6 and
    // 0x9C8565, 0.3 at 0x9C7EA9, 0.0001 at 0x9C7E7D and 0x9C852D. Not yet confirmed in game.
    constexpr uintptr_t kSetTimeMultiplier = 0x00AA4DB0;
    // The Escape menu keeps the world running in co-op (Alex, October 2026: the host's pause froze everyone's NPCs).
    // From Real Time Menus' source (GPL: facts only; its Interface.cpp, StartMenu.cpp, RealTimeMenus.cpp): 0x702360
    // InterfaceManager's "in menu mode" (cdecl, no arguments, bool), 0x4A4040 "the pause menu is up" (same), and the
    // calls to the first that decide whether the world pauses, each a 5-byte call (E8) Real Time Menus itself takes
    // over: TESMain::OnIdle 0x86E79E and 0x86EEAF, Actor::Update 0x8898BD, BSShaderManager::CalculateTime 0x86A4A1
    // and 0x86A4DE, SkySound::Play 0x63E0F8, ImpactMixer footsteps 0x837D05 and jumps 0x8387C5, Actor::HitMe
    // 0x89C6CA, 0x89C74A, 0x89CEBA, 0x89CF56, Actor::TriggerPain 0x8A7EA9, SetExplosionCameraShake 0x8D1AA1.
    // Confirmed in game (2026-10-19: all 14 calls, Real Time Menus' by then; with the Escape menu up in co-op, Powder
    // Gangers kept fighting the host for 12 s).
    constexpr uintptr_t kInMenuMode = 0x00702360;
    constexpr uintptr_t kPauseMenuUp = 0x004A4040;
    constexpr uintptr_t kMenuModeCalls[] = {0x0086E79E, 0x0086EEAF, 0x008898BD, 0x0086A4A1, 0x0086A4DE, 0x0063E0F8,
        0x00837D05, 0x008387C5, 0x0089C6CA, 0x0089C74A, 0x0089CEBA, 0x0089CF56, 0x008A7EA9, 0x008D1AA1};
    // What it's called on (every call above passes it). Calling it ourselves confirmed in game (lab, 2026-10-15: the
    // loading freeze set GetGlobalTimeMultiplier to 0.0001, then back to 1).
    constexpr uintptr_t kTimeGlobal = 0x011F6394;
    constexpr uintptr_t kVatsTimeCalls[] = {0x009C7E7D, 0x009C7EA9, 0x009C7EC6, 0x009C852D, 0x009C8565};
    // Mines (Alex, 2026-10-19 session: a friend's puppet set off the host's copy of a mine the friend was disarming).
    // Projectile::GetMineReactsToTarget(target, shooter, ownerFaction), thiscall (JohnnyGuitar Projectile.cpp calls
    // it at this address; Ghidra: RET 0xC). True: that actor sets the mine off (none with no shooter or owner).
    constexpr uintptr_t kMineReactsToTarget = 0x009C3930;
    // Its call in the mines' check over the actors near them (Ghidra FUN_009C39E0, the loop over the process list;
    // the player is looked at separately, at 0x009C3C6F, which we leave alone). E8 rel32. Confirmed in game (lab
    // 2026-10-19, the four Powder Gangers' mines Cappy hit: each player stepping onto one set off only their own
    // game's; the other game logged its copy "not set off by a friend's character").
    constexpr uintptr_t kMineReactsCall = 0x009C3BD1;
    // A projectile's turning off (a mine disarmed): thiscall(projectile, actor, bool quietly), RET 0x8 (Ghidra; it's in
    // the projectiles' vtables). Returns at once unless the base can be turned off (0x004FDF20, JohnnyGuitar
    // BGSProjectile::GetCanTurnOff) and it isn't off yet (flag kProjTurnedOff); then stops its light and sets the
    // flag. Not quietly: also the disarm sound, the XP and the "mines disarmed" stat (for `actor`). Confirmed in game
    // (lab 2026-10-19: the laptop's disarmed mine 001743AF read disarmed on the host after this).
    constexpr uintptr_t kProjectileTurnOff = 0x009C43E0;
}

// ActorMover: each actor's "legs" (Actor::actorMover, Actor +0x190). It holds the movement flags (coop::moves)
// that pick the walk, run and sneak animations. Layout from JIP LN's GameObjects.h (class ActorMover), and
// every entry below read in Ghidra. Before calling through a mover's function table, check its table is one of
// these two (mover.cpp), so a wrong guess can never call into random code.
namespace game::mover
{
    // Function tables (found by class name in the game's type info).
    constexpr uintptr_t kActorMoverTable = 0x0109223C;   // NPCs and creatures
    constexpr uintptr_t kPlayerMoverTable = 0x0109297C;  // the player (PlayerMover, a bigger ActorMover)
    // Slot 3: SetMovementFlags(UInt32). For an NPC (0x009DE060) it only hands the flags over: stores them at
    // kAskedFlags and sets kAskedOn, and from then on the mover's update (0x009DDC00) uses them instead of
    // its path (until slot 4). For the player (0x009EA3E0) it sets kPlayerFlags directly.
    constexpr int kSetFlagsSlot = 3;
    constexpr uintptr_t kActorSetFlags = 0x009DE060;
    constexpr uintptr_t kPlayerSetFlags = 0x009EA3E0;  // (also how xNVSE forces the player out of sneaking)
    // Slot 4 (0x009DE080, both tables): undoes slot 3 (clears kAskedOn and kAskedFlags): back to the path.
    constexpr int kReleaseFlagsSlot = 4;
    constexpr uintptr_t kReleaseFlags = 0x009DE080;
    // Slot 8: GetMovementFlags(). NPC (0x009DDB70): kFlags (unless byte +0x70 is set: then it works them
    // out). Player (0x009185E0): kPlayerFlags.
    // Slot 14 (0x009DFB80, NPC table entry at 0x01092274): resets the legs: clears kAskedOn, kAskedFlags and
    // kFlags, among others (read in Ghidra). It also takes one argument on the stack (ends with "ret 4"; the
    // decompiler hid it): chaining it as taking only the mover unbalanced the stack on every call, which happen
    // while a save loads (the host's game stopped with a run-time check failure, 2026-10-10).
    constexpr int kResetSlot = 14;
    constexpr uintptr_t kActorReset = 0x009DFB80;
    // Puppets' legs are held (mover.cpp): we chain onto slots 4 and 14 in the NPC table and hand a puppet its
    // flags straight back. A real NPC's AI releases them every frame when its routine wants it elsewhere (step 5
    // lab, Oct 2026: Chet outside his shop, 144 of 144 frames).
    constexpr int kGetFlagsSlot = 8;
    constexpr uintptr_t kActorGetFlags = 0x009DDB70;
    constexpr uintptr_t kPlayerGetFlags = 0x009185E0;
    // Fields (byte offsets).
    constexpr uint32_t kActor = 0x28;       // Actor* it belongs to
    constexpr uint32_t kAskedFlags = 0x38;  // flags handed over with slot 3 (JIP: unk38)
    constexpr uint32_t kFlags = 0x3C;       // the flags it's moving with now (JIP: movementFlags2)
    constexpr uint32_t kAskedOn = 0x71;     // byte: 1 while slot 3's flags are in use (JIP: byte71)
    constexpr uint32_t kPlayerFlags = 0x94; // PlayerMover only (JIP: pcMovementFlags)
}

// The AI's state for an actor (Actor::baseProcess, Actor +0x68). From JIP LN's GameProcess.h, read in Ghidra.
namespace game::process
{
    // Function table offset of GetAnimData(): AnimData* (HighProcess 0x00602150 returns its +0x1C0; LowProcess
    // 0x00ACBB70 returns null: far-away actors aren't animated).
    constexpr uint32_t kGetAnimDataOffset = 0x1B8;
    constexpr uint32_t kLevel = 0x28;  // 0 high (nearby, fully simulated), 1 middle-high, 2 middle-low, 3 low
    // AnimData fields.
    constexpr uint32_t kAnimActor = 0x04;     // Actor* it belongs to
    constexpr uint32_t kAnimGroupIds = 0x4C;  // UInt16[8]: group playing in each animation layer
    constexpr int kAnimLayers = 8;
    // ProcessManager fields (see addr::kProcessManager).
    constexpr uint32_t kManagerObjects = 0x08;      // MobileObject** : the list (NiTArray +0x04)
    constexpr uint32_t kManagerCapacity = 0x0C;     // UInt16: slots it has room for (NiTArray capacity, +0x08)
    constexpr uint32_t kManagerObjectCount = 0x0E;  // UInt16: slots in use (NiTArray firstFreeEntry, +0x0A)
    constexpr uint32_t kManagerBegin = 0x14;        // UInt32[4]: where each level starts in the list (0 high)
    constexpr uint32_t kManagerEnd = 0x24;          // UInt32[4]: and where it ends
}

// The game's own script commands that open a screen (opcodes: Johnny Guitar CommandOpcodes.h, the game's command
// table). A friend's conversation line using one isn't run in the host's game (it would open there: step 7.4).
namespace game::opcodes
{
    constexpr uint16_t kOpensScreen[] = {
        0x1056,  // StartConversation
        0x10D9,  // ShowClassMenu
        0x10DA,  // ShowRaceMenu
        0x10DC,  // ShowNameMenu
        0x1173,  // ShowBarterMenu
        0x1179,  // ShowSurgeryMenu
        0x117A,  // ShowRepairMenu
        0x1197,  // ShowChargenMenu
        0x11C8,  // OpenTeammateContainer
        0x11D3,  // ShowLockpickMenu
        0x11E3,  // ShowTutorialMenu
        0x11E8,  // ShowSPECIALBookMenu
        0x11F1,  // ShowBarberMenu
        0x11F2,  // ShowPlasticSurgeonMenu
        0x1244,  // ShowLoveTesterMenu
        0x1247,  // ShowTraitSelectMenu
        0x1250,  // ShowRecipeMenu
        0x1251,  // ShowSlotMachineMenu
        0x1253,  // ShowBlackJackMenu
        0x1255,  // ShowRouletteMenu
        0x125A,  // ShowCaravanMenu
        0x1268,  // ShowTraitMenu
        0x1279,  // ShowSleepWaitMenu
    };
    // A compiled script is a run of statements: u16 opcode, u16 length, then that many bytes; a command called on
    // a reference has 0x1C and a u16 reference index before it. Confirmed in game (lab coop screenlines,
    // 2026-10-15: all 7053 dialogue scripts in FalloutNV.esm read so, end to end; 144 open a screen, e.g. "I'd
    // like to purchase something." ShowBarterMenu).
    constexpr uint16_t kOnReference = 0x1C;
}

// Game data form ids, all from FalloutNV.esm (always load order slot 00, so the ids are fixed).
// Editor ids (names) can't be used: the running game doesn't keep most of them.
namespace game::ids
{
    // The game's own "follow the player" package with no conditions (FalloutNV.esm PACK DefaultFollowPlayerClose,
    // found with the data detective, 2026-10-16): a copy aimed at a friend's character makes a companion follow
    // them (DESIGN: companions are the party's). Confirmed in game 2026-10-16: Veronica, given a copy aimed at the
    // laptop's character, walked over and followed it, through a door too.
    constexpr uint32_t kDefaultFollowPlayerClose = 0x00119230;
    // Quick start (DESIGN build order 15): FalloutNV.esm's tutorial in Doc Mitchell's house (found with the data
    // detective, 2026-10-17). VCG00 the intro (Benny): its last stage starts VCG01 from stage 0, which stops VCG00;
    // VCG01 runs (stage 0) before that, so it's only waking up once VCG00 has stopped (2026-10-17: skipped while the
    // intro ran, the intro started it again outside). VCG01 the tutorial quest (stages 0 waking up to 200 done: it stops itself, starts the
    // main quest VMQ01 and VCG04, the "finish your character" box shown on leaving Goodsprings, which its activator
    // VCG04ActivatorRef shows when activated where the player is, and which stops itself when done); VCG02 Sunny
    // Smiles' tutorial (Doc's farewell sets its stage 5 with VCG01's 200). Doc's farewell gives the Pip-Boy and its
    // glove, Vault 21 jumpsuit, the Courier's note, and (by tag skills) the starting gear; it un-breaks the house's door
    // (GSDocMitchellHouseIntDoorREF) and asks about Hardcore mode.
    constexpr uint32_t kBennyIntroQuest = 0x00102037, kTutorialQuest = 0x00104C1C, kSunnyQuest = 0x0010A214, kReviseQuest = 0x0011649E;
    constexpr uint32_t kDocsDoor = 0x00103E61, kReviseActivator = 0x001164A3;
    constexpr uint32_t kPipBoy = 0x00015038, kPipBoyGlove = 0x00025B83, kVaultSuit21 = 0x00104184,
        kCourierNote = 0x00105BCD;
    // (+ NVEyeAdjustISFX 0x0011E6E1, the eyes getting used to daylight on the way out: data detective, and JIP's
    // GetActiveIMODs showed it alone left over the "finish your character" box, its screen white, 2026-10-19.)
    constexpr uint32_t kTutorialFades[] = {0x00120E95, 0x00120E96, 0x0016A8A2, 0x0016A8A3, 0x000AC03C, 0x0011E6E1};
    // The DLCs' intro popups (data detective, 2026-10-17): each DLC's start script (NVDLC0xMQ00) shows two boxes, a
    // radio broadcast found and "loaded, level cap raised" (ids in the DLC's own range: its place in the load order
    // goes on top); each Courier's Stash pack's (VPreorder...SCRIPT, in FalloutNV.esm's range) one box and its items
    // with an "added" note each.
    struct DlcPopups { const char* file; uint32_t broadcast, loaded; };
    constexpr DlcPopups kDlcPopups[] = {{"DeadMoney.esm", 0x0062B2, 0x00F41E}, {"HonestHearts.esm", 0x008A33, 0x008A34},
        {"OldWorldBlues.esm", 0x002FD3, 0x002FD1}, {"LonesomeRoad.esm", 0x003605, 0x003606},
        {"GunRunnersArsenal.esm", 0x00093E, 0x00093D}};
    constexpr uint32_t kPackPopups[] = {0x00174777, 0x00174776, 0x001674D2, 0x00174775};
    constexpr uint32_t kPackScripts[] = {0x00172A27, 0x00172A24, 0x00172A26, 0x00172A25};
    constexpr uint32_t kCaps = 0x0000000F, kLockpick = 0x0000000A,
        k9mmPistol = 0x000E3982, k9mmAmmo = 0x00096BDF, kLaserPistol = 0x000301D3, kEnergyCells = 0x00029377,
        kDynamite = 0x000E39AD, kStraightRazor = 0x000F52B5, kBoxingGloves = 0x00107234;
    constexpr uint32_t kPlayerRef = 0x00000014;
    // Player: the player's base form (TESNPC); made essential while in co-op so 0 health downs instead of kills.
    constexpr uint32_t kPlayerBase = 0x00000007;
    // VStreetNCRTravelerCMe: generic non-unique "Traveler" with no script. Each friend's stand-in gets its own copy.
    constexpr uint32_t kStandInBase = 0x00120EB1;
    // VCG01: tutorial quest "Ain't That a Kick in the Head" (completed on leaving Doc Mitchell's).
    constexpr uint32_t kIntroQuest = 0x00104C1C;
    // PlayerFaction: stand-ins join it so the player's companions treat them as friends.
    constexpr uint32_t kPlayerFaction = 0x0001B2A4;
    // GSDocMitchellHouse: the interior cell.
    constexpr uint32_t kDocMitchellHouse = 0x00103DF9;
    // LvlGoodspringsPowderGanger: a levelled NPC (step 5.4 labs; read from FalloutNV.esm).
    constexpr uint32_t kLvlPowderGanger = 0x0015F315;
    // Stimpak (ALCH, read from FalloutNV.esm): the container lab's test item.
    constexpr uint32_t kStimpak = 0x00015169;
    constexpr uint32_t kSuperStimpak = 0x000CCEF2;
    constexpr uint32_t k9mmPistolWeapon = 0x000E3778;  // (WEAP WeapNV9mmPistol: the item kinds lab's gun)  // (ALCH NVSuperStimpak, read from FalloutNV.esm)
    // The game's clock and calendar (TESGlobal, FalloutNV.esm): GameYear, GameMonth (0-11), GameDay, GameHour,
    // GameDaysPassed. Confirmed in game (2026-10-15: read by id, they matched the values read by name).
    constexpr uint32_t kGameYear = 0x00000035;
    constexpr uint32_t kGameMonth = 0x00000036;
    constexpr uint32_t kGameDay = 0x00000037;
    constexpr uint32_t kGameHour = 0x00000038;
    constexpr uint32_t kGameDaysPassed = 0x00000039;
}

namespace game::layout
{
    // ExtraLeveledCreature (extra data type 0x2E, xNVSE GameExtraData.h): on a levelled actor, whose base form is
    // then made while playing (FF......). +0x0C: the base it came from (JIP's GetBaseForm, xNVSE's
    // GetPermanentBaseForm); +0x10: JIP's GetBaseForm2 (JIP GameExtraData.h names it actorBase). Not yet
    // confirmed in game (lab: coop npcs).
    constexpr uint8_t kExtraLeveledCreature = 0x2E;
    constexpr uint32_t kLeveledBaseForm = 0x0C;
    constexpr uint32_t kLeveledActorBase = 0x10;
    // ExtraLock (extra data type 0x2A, JIP GameExtraData.h): +0x0C points at its data: lock level (UInt8) at +0,
    // flags at +8 (bit 1: locked). ExtraTeleport (0x2B): a door that leads to another place. Confirmed in game
    // (coop doors, 2026-10-14: Doc Mitchell's front door leads away; Lock 50 / Unlock read back as set).
    constexpr uint8_t kExtraLock = 0x2A;
    constexpr uint8_t kExtraTeleport = 0x2B;
    constexpr uint8_t kExtraMerchantContainer = 0x3C;
    constexpr uint32_t kMerchantContainerRef = 0x0C;
    constexpr uint32_t kLockData = 0x0C;
    constexpr uint32_t kLockLevel = 0x00;
    constexpr uint32_t kLockFlags = 0x08;
    constexpr uint8_t kLockLocked = 0x01;
    // Outdoors (JIP GameData.h / GameForms.h): TES singleton pointer at 0x011DEA10 (TES::GetSingleton); TES +0x08:
    // GridCellArray* (the loaded cells around the player), whose +0x0C is gridSize (uGridsToLoad) and +0x10 the
    // gridSize*gridSize TESObjectCELL* array (entries can be null). TESObjectCELL +0xC0: its TESWorldSpace (null
    // indoors); TESWorldSpace +0x34: the worldspace's persistent cell (doors leading indoors live there, not in
    // the grid cells). Confirmed in game (coop doors in Goodsprings, 2026-10-14: 39 doors, persistent ones listed twice
    // until kRefPersistent was used; range lab, 2026-10-17: gridSize 5, and 7 with uGridsToLoad=7).
    constexpr uintptr_t kTesSingleton = 0x011DEA10;
    constexpr uint32_t kTesGridCells = 0x08;
    constexpr uint32_t kGridSize = 0x0C;
    constexpr uint32_t kGridCellArray = 0x10;
    constexpr uint32_t kCellWorldSpace = 0xC0;
    constexpr uint32_t kWorldPersistentCell = 0x34;
    // Co-op balanced: armor and loot (JIP and xNVSE GameForms.h agree). TESObjectARMO: +0x70 TESBipedModelForm, whose
    // +0x08 bipedFlags hold the weight class (0x08 medium, 0x80 heavy: JIP's SetArmorClass), +0x178 armorRating (DR,
    // UInt16). AlchemyItem +0xBC alchFlags (the ESM's ENIT flags: 0x02 food, 0x04 medicine). TESLevItem +0x30
    // TESLeveledList: +0x34 its entries (tList: node data +0, next +4; an entry's form at +0), +0x3C chanceNone (%).
    // Confirmed in game (2026-10-19: DR read as xNVSE's GetArmorAR on 20 of 20; Combat Armor read medium and T-51b
    // heavy, then GetArmorAR 10 and 20; LootChemsStimpak50's GetChanceNone 50 to 75, and back).
    constexpr uint8_t kFormTypeArmor = 0x18;
    constexpr uint8_t kFormTypeAlchemy = 0x2F;
    constexpr uint8_t kFormTypeLevItem = 0x34;
    constexpr uint32_t kArmorBipedFlags = 0x78;
    constexpr uint32_t kArmorMedium = 0x08;
    constexpr uint32_t kArmorHeavy = 0x80;
    constexpr uint32_t kArmorRating = 0x178;
    constexpr uint32_t kAlchFlags = 0xBC;
    constexpr uint8_t kAlchFood = 0x02;
    constexpr uint8_t kAlchMedicine = 0x04;
    constexpr uint32_t kLevListEntries = 0x34;
    constexpr uint32_t kLevListChanceNone = 0x3C;
    // Item lists (JIP and xNVSE GameForms.h agree): TESContainer +0x04 its tList of FormCount (count +0, form +4), at
    // +0x30 in a TESObjectCONT (form type 0x1B) and +0x64 in a TESNPC (0x2A) or TESCreature (0x2B). Confirmed in game
    // (2026-10-19: 1967 loot lists found from boxes, 1210 from characters, close to the data's 1409 and 980 without the
    // DLCs; VendorAmmo4570, box-only in the data, set from 0 to 50 and back).
    constexpr uint8_t kFormTypeContainer = 0x1B;
    constexpr uint8_t kFormTypeNpc = 0x2A;
    constexpr uint8_t kFormTypeCreature = 0x2B;
    constexpr uint32_t kContainerItemsOfBox = 0x34;
    constexpr uint32_t kContainerItemsOfActor = 0x68;
    // TESCombatStyle (form type 0x4A; JIP and xNVSE GameForms.h agree, and JIP's Get/SetCombatStyleTraitNumeric read
    // traits 0, 1, 15, 16 there): +0x18 cover search radius, +0x1C take cover chance (%, float), +0x58 dodge chance,
    // +0x59 left/right (side step) chance (%, UInt8), +0xA8 flags (UInt16; 0x04 flee based on personal survival).
    // Confirmed in game (2026-10-19: CSRaider read 1024, 50, 50, flee 0 by JIP; ours set 1536, 75, 75, 1; back).
    constexpr uint8_t kFormTypeCombatStyle = 0x4A;
    constexpr uint32_t kStyleCoverRadius = 0x18;
    constexpr uint32_t kStyleTakeCover = 0x1C;
    constexpr uint32_t kStyleSideStep = 0x59;
    constexpr uint32_t kStyleFlags = 0xA8;
    constexpr uint16_t kStyleFleeWhenHurt = 0x04;
    // An AI package's target (xNVSE GameForms.h TESPackage +0x30: a TargetData, its type at +0, 0 a reference, and
    // the reference at +4). Lab only (companions: is a follow package's copy aimed at a friend's character?).
    constexpr uint32_t kPackageTarget = 0x30;
    // An idle animation's list of children (JIP GameForms.h TESIdleForm +0x40, a BSSimpleArray*; JIP's note: for
    // some idles the game fills it with every idle in its path). JIP's PlayIdleEx then picks one through
    // TESIdleForm::FindIdle, which walks that list as children and crashed on an empty entry: a friend's game,
    // 2026-10-16, reading 0x34 (a null idle's conditions at +0x30) in the game's condition check 0x680C60, called from
    // JIP's FindIdle (Ghidra on jip_nvse.dll). Ours never hands PlayIdleEx an idle with that list.
    constexpr uint32_t kIdleChildren = 0x40;
    // A 3D node's place in the world (JIP netimmerse.h NiAVObject: m_transformWorld at +0x68, an NiTransform: a 3x3
    // rotation, row by row, then the position at +0x8C). Confirmed in game 2026-10-16 (coop items): an ammo stack
    // blown away by an explosion, its 3D and its reference's own position and angles agreeing before and after (so
    // reading the reference is enough: step 8.9).
    constexpr uint32_t kNodeWorldRotate = 0x68;
    constexpr uint32_t kNodeWorldTranslate = 0x8C;
    // TESForm::flags bit on a persistent ref (JIP GameObjects.h kFlags_Persistent). Outdoors these are listed in the
    // grid cells too, so only the persistent cell's copy counts (seen: coop doors listed them twice).
    constexpr uint32_t kRefPersistent = 0x400;
    // TESForm::flags bits on a ref: 0x20 deleted (JIP GameObjects.h kFlags_Deleted; with 0x02 "taken": seen on a
    // picked-up stimpak, coop itempick 2026-10-15, flags 08 -> 2A) and 0x800 disabled (JIP GetDisabled tests it).
    constexpr uint32_t kRefDeleted = 0x20;
    constexpr uint32_t kRefDisabled = 0x800;
    // ExtraEnableStateParent (extra data type 0x37, xNVSE GameExtraData.h): what switches a ref on and off; its ref at
    // +0x0C (JIP GameExtraData.h parentRef; flags at +0x10, 1: the opposite of it).
    constexpr uint8_t kExtraEnableStateParent = 0x37;
    constexpr uint32_t kEnableParentRef = 0x0C;
    // Projectile (a placed mine's ref) +0xC8: its flags (JIP GameObjects.h projFlags); 0x200 turned off (disarmed:
    // JohnnyGuitar Projectile.hpp bTurnedOff, bit 9; JIP GetMineArmed tests it). BGSProjectile +0x60 its base's flags
    // (xNVSE GameForms.h projFlags): a mine has explosion, alt trigger and can-be-disabled (0x26) but not
    // detonates (0x400) (JIP GetMineArmed: flags & 0x426 == 0x26). Confirmed in game (lab 2026-10-19: the laptop's
    // disarm turned 0x200 on; a mine picked up read flags 0x2B, deleted, and one blown up 0x80B, disabled).
    constexpr uint32_t kProjFlags = 0xC8;
    constexpr uint32_t kProjTurnedOff = 0x200;
    constexpr uint16_t kMineBaseMask = 0x426, kMineBaseFlags = 0x26;
    // ContainerMenu +0x74: the container or body it shows (JIP GameUI.h containerRef). Confirmed in game (coop box
    // showed the box coop openbox opened).
    constexpr uint32_t kContainerMenuRef = 0x74;
    constexpr uint32_t kBarterMenuMerchant = 0x80;
    constexpr uint32_t kDialogMenuPartner = 0x80;  // DialogMenu: who the conversation is with (see kDialogMenu)
    // DialogMenu +0x70: where the list node of the topic being said is kept (that node's item, MenuTopic*, first).
    // Read in Ghidra (the dialog menu's response update, 0x762FF0, gets it so and asks it for its response).
    // MenuTopic +0x0C its responses (a list: item, next), +0x18 its line (TESTopicInfo*), +0x20 the node of the
    // next response to say (null: the last is being said); a response's +0x00 is its text
    // (BSString: char* first). MenuTopic and DialogueResponse: Johnny Guitar MenuTopic.hpp / DialogueResponse.hpp.
    // Confirmed in game (lab coop dialogmem, 2026-10-15: Doc Mitchell's "Welcome back. I had hoped..." first in the
    // list, +0x20 null while he said it).
    constexpr uint32_t kDialogMenuTopicNode = 0x70;
    constexpr uint32_t kMenuTopicResponses = 0x0C;
    constexpr uint32_t kMenuTopicInfo = 0x18;
    constexpr uint32_t kMenuTopicResponseNode = 0x20;
    constexpr uint32_t kMenuTopicTopic = 0x1C;  // TESTopic* (Johnny Guitar MenuTopic.hpp pTopic)
    // DialogueResponse +0x28: its number in its line (Johnny Guitar DialogueResponse.hpp uiResponseNumber). The list
    // isn't in speaking order (two-PC test 2026-10-15: Chet's Great Khans lines came out second part first), so
    // parts are matched by this. Confirmed in game (lab coop spoken: Chet's greeting "I've got plenty of supplies"
    // 2, "You looking to buy some supplies?" 1; +0x0C emotion value 50 and +0x24 flags 1 as Johnny Guitar has them).
    constexpr uint32_t kResponseNumber = 0x28;
    // MobileObject +0x7D: talking now (JIP GameObjects.h isTalking; the line-saying function 0x8A5580 sets it).
    constexpr uint32_t kIsTalking = 0x7D;
    constexpr uint32_t kDialogueItemInfo = 0x0C;
    constexpr uint32_t kActorProcess = 0x68;          // Actor: its AI process (JIP GameObjects.h baseProcess)
    constexpr uint32_t kHudSubtitles = 0x198;
    constexpr uint32_t kHudSubtitleShown = 0x1A8;
    constexpr uint32_t kSubtitleShowFor = 0x108;
    // ExtraCount (extra data type 0x24, JIP GameExtraData.h): a stack's size, SInt16 at +0x0C (none: 1). Confirmed
    // in game (coop items: 24 rounds of ammo, a dropped stack of 2). After a pick-up it reads 1.
    constexpr uint8_t kExtraCount = 0x24;
    constexpr uint32_t kCountValue = 0x0C;
    // Quests (JIP GameData.h / GameForms.h; step 7.1). DataHandler +0x118: tList<TESQuest> (first node inline:
    // data, next). TESQuest: +0x3C questFlags (1 running, 2 completed, 0x20 shown in the Pip-Boy, 0x40 failed),
    // +0x40 questDelayTime (float, seconds between its script's runs), +0x44 tList<StageInfo> (StageInfo: +0 stage,
    // +1 isDone), +0x4C tList of objectives and script variables, +0x60 currentStage (UInt8), +0x64 editor id
    // (String: char* then UInt16 length). BGSQuestObjective: +0x04 objectiveId, +0x20 status (bit 1 shown, bit 2
    // completed). PlayerCharacter +0x6B8: the quest being tracked. Not yet confirmed in game (lab: coop quests).
    constexpr uint32_t kDataHandlerQuests = 0x118;
    constexpr uint32_t kQuestFlags = 0x3C;
    constexpr uint32_t kQuestDelay = 0x40;
    constexpr uint32_t kQuestStages = 0x44;
    constexpr uint32_t kQuestObjectives = 0x4C;
    constexpr uint32_t kQuestCurrentStage = 0x60;
    constexpr uint32_t kQuestEditorId = 0x64;
    constexpr uint32_t kStageId = 0x00;
    constexpr uint32_t kStageDone = 0x01;
    constexpr uint32_t kObjectiveId = 0x04;
    constexpr uint32_t kObjectiveStatus = 0x20;
    constexpr uint32_t kPlayerActiveQuest = 0x6B8;
    // PlayerCharacter +0x6BC tList<BGSQuestObjective> and +0x6C4 tList<ObjectiveTarget> (JIP GameObjects.h
    // questObjectiveList, questTargetList): the player's own lists of shown objectives and their targets. Not yet
    // confirmed in game (lab: coop questlog).
    constexpr uint32_t kPlayerObjectives = 0x6BC;
    constexpr uint32_t kPlayerTargets = 0x6C4;
    // TESQuest +0x18 TESScriptableForm, whose +0x04 is its Script* (JIP GameForms.h; quests without a script are
    // normal, so null means "nothing to run"). TESQuest +0x5C ScriptLocals*, whose +0x0C is a tList<ScriptVar>*
    // (ScriptVar: +0 id, +8 double value; JIP GameAPI.h). Not yet confirmed in game (lab: coop questvars).
    constexpr uint32_t kQuestScript = 0x1C;
    // Reputations (step 7.2; JIP GameData.h / GameForms.h): DataHandler +0x90 tList<TESReputation>; TESReputation
    // +0x1C its name (TESFullName at +0x18), and two floats JIP calls positiveRep (+0x44) and negativeRep (+0x48).
    // Confirmed in game the other way round (two-PC lab, 2026-10-15: AddReputationExact RepNVNovac 0 10, fame,
    // changed +0x48, and GetReputation ... 0 read it back): fame is +0x48, infamy +0x44.
    constexpr uint32_t kDataHandlerReputations = 0x90;
    // Loaded mods (step 8.6; xNVSE GameData.h): DataHandler +0x210 ModList, its loadedModCount at +0x218 and
    // ModInfo* loadedMods[0xFF] at +0x21C (in load order); a ModInfo's (TESFile's) file name, char[0x104], at +0x20.
    // Filled before the main menu. Confirmed in game (2026-10-16: the same list as the script's GetNthModName, and
    // read at the laptop's main menu).
    constexpr uint32_t kDataHandlerModCount = 0x218;
    constexpr uint32_t kDataHandlerMods = 0x21C;
    constexpr uint32_t kModFileName = 0x20;
    constexpr uint32_t kReputationName = 0x1C;
    constexpr uint32_t kReputationFame = 0x48;
    constexpr uint32_t kReputationInfamy = 0x44;
    // TESQuest +0x30 TESFullName, whose +0x04 is the name (String); BGSQuestObjective +0x08: its text (String).
    // (JIP GameForms.h.) Not yet confirmed in game.
    constexpr uint32_t kQuestName = 0x34;
    constexpr uint32_t kObjectiveText = 0x08;
    constexpr uint32_t kQuestLocals = 0x5C;
    constexpr uint32_t kLocalsVars = 0x0C;
    constexpr uint32_t kVarId = 0x00;
    constexpr uint32_t kVarValue = 0x08;
    // TESQuest::flags bit set once the quest is completed.
    constexpr uint8_t kQuestCompleted = 0x02;
    // TESActorBaseData::flags bit: essential (knocked out at 0 health instead of dying).
    constexpr uint32_t kActorBaseEssential = 1u << 1;
    // Actor::lifeState values (names as JIP LN uses them). 3 and 6: knocked out (6: essential, at 0 health).
    // 1 and 2: dying and dead (JIP nvse/GameObjects.h Actor::GetDead).
    constexpr uint32_t kLifeStateDying = 1;
    constexpr uint32_t kLifeStateDead = 2;
    constexpr uint32_t kLifeStateUnconscious = 3;
    constexpr uint32_t kLifeStateEssentialDown = 6;
    // xNVSE IsControlPressed code for the Activate control (E by default; follows the player's key bindings).
    constexpr int kControlActivate = 5;
}

// Steam (step 8.5). Not game memory: the function slots of Steam's interfaces, as Valve's public Steamworks headers
// lay them out (read in the Goldberg emulator's copies of every version, github.com/Detanup01/gbe_fork sdk/steam).
// The game's steam_api.dll (2010) exports SteamClient() (an ISteamClient009) and SteamAPI_GetHSteamUser/Pipe and
// SteamAPI_RegisterCallback; the Steam client then hands out today's interfaces by version string. Functions
// returning a CSteamID return it through a hidden pointer after `this` (MSVC, member functions). Confirmed in game
// (playthrough test with a friend, 2026-10-16: our names, IDs and friends counts; the friend's game saw the host's
// rich presence among its friends; P2P connected both ways, session requests and connect failures arrived).
namespace game::steamslots
{
    // ISteamClient009 (thiscall): GetISteamUser 5, GetISteamFriends 8, GetISteamNetworking 17; each (HSteamUser,
    // HSteamPipe, version).
    constexpr int kClientUser = 5, kClientFriends = 8, kClientNetworking = 17;
    // ISteamUser ("SteamUser013"): GetSteamID 2.
    constexpr const char* kUserVersion = "SteamUser013";
    constexpr int kUserSteamId = 2;
    // ISteamUser ("SteamUser021", for its voice; isteamuser021.h): StartVoiceRecording 7, StopVoiceRecording 8,
    // GetAvailableVoice 9 (uint32* compressed bytes, uint32* (deprecated, 0), uint32 (deprecated, 0)), GetVoice 10
    // (bool want compressed, void* out, uint32 out size, uint32* written, then 5 deprecated: false, 0, 0, 0, 0),
    // DecompressVoice 11 (const void* in, uint32 size, void* out, uint32 out size, uint32* written, uint32 sample
    // rate: 11025 to 48000), each an EVoiceResult (steamclientpublic.h: 0 OK, 2 not recording, 3 no data, 4 buffer
    // too small, 5 data corrupted).
    constexpr const char* kVoiceUserVersion = "SteamUser021";
    constexpr int kVoiceStart = 7, kVoiceStop = 8, kVoiceAvailable = 9, kVoiceGet = 10, kVoiceDecompress = 11;
    // ISteamNetworking ("SteamNetworking006"): SendP2PPacket 0 (to, data, size, send type: 0 unreliable, 2 reliable;
    // channel), IsP2PPacketAvailable 1, ReadP2PPacket 2, AcceptP2PSessionWithUser 3, CloseP2PSessionWithUser 4,
    // AllowP2PPacketRelay 7.
    constexpr const char* kNetworkingVersion = "SteamNetworking006";
    constexpr int kSend = 0, kAvailable = 1, kRead = 2, kAccept = 3, kClose = 4, kAllowRelay = 7;
    // ISteamFriends ("SteamFriends018"): GetPersonaName 0, GetFriendCount 2 (flags: 4 regular friends),
    // GetFriendByIndex 3, GetFriendRelationship 4 (3: friend), GetFriendPersonaName 6, GetFriendGamePlayed 7
    // (FriendGameInfo_t, 24 bytes: the game's id first, its app id in the low 24 bits), SetRichPresence 41,
    // ClearRichPresence 42, GetFriendRichPresence 43.
    constexpr const char* kFriendsVersion = "SteamFriends018";
    constexpr int kPersonaName = 0, kFriendCount = 2, kFriendByIndex = 3, kRelationship = 4, kFriendName = 6,
        kFriendGame = 7, kSetPresence = 41, kClearPresence = 42, kFriendPresence = 43;
    // Callbacks (k_iSteamNetworkingCallbacks 1200): P2PSessionRequest_t 1202 (the CSteamID, 8 bytes),
    // P2PSessionConnectFail_t 1203 (CSteamID, then a uint8 reason; 16 bytes, packed to 8 on Windows).
    constexpr int kSessionRequest = 1202, kConnectFail = 1203;
    constexpr uint32_t kNewVegasAppId = 22380;
}

