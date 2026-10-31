// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

namespace coop
{
    constexpr int kAttackVersions = 3;

    template <class Play, class WeaponLayerPlays>
    int PlayAttackVersions(Play&& play, WeaponLayerPlays&& weaponLayerPlays)
    {
        play(0);
        int left = 0;
        for (int version = 1; version < kAttackVersions; ++version)
        {
            if (weaponLayerPlays())
                play(version);
            else
                ++left;
        }
        return left;
    }
}
