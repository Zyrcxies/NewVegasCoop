#pragma once

// Linked saves (step 8.3, DESIGN rule 3): when the host saves, every friend's game saves too, named after the
// host's save; loading the host's save loads each friend's matching one. The names come from another PC, so they
// are untrusted: only plain characters, no folders, short. Game-independent: tested in tests/.

#include <string>
#include <string_view>

namespace coop
{
    constexpr size_t kMaxSaveName = 100;     // the host's save's name
    constexpr size_t kMaxSaveHostName = 32;  // the host's player name, as it goes into a friend's save name

    // A save's name from its file name ("Quicksave.fos" -> "Quicksave"); "" if it can't be one of ours to share:
    // empty, too long, a folder in it, or a character outside letters, digits, space and - _ . , ( ) '.
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
            return "";  // (Windows trims these: two names could meet)
        return std::string(file);
    }

    // A player's name cut down for a save name: letters, digits, space, - and _ only (others dropped), at most
    // kMaxSaveHostName, no spaces at the ends; `otherwise` if nothing is left.
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

    // What starts a friend's saves matching this host's: "Co-op - <host> - ".
    inline std::string LinkedSavePrefix(std::string_view host) { return "Co-op - " + PlainName(host, "Host") + " - "; }

    // A friend's save matching the host's: "Co-op - <host> - <host's save>"; "" if the save's name isn't valid
    // (SaveNameOf).
    inline std::string LinkedSaveName(std::string_view host, std::string_view hostSave)
    {
        std::string save = SaveNameOf(hostSave);
        return save.empty() ? "" : LinkedSavePrefix(host) + save;
    }

    // The host's save of a friend's save (everyone saves when anyone does): "Co-op <friend> <when>", when like
    // "2026-10-15 23-27-45". A new name each time, so it never replaces one of the host's own saves.
    inline std::string SaveForFriend(std::string_view friendName, std::string_view when)
    {
        return SaveNameOf("Co-op " + PlainName(friendName, "Friend") + " " + std::string(when));
    }

    // The host's save a friend's save matches ("" if it isn't one of the host's linked saves).
    inline std::string HostSaveOf(std::string_view host, std::string_view friendSave)
    {
        std::string name = SaveNameOf(friendSave), prefix = LinkedSavePrefix(host);
        if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0)
            return "";
        return SaveNameOf(name.substr(prefix.size()));
    }

    // One of the game's own autosaves (made at doors; the host's already save everyone's).
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
