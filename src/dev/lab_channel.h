#pragma once

// The lab channel (DESIGN "Scope": side projects; lab automation): a tool on this PC can send the game lines to
// run and read an answer back, so labs run without someone at the keyboard. A Windows named pipe only this user
// on this PC can open (remote clients are refused, and we must be the one who made it). Off unless NVCoop.ini
// says LabChannel=1. Its own thread talks to the pipe; the game thread takes each line (TakeCommand) and answers
// it (Reply). One line at a time: the next isn't read until the last one is answered.

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace dev
{
    constexpr size_t kMaxLabLine = 1000;       // longer lines are refused
    constexpr uint32_t kLabReplyWaitMs = 30000;  // a line not answered in this time gets "timeout"

    class LabChannel
    {
    public:
        LabChannel() = default;
        LabChannel(const LabChannel&) = delete;
        LabChannel& operator=(const LabChannel&) = delete;
        ~LabChannel();

        bool Start(const std::wstring& pipeName);  // e.g. L"\\\\.\\pipe\\NVCoopLab"
        void Stop();
        bool Running() const { return m_thread.joinable(); }

        // Game thread: the line waiting to be run, if any.
        bool TakeCommand(uint32_t& id, std::string& line);
        // Game thread: its answer (one line: line breaks become " | ").
        void Reply(uint32_t id, std::string text);

    private:
        void Run();
        bool WaitIo(void* overlapped);  // false: stopping (the I/O was cancelled)
        bool WriteLine(const std::string& text, void* overlapped);
        std::string Ask(const std::string& line);  // pipe thread: hand a line to the game thread, wait for its answer

        void* m_pipe = nullptr;       // HANDLE
        void* m_ioEvent = nullptr;    // HANDLE
        void* m_stopEvent = nullptr;  // HANDLE
        std::thread m_thread;

        std::mutex m_mutex;
        std::condition_variable m_answered;
        bool m_stopping = false;
        uint32_t m_nextId = 1;
        bool m_hasCommand = false;  // a line waits for the game thread
        uint32_t m_commandId = 0;
        std::string m_command;
        bool m_hasReply = false;
        uint32_t m_replyId = 0;
        std::string m_reply;
    };
}
