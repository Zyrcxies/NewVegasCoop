#include "dev/dump_game.h"

#include "log.h"

#include <vector>
#include <windows.h>

bool DumpGameImage(const std::string& path)
{
    auto* base = reinterpret_cast<const uint8_t*>(GetModuleHandleA(nullptr));
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    DWORD size = nt->OptionalHeader.SizeOfImage;

    // Copy the whole image; pages that can't be read stay zero.
    std::vector<uint8_t> image(size, 0);
    for (DWORD at = 0; at < size;)
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(base + at, &mbi, sizeof(mbi)))
            break;
        DWORD end = static_cast<DWORD>(reinterpret_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize - base);
        end = end > size ? size : end;
        bool readable = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
        if (readable)
            memcpy(image.data() + at, base + at, end - at);
        at = end;
    }

    // In memory each section sits at its virtual address: make the file say so, so tools load it as-is.
    auto* fileNt = reinterpret_cast<IMAGE_NT_HEADERS32*>(image.data() + dos->e_lfanew);
    fileNt->OptionalHeader.FileAlignment = fileNt->OptionalHeader.SectionAlignment;
    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(fileNt);
    for (WORD i = 0; i < fileNt->FileHeader.NumberOfSections; ++i)
    {
        DWORD virtualSize = sections[i].Misc.VirtualSize ? sections[i].Misc.VirtualSize : sections[i].SizeOfRawData;
        sections[i].PointerToRawData = sections[i].VirtualAddress;
        sections[i].SizeOfRawData = virtualSize;
        if (sections[i].VirtualAddress + virtualSize > size)
            sections[i].SizeOfRawData = size - sections[i].VirtualAddress;
    }

    HANDLE f = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    bool ok = WriteFile(f, image.data(), size, &written, nullptr) && written == size;
    CloseHandle(f);
    LOG_INFO("Saved the game's program from memory to %s (%u bytes)", path.c_str(), size);
    return ok;
}
