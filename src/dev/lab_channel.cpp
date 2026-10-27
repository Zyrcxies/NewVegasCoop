#include "dev/lab_channel.h"

#include "log.h"

#include <windows.h>
#include <sddl.h>

#include <chrono>
#include <vector>

namespace dev
{
    namespace
    {
        // A pipe's default permissions let every account on the PC read it: ours allows this user only
        // ("D:P(A;;GA;;;<this user's SID>)", nothing inherited).
        bool OnlyThisUser(SECURITY_ATTRIBUTES& sa, PSECURITY_DESCRIPTOR& sd)
        {
            HANDLE token = nullptr;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
                return false;
            DWORD size = 0;
            GetTokenInformation(token, TokenUser, nullptr, 0, &size);
            std::vector<BYTE> buffer(size);
            bool ok = size && GetTokenInformation(token, TokenUser, buffer.data(), size, &size);
            CloseHandle(token);
            LPWSTR sid = nullptr;
            if (!ok || !ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sid))
                return false;
            std::wstring sddl = L"D:P(A;;GA;;;" + std::wstring(sid) + L")";
            LocalFree(sid);
            if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &sd, nullptr))
                return false;
            sa = {sizeof(sa), sd, FALSE};
            return true;
        }

        bool HasControlCharacters(const std::string& line)
        {
            for (unsigned char c : line)
                if (c < 0x20 || c == 0x7F)
                    return true;
            return false;
        }
    }

    LabChannel::~LabChannel()
    {
        Stop();
    }

    bool LabChannel::Start(const std::wstring& pipeName)
    {
        if (Running())
            return false;
        SECURITY_ATTRIBUTES sa{};
        PSECURITY_DESCRIPTOR sd = nullptr;
        if (!OnlyThisUser(sa, sd))
        {
            LOG_ERROR("Lab channel: couldn't limit it to this user (error %lu); not opened", GetLastError());
            return false;
        }
        // One client at a time; refused from other PCs; and we must be the one who made it (nothing else on the
        // PC can have opened a pipe of that name first to pose as the game).
        HANDLE pipe = CreateNamedPipeW(pipeName.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 4096, 4096, 0, &sa);
        LocalFree(sd);
        if (pipe == INVALID_HANDLE_VALUE)
        {
            LOG_ERROR("Lab channel: couldn't open it (error %lu)", GetLastError());
            return false;
        }
        m_pipe = pipe;
        m_ioEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        m_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        m_stopping = false;
        m_hasCommand = m_hasReply = false;
        m_thread = std::thread(&LabChannel::Run, this);
        return true;
    }

    void LabChannel::Stop()
    {
        if (!Running())
            return;
        {
            std::lock_guard lock(m_mutex);
            m_stopping = true;
        }
        m_answered.notify_all();
        SetEvent(m_stopEvent);
        m_thread.join();
        CloseHandle(m_pipe);
        CloseHandle(m_ioEvent);
        CloseHandle(m_stopEvent);
        m_pipe = m_ioEvent = m_stopEvent = nullptr;
        m_hasCommand = m_hasReply = false;
    }

    bool LabChannel::TakeCommand(uint32_t& id, std::string& line)
    {
        std::lock_guard lock(m_mutex);
        if (!m_hasCommand)
            return false;
        m_hasCommand = false;
        id = m_commandId;
        line = std::move(m_command);
        return true;
    }

    void LabChannel::Reply(uint32_t id, std::string text)
    {
        std::string oneLine;
        for (char c : text)
        {
            if (c == '\n')
                oneLine += " | ";
            else if (c != '\r')
                oneLine += c;
        }
        {
            std::lock_guard lock(m_mutex);
            m_hasReply = true;
            m_replyId = id;
            m_reply = std::move(oneLine);
        }
        m_answered.notify_all();
    }

    std::string LabChannel::Ask(const std::string& line)
    {
        std::unique_lock lock(m_mutex);
        uint32_t id = m_nextId++;
        m_commandId = id;
        m_command = line;
        m_hasCommand = true;
        m_hasReply = false;
        bool answered = m_answered.wait_for(lock, std::chrono::milliseconds(kLabReplyWaitMs),
            [&] { return m_stopping || (m_hasReply && m_replyId == id); });
        if (m_stopping)
            return "stopping";
        if (!answered)
        {
            m_hasCommand = false;  // (not run later by surprise)
            return "timeout: the game didn't take it (loading, or not running its frames?)";
        }
        m_hasReply = false;
        return std::move(m_reply);
    }

    bool LabChannel::WaitIo(void* overlapped)
    {
        auto* ov = static_cast<OVERLAPPED*>(overlapped);
        HANDLE events[2] = {m_ioEvent, m_stopEvent};
        if (WaitForMultipleObjects(2, events, FALSE, INFINITE) == WAIT_OBJECT_0)
            return true;
        CancelIoEx(m_pipe, ov);
        DWORD ignored = 0;
        GetOverlappedResult(m_pipe, ov, &ignored, TRUE);  // the cancel finishing
        return false;
    }

    bool LabChannel::WriteLine(const std::string& text, void* overlapped)
    {
        auto* ov = static_cast<OVERLAPPED*>(overlapped);
        std::string data = text + "\n";
        ResetEvent(m_ioEvent);
        if (!WriteFile(m_pipe, data.data(), static_cast<DWORD>(data.size()), nullptr, ov) &&
            GetLastError() != ERROR_IO_PENDING)
            return false;
        if (!WaitIo(ov))
            return false;
        DWORD wrote = 0;
        return GetOverlappedResult(m_pipe, ov, &wrote, FALSE) && wrote == data.size();
    }

    // Pipe thread: wait for a tool, read its lines, answer each, and wait for the next tool.
    void LabChannel::Run()
    {
        OVERLAPPED ov{};
        ov.hEvent = m_ioEvent;
        auto stopping = [&] { return WaitForSingleObject(m_stopEvent, 0) == WAIT_OBJECT_0; };
        while (!stopping())
        {
            ResetEvent(m_ioEvent);
            if (!ConnectNamedPipe(m_pipe, &ov))
            {
                DWORD error = GetLastError();
                if (error == ERROR_IO_PENDING)
                {
                    if (!WaitIo(&ov))
                        return;
                }
                else if (error != ERROR_PIPE_CONNECTED)
                {
                    LOG_WARN("Lab channel: waiting for a tool failed (error %lu)", error);
                    if (WaitForSingleObject(m_stopEvent, 1000) == WAIT_OBJECT_0)
                        return;
                    continue;
                }
            }
            LOG_DEBUG("Lab channel: a tool connected");
            std::string pending;
            bool skipping = false;  // the rest of a line already refused as too long, up to its line break
            for (bool open = true; open;)
            {
                char buffer[512];
                DWORD got = 0;
                ResetEvent(m_ioEvent);
                if (!ReadFile(m_pipe, buffer, sizeof(buffer), nullptr, &ov) && GetLastError() != ERROR_IO_PENDING)
                    break;
                if (!WaitIo(&ov))
                    return;
                if (!GetOverlappedResult(m_pipe, &ov, &got, FALSE) || got == 0)
                    break;
                pending.append(buffer, got);
                for (size_t end; open && (end = pending.find('\n')) != std::string::npos;)
                {
                    std::string line = pending.substr(0, end);
                    pending.erase(0, end + 1);
                    if (skipping)
                    {
                        skipping = false;
                        continue;
                    }
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    std::string answer = line.size() > kMaxLabLine ? "refused: line too long" :
                        HasControlCharacters(line) ? "refused: control characters" : Ask(line);
                    open = !stopping() && WriteLine(answer, &ov);
                }
                if (open && pending.size() > kMaxLabLine)
                {
                    pending.clear();
                    if (!skipping)
                        open = WriteLine("refused: line too long", &ov);
                    skipping = true;
                }
            }
            if (stopping())
                return;
            DisconnectNamedPipe(m_pipe);
            LOG_DEBUG("Lab channel: the tool left");
        }
    }
}
