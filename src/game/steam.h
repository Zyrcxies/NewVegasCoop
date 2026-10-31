// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include "net/steam_session.h"

#include <cstdint>
#include <string>
#include <vector>

namespace game
{
    bool SteamReady();
    net::SteamApi& Steam();
    std::string SteamName();

    void SetHostingOnSteam(bool hosting, const std::string& mode = "");

    struct SteamFriend
    {
        net::SteamId id = 0;
        std::string name;
        std::string mode;
    };
    std::vector<SteamFriend> HostingFriends();

    bool VoiceRecording(bool on);
    bool TakeVoice(std::vector<uint8_t>& out);
    void StopVoice();
    bool DecompressVoice(const std::vector<uint8_t>& in, uint32_t rate, std::vector<int16_t>& out);
}
