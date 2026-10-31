// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace dev
{
    constexpr size_t kMaxLabLine = 1000;
    constexpr uint32_t kLabReplyWaitMs = 30000;

    class LabChannel
    {
    public:
        LabChannel() = default;
        LabChannel(const LabChannel&) = delete;
        LabChannel& operator=(const LabChannel&) = delete;
        ~LabChannel();

        bool Start(const std::wstring& pipeName);
        void Stop();
        bool Running() const { return m_thread.joinable(); }

        bool TakeCommand(uint32_t& id, std::string& line);
        void Reply(uint32_t id, std::string text);

    private:
        void Run();
        bool WaitIo(void* overlapped);
        bool WriteLine(const std::string& text, void* overlapped);
        std::string Ask(const std::string& line);

        void* m_pipe = nullptr;
        void* m_ioEvent = nullptr;
        void* m_stopEvent = nullptr;
        std::thread m_thread;

        std::mutex m_mutex;
        std::condition_variable m_answered;
        bool m_stopping = false;
        uint32_t m_nextId = 1;
        bool m_hasCommand = false;
        uint32_t m_commandId = 0;
        std::string m_command;
        bool m_hasReply = false;
        uint32_t m_replyId = 0;
        std::string m_reply;
    };
}
