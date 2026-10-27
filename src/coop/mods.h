#pragma once

// Mod-list sync (step 8.6, DESIGN rule 2): each game's list of the mod files it loads, with sizes and fingerprints,
// so a joining friend's game can tell exactly what differs from the host's (and later fix it). Game-independent:
// tested in tests/ (the scan reads folders, not the game).

#include "net/protocol.h"

#include <string>
#include <vector>

namespace coop
{
    // Plugins in load order first, then archives, then xNVSE plugins (both sorted by name).
    using ModList = std::vector<net::ModFile>;

    bool IsGameArchive(const std::string& archive);  // the base game's own (Fallout - *.bsa, Update.bsa)
    // Whether the game loads an archive with this plugin: "<plugin>.bsa" or "<plugin> - <anything>.bsa".
    bool ArchiveBelongsTo(const std::string& archive, const std::string& plugin);

    // Fingerprint of the plugins and archives (what has to match to join; xNVSE plugins only warn).
    net::Hash ModsFingerprint(const ModList& mods);

    struct ModDifference
    {
        std::vector<std::string> missing;    // the host loads them, we don't have them
        std::vector<std::string> different;  // both have them, but not the same version
        std::vector<std::string> extra;      // we load them, the host doesn't
        bool orderDiffers = false;           // same plugins, another load order
        std::vector<std::string> missingDlls, differentDlls;  // xNVSE plugins (a warning, not a refusal)

        bool Blocks() const { return !missing.empty() || !different.empty() || !extra.empty() || orderDiffers; }
        bool Any() const { return Blocks() || !missingDlls.empty() || !differentDlls.empty(); }
    };
    ModDifference CompareMods(const ModList& host, const ModList& mine);

    // For the friend's screen (lists cut short) and the log (everything), one line each.
    std::string DescribeForScreen(const ModDifference& d, const std::string& hostName);
    std::vector<std::string> DescribeForLog(const ModDifference& d);

    // Reads the mod files this game loads: the active plugins (in load order) from dataFolder, their archives and
    // the base game's, and the xNVSE plugins in dataFolder\NVSE\Plugins (but skipDll: ours, checked by version).
    // Fingerprints are kept in cacheFile (by name, size and date) so big files are only read once.
    // Slow the first time (reads every mod file): not for the game thread.
    ModList ScanMods(const std::string& dataFolder, const std::vector<std::string>& plugins,
        const std::string& cacheFile, const std::string& skipDll);
}
