#include "coop/quests.h"

#include <cstring>

namespace coop
{
    QuestNews CompareQuest(const net::QuestState& before, const net::QuestState& after)
    {
        QuestNews news;
        auto newly = [&](uint8_t flag) { return !(before.flags & flag) && (after.flags & flag); };
        news.added = newly(kQuestShown);
        news.completed = newly(kQuestCompleted);
        news.failed = newly(kQuestFailed);
        for (const net::QuestObjective& o : after.objectives)
        {
            uint8_t was = 0;
            for (const net::QuestObjective& b : before.objectives)
                if (b.id == o.id)
                    was = b.status;
            if ((o.status & 1) && !(was & 1))
                news.shown.push_back(o.id);
            if ((o.status & 2) && !(was & 2))
                news.finished.push_back(o.id);
        }
        return news;
    }

    bool IsGameFile(const std::string& name)
    {
        static const char* const kFiles[] = {"FalloutNV.esm", "DeadMoney.esm", "HonestHearts.esm", "OldWorldBlues.esm",
            "LonesomeRoad.esm", "GunRunnersArsenal.esm", "ClassicPack.esm", "MercenaryPack.esm", "TribalPack.esm",
            "CaravanPack.esm"};
        for (const char* f : kFiles)
            if (_stricmp(f, name.c_str()) == 0)
                return true;
        return false;
    }

    bool FromGameFile(uint32_t formId, const std::vector<std::string>& loadOrder)
    {
        size_t index = formId >> 24;
        return index < loadOrder.size() && IsGameFile(loadOrder[index]);
    }

    bool IsPerPlayerQuest(uint32_t formId, const std::vector<std::string>& loadOrder)
    {
        // The DLCs' quests that are a mechanic on the player rather than the story: their scripts run in every game,
        // each for its own player (Dead Money playthrough, 2026-10-16: a friend's bomb collar never beeped, its quest
        // script detached, until the host left and it ran).
        struct PerPlayer { const char* file; uint32_t id; };
        static const PerPlayer kQuests[] = {
            {"DeadMoney.esm", 0x006E93},  // NVDLC01BombCollarQuest: the collar's beeping countdown near radios
            {"DeadMoney.esm", 0x00E882},  // NVDLC01GlobalToxicDamage: the Cloud hurting the player
            {"DeadMoney.esm", 0x00C543},  // NVDLC01ToxicCloud
            {"DeadMoney.esm", 0x013F58},  // NVDLC01ElijahCollarBark: Elijah's warnings about the collar
        };
        size_t index = formId >> 24;
        if (index >= loadOrder.size())
            return false;
        for (const PerPlayer& q : kQuests)
            if ((formId & 0xFFFFFF) == q.id && _stricmp(loadOrder[index].c_str(), q.file) == 0)
                return true;
        return false;
    }

    bool HostRunsQuestsOf(uint32_t formId, const std::vector<std::string>& loadOrder,
        const std::vector<std::string>& questMods)
    {
        size_t index = formId >> 24;
        if (index >= loadOrder.size() || IsPerPlayerQuest(formId, loadOrder))
            return false;
        if (IsGameFile(loadOrder[index]))
            return true;
        for (const std::string& mod : questMods)
            if (_stricmp(mod.c_str(), loadOrder[index].c_str()) == 0)
                return true;
        return false;
    }
}
