#pragma once

// Mod-list sync, fixing a friend's mods (step 8.6 parts 2 and 3): the host's game sends the mod files a friend is
// missing (plugins and their archives; never xNVSE plugins), the friend's game checks each against the host's list
// (size and SHA-256) and puts them in Data, older versions moved to a backup folder first, then matches the
// host's load order. Game-independent: tested in tests/ (folders, not the game).

#include "coop/mods.h"
#include "coop/sha256.h"
#include "net/protocol.h"
#include "net/session.h"

#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace coop
{
    // Host: reads the pieces friends ask for on its own thread, so a slow disk never holds up the game.
    class ModServer
    {
    public:
        struct Piece
        {
            net::PeerId peer = 0;
            net::ModPiece piece;
        };
        static constexpr size_t kMaxWaiting = 32;  // per friend; more is a bad friend

        ~ModServer();
        // False if the friend already has kMaxWaiting pieces waiting.
        bool Ask(net::PeerId peer, uint16_t index, const std::string& path, uint64_t offset, uint32_t length);
        bool Take(Piece& out);
        void Forget(net::PeerId peer);  // they left: drop what they asked for

    private:
        struct Job
        {
            net::PeerId peer;
            uint16_t index;
            std::string path;
            uint64_t offset;
            uint32_t length;
        };
        void Run();
        size_t WaitingFor(net::PeerId peer) const;

        std::thread m_thread;
        mutable std::mutex m_mutex;
        std::condition_variable m_wake;
        std::deque<Job> m_jobs;
        std::deque<Piece> m_done;
        bool m_stop = false;
    };

    // Friend: fetches the files it needs from the host, one at a time, a few pieces ahead.
    class ModDownload
    {
    public:
        struct Wanted
        {
            uint16_t index = 0;  // in the host's list
            net::ModFile file;   // as the host's list has it (hashed: only those can be checked, so sent)
        };
        static constexpr size_t kAhead = 16;  // pieces asked for at once (half of what the host keeps per friend)

        ~ModDownload() { Close(); }
        // Into folder (made if needed), each as "<name>.part" until checked, then "<name>".
        bool Start(std::vector<Wanted> files, const std::string& folder);
        void Asks(std::vector<net::ModFileAsk>& out);  // what to ask the host for now
        bool Got(const net::ModPiece& piece);          // false: bad piece (Failed)
        bool Fail(const std::string& why);             // stop here (the unfinished file is deleted)

        bool Done() const { return !m_failed && m_current >= m_files.size(); }
        bool Failed() const { return m_failed; }
        const std::string& Error() const { return m_error; }
        const std::vector<std::string>& Finished() const { return m_finished; }  // checked files' names
        size_t FilesTotal() const { return m_files.size(); }
        uint64_t BytesTotal() const;
        uint64_t BytesDone() const { return m_bytesDone; }

    private:
        bool OpenCurrent();
        bool FinishCurrent();
        void Close();

        std::vector<Wanted> m_files;
        std::string m_folder;
        size_t m_current = 0;
        FILE* m_out = nullptr;
        Sha256 m_hash;
        uint64_t m_received = 0;  // of the current file
        uint64_t m_nextAsk = 0;
        size_t m_waiting = 0;     // pieces asked for, not here yet
        uint64_t m_bytesDone = 0;
        std::vector<std::string> m_finished;
        bool m_failed = false;
        std::string m_error;
    };

    struct ModFolders
    {
        std::string data;       // the game's Data folder
        std::string downloads;  // in the game folder: downloads until checked and put in place
        std::string backup;     // in the game folder: older versions, extra archives and plugins.txt go here
        std::string pluginsTxt; // the game's list of active plugins (%LOCALAPPDATA%\FalloutNV\plugins.txt)
    };

    // Puts the checked downloads in Data (an older version moved to the backup first), moves extra archives to
    // the backup, makes the host's plugins the active ones (the old plugins.txt backed up) and dates the plugins in
    // the host's order (the game loads them by date). False (error says why) on the first thing that fails.
    bool ApplyModFix(const ModFolders& folders, const ModList& host, const std::vector<std::string>& downloaded,
        const std::vector<std::string>& extra, std::string& error);
}
