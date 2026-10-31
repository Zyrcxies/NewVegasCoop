// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstddef>

inline bool IsBlockedDll(const wchar_t* path, size_t len)
{
    static const wchar_t kName[] = L"discordhook.dll";
    const size_t n = sizeof(kName) / sizeof(kName[0]) - 1;
    if (!path || len < n)
        return false;
    const wchar_t* tail = path + len - n;
    if (len > n && tail[-1] != L'\\' && tail[-1] != L'/')
        return false;
    for (size_t i = 0; i < n; ++i)
    {
        wchar_t c = tail[i];
        if (c >= L'A' && c <= L'Z')
            c = static_cast<wchar_t>(c + (L'a' - L'A'));
        if (c != kName[i])
            return false;
    }
    return true;
}
