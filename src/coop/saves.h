// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <string>
#include <string_view>

namespace coop
{
    constexpr size_t kMaxSaveName = 100;
    constexpr size_t kMaxSaveHostName = 32;

    inline std::string SaveNameOf(std::string_view file)
    {
        if (file.size() > 4 && (file.substr(file.size() - 4) == ".fos" || file.substr(file.size() - 4) == ".FOS"))
            file.remove_suffix(4);
        if (file.empty() || file.size() > kMaxSaveName || file.find("..") != std::string_view::npos)
            return "";
        for (char c : file)
        {
            bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' ||
                c == '-' || c == '_' || c == '.' || c == ',' || c == '(' || c == ')' || c == '\'';
            if (!plain)
                return "";
        }
        if (file.front() == ' ' || file.front() == '.' || file.back() == ' ' || file.back() == '.')
            return "";
        return std::string(file);
    }

    inline std::string PlainName(std::string_view name, const char* otherwise)
    {
        std::string who;
        for (char c : name)
            if (((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' ||
                    c == '_') && who.size() < kMaxSaveHostName)
                who += c;
        while (!who.empty() && who.back() == ' ')
            who.pop_back();
        while (!who.empty() && who.front() == ' ')
            who.erase(who.begin());
        return who.empty() ? otherwise : who;
    }

    inline std::string LinkedSavePrefix(std::string_view host) { return "Co-op - " + PlainName(host, "Host") + " - "; }

    inline std::string LinkedSaveName(std::string_view host, std::string_view hostSave)
    {
        std::string save = SaveNameOf(hostSave);
        return save.empty() ? "" : LinkedSavePrefix(host) + save;
    }

    inline std::string SaveForFriend(std::string_view friendName, std::string_view when)
    {
        return SaveNameOf("Co-op " + PlainName(friendName, "Friend") + " " + std::string(when));
    }

    inline std::string HostSaveOf(std::string_view host, std::string_view friendSave)
    {
        std::string name = SaveNameOf(friendSave), prefix = LinkedSavePrefix(host);
        if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0)
            return "";
        return SaveNameOf(name.substr(prefix.size()));
    }

    inline bool IsAutosave(std::string_view save)
    {
        constexpr std::string_view kAuto = "autosave";
        if (save.size() < kAuto.size())
            return false;
        for (size_t i = 0; i < kAuto.size(); ++i)
            if ((save[i] | 0x20) != kAuto[i])
                return false;
        return true;
    }
}
