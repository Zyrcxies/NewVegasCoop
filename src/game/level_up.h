// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

namespace coop
{
    struct Balance;
}

namespace game
{
    void TakeOverLevelUpSkillPoints();
    void SetLevelUpBalance(const coop::Balance* balance);
}
