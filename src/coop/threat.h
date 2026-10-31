// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include <cstdint>
#include <map>
#include <vector>

namespace coop
{
    class ThreatTable
    {
    public:
        static constexpr float kFadeMs = 8000;
        static constexpr float kNearBonus = 15;
        static constexpr float kNearRange = 1500;
        static constexpr float kSwitchMargin = 1.25f;
        static constexpr uint32_t kForgetMs = 20000;

        struct Player
        {
            uint8_t id;
            float distance;
            bool downed;
        };

        void Hurt(uint32_t enemy, uint8_t player, float damage, uint32_t now)
        {
            if (damage <= 0)
                return;
            Enemy& e = m_enemies[enemy];
            e.hurtAt = now;
            for (Hurt_& h : e.hurts)
                if (h.player == player)
                {
                    h.damage = Faded(h, now) + damage;
                    h.at = now;
                    return;
                }
            e.hurts.push_back({player, damage, now});
        }

        int Pick(uint32_t enemy, const std::vector<Player>& players, uint32_t now)
        {
            auto it = m_enemies.find(enemy);
            if (it == m_enemies.end())
                return -1;
            Enemy& e = it->second;
            int best = -1;
            float bestScore = 0, currentScore = -1;
            for (const Player& p : players)
            {
                if (p.downed)
                    continue;
                float score = Score(e, p, now);
                if (p.id == e.target)
                    currentScore = score;
                if (best < 0 || score > bestScore)
                {
                    best = p.id;
                    bestScore = score;
                }
            }
            if (best >= 0 && currentScore >= 0 && best != e.target && bestScore < currentScore * kSwitchMargin)
                best = e.target;
            e.target = best;
            return best;
        }

        std::vector<uint32_t> Stale(uint32_t now) const
        {
            std::vector<uint32_t> out;
            for (const auto& [enemy, e] : m_enemies)
                if (now - e.hurtAt > kForgetMs)
                    out.push_back(enemy);
            return out;
        }
        void Forget(uint32_t enemy) { m_enemies.erase(enemy); }
        std::vector<uint32_t> Enemies() const
        {
            std::vector<uint32_t> out;
            for (const auto& [enemy, e] : m_enemies)
                out.push_back(enemy);
            return out;
        }
        void Clear() { m_enemies.clear(); }

    private:
        struct Hurt_
        {
            uint8_t player;
            float damage;
            uint32_t at;
        };
        struct Enemy
        {
            std::vector<Hurt_> hurts;
            uint32_t hurtAt = 0;
            int target = -1;
        };

        static float Faded(const Hurt_& h, uint32_t now)
        {
            float age = static_cast<float>(now - h.at);
            return age >= kFadeMs ? 0 : h.damage * (1 - age / kFadeMs);
        }

        static float Score(const Enemy& e, const Player& p, uint32_t now)
        {
            float score = 0;
            for (const Hurt_& h : e.hurts)
                if (h.player == p.id)
                    score += Faded(h, now);
            if (p.distance < kNearRange)
                score += kNearBonus * (1 - p.distance / kNearRange);
            return score;
        }

        std::map<uint32_t, Enemy> m_enemies;
    };
}
