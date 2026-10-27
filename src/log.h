#pragma once

// Simple log file: NVCoop.log in the game folder. Thread-safe.
// Each line: time, level, message. Use LOG_INFO("joined %s", name) etc. Lines reach the file within 200 ms (written
// by a thread of ours, so the game never waits for the disk); errors at once.

enum class LogLevel { Debug, Info, Warn, Error };

namespace Log
{
    // Opens NVCoop.log; if another game on this PC already has it open, NVCoop-2.log, etc.
    void Open(const char* folder);
    void EchoToConsole(bool on);
    void Flush();  // every line so far into the file now (the game closing, a crash)
    void Write(LogLevel level, const char* fmt, ...);
}

#define LOG_DEBUG(...) Log::Write(LogLevel::Debug, __VA_ARGS__)
#define LOG_INFO(...)  Log::Write(LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...)  Log::Write(LogLevel::Warn, __VA_ARGS__)
#define LOG_ERROR(...) Log::Write(LogLevel::Error, __VA_ARGS__)
