#pragma once

// The game's Steam, for co-op over Steam (step 8.5): its peer-to-peer networking for net::SteamSession, our
// friends list, and "rich presence" (the tag that tells friends' games we're hosting). New Vegas's steam_api.dll is
// from 2010, but the Steam client behind it still hands out today's interfaces (asked for by version through its
// SteamClient()). Interface layouts: game/addresses.h.

#include "net/steam_session.h"

#include <cstdint>
#include <string>
#include <vector>

namespace game
{
    // Once (then cached): whether the game's Steam is there and logged in. Call from the game thread, after the
    // main menu is up (the game starts Steam first).
    bool SteamReady();
    net::SteamApi& Steam();  // only when SteamReady
    std::string SteamName();  // our Steam name ("" if none)

    // Rich presence: whether friends' games see us hosting co-op (this mod's version, so only matching ones list us).
    void SetHostingOnSteam(bool hosting, const std::string& mode = "");  // mode: the save's, shown to friends

    struct SteamFriend
    {
        net::SteamId id = 0;
        std::string name;
        std::string mode;  // their save's mode, as they show it ("" if not)
    };
    std::vector<SteamFriend> HostingFriends();  // Steam friends in New Vegas, hosting co-op now

    // Voice chat through Steam's voice (it records the microphone and squeezes it small). VoiceRecording: on or off
    // (false: no Steam voice), done on a thread of ours. TakeVoice: a piece it recorded (false: nothing waiting).
    // StopVoice: that thread ends (the game is closing). DecompressVoice (the game's thread): another player's bytes
    // back to 16-bit mono samples at `rate`.
    bool VoiceRecording(bool on);
    bool TakeVoice(std::vector<uint8_t>& out);
    void StopVoice();
    bool DecompressVoice(const std::vector<uint8_t>& in, uint32_t rate, std::vector<int16_t>& out);
}
