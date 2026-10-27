#pragma once

// Quests on a friend's game (step 7.1): what changed between our copy of a quest and the host's, for the messages
// the game itself would have shown (its quest scripts don't run here). Game-independent: tested in tests/.

#include "net/protocol.h"

#include <cstdint>
#include <string>
#include <vector>

namespace coop
{
    constexpr uint8_t kQuestRunning = 0x01;
    constexpr uint8_t kQuestCompleted = 0x02;
    constexpr uint8_t kQuestShown = 0x20;
    constexpr uint8_t kQuestFailed = 0x40;

    struct QuestNews
    {
        bool added = false;      // now in the Pip-Boy
        bool completed = false;
        bool failed = false;
        std::vector<uint32_t> shown;      // objectives newly shown
        std::vector<uint32_t> finished;   // objectives newly completed
    };

    QuestNews CompareQuest(const net::QuestState& before, const net::QuestState& after);

    // One of the game's own files (the base game, its DLC and packs; any letter case). Only their quests are the
    // host's to run: other mods' quests (MCM's menu, for one) keep running on a friend's game (two-PC test
    // 2026-10-15: the laptop crashed joining once MCM's quest script was detached).
    bool IsGameFile(const std::string& name);
    // Whether a form id's file is one of those, by the load order's file names.
    bool FromGameFile(uint32_t formId, const std::vector<std::string>& loadOrder);
    // Whether the host runs the quests of a form id's file: the game's own, and the mods the host lists (questMods,
    // any letter case; step 8.6: quest mods work without a patch each).
    // One of the DLCs' quests that's a mechanic on the player (Dead Money's bomb collar, its Cloud): it runs in every
    // game, each for its own player, and isn't the host's to share.
    bool IsPerPlayerQuest(uint32_t formId, const std::vector<std::string>& loadOrder);
    bool HostRunsQuestsOf(uint32_t formId, const std::vector<std::string>& loadOrder,
        const std::vector<std::string>& questMods);
}
