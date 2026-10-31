// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

namespace coop
{
    struct StackNode
    {
        const void* stack;
        const StackNode* next;
    };

    inline bool HoldsStack(const StackNode* head, const void* stack)
    {
        int seen = 0;
        for (const StackNode* node = head; node; node = node->next)
            if (node->stack == stack || ++seen > 10000)
                return true;
        return false;
    }
}
