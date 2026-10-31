// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

void InstallCrashReporter();

struct AddressSpace
{
    bool large = false;
    unsigned freeMb = 0, totalMb = 0;
};
AddressSpace GetAddressSpace();
void CheckCrashReporter();
void FreezeBeat(bool watch);
