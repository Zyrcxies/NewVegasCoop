#pragma once

// Settings from NVCoop.ini, next to NVCoop.dll (created with defaults if missing), and the player's own from the MCM
// menu's file.

#include <cstdint>
#include <string>
#include <vector>

struct Config
{
    // Menu: the main menu's Host Co-op and Join Co-op decide (the others start by themselves: for labs).
    enum class Mode { Off, Menu, Host, Join, Auto } mode = Mode::Menu;
    std::string address = "127.0.0.1";
    uint16_t port = 27960;
    std::string name = "Player";
    // Host: mods whose quests the host's game runs and shares like the base game's (step 8.6; file names, from
    // HostQuestMods, commas between). Other mods' quests run in each player's game.
    std::vector<std::string> hostQuestMods;
    bool allowSecondCopy = false;  // developer-only
    bool secondCopyWaits = false;  // developer-only: the second copy waits for a debugger before going on
    bool blockDiscordOverlay = true;  // its hook crashes the game, even with the overlay off
    bool borderless = true;  // Borderless: a borderless window instead of fullscreen (alt-tab doesn't crash it)
    bool desyncNotices = true;        // corner messages for desyncs (DESIGN rule 8)
    int fieldOfView = 0;              // the world's field of view, 60 to 120 (0: the game's own)
    bool friendMarkers = true;        // a chevron and the name over each friend
    bool syncRadio = true;            // a friend's radios play the host's stations' songs
    bool chat = true;                 // text chat
    int chatKey = 21;                 // the key that opens it (DirectInput code; 21: Y)
    bool chatSpeech = true;           // chat lines read out loud
    int chatSpeechVolume = 80;        // 0 to 100
    int voice = 0;                    // the voice others hear our lines in: 0 male, 1 female
    int voiceVariant = 0;             // which voice of that kind (0 to 3)
    int voicePitch = 0;               // -10 to 10
    int voiceSpeed = 0;               // -10 to 10
    int voiceEffect = 0;              // coop::VoiceEffect
    bool voiceChat = true;            // voice chat
    int voiceChatKey = 48;            // push to talk (DirectInput code; 48: B)
    int voiceChatMode = 0;            // 0 push to talk, 1 open mic
    int voiceChatVolume = 100;        // others' voices, 0 to 100
    bool mouths = true;               // characters' mouths move while their player talks (voice or speech)
    bool skipDlcPopups = true;        // the DLCs' and packs' intro popups out of Doc Mitchell's house skipped
    bool labChannel = false;  // developer-only: tools on this PC can run console lines (dev/lab_channel.h)
    bool lanCoop = false;  // developer-only: also host on (and list for Join) the IP connection: Port, Address
    // Joining by IP (Alex, 2026-10-30: for port forwarding or a free VPN): the host also lets friends join on Port
    // (IpHosting=1; the host still lets each one in), and Join Co-op lists JoinAddress ("ip" or "ip:port").
    bool ipHosting = false;
    std::string joinAddress;  // "" none (checked: net::ParseHostAddress)
    uint16_t joinPort = 0;
    bool autoApproveJoins = false;  // developer-only: everyone who asks to join is let in without asking (labs)

    static Config Load(const std::string& iniPath);
    // The player's own settings (chat, speech, radio, notices) from the MCM menu's file (Data\config\NVCoop.ini,
    // [Settings]); written with ours if it isn't there. Again when it changes: the menu saves when it closes.
    void LoadPlayer(const std::string& path);
};
