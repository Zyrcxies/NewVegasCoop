#pragma once

// Who an enemy goes for, among the players (host; Alex, October 2026: in fights with several players, enemies stuck to
// whoever shot them first). The game picks its own player over a friend's character whatever happens, so the host's
// game points each enemy a player hurt at the biggest threat: who hurt it most lately (fading), nearer ones a bit more,
// downed ones not at all. It changes its mind only when another player is clearly ahead, so it doesn't flicker.

#include <cstdint>
#include <map>
#include <vector>

namespace coop
{
    class ThreatTable
    {
    public:
        static constexpr float kFadeMs = 8000;        // damage counts less with age: gone after this
        static constexpr float kNearBonus = 15;       // a player right next to it counts as this much damage...
        static constexpr float kNearRange = 1500;     // ...down to none at this distance (game units)
        static constexpr float kSwitchMargin = 1.25f; // another player must be this much ahead to be switched to
        static constexpr uint32_t kForgetMs = 20000;  // an enemy nobody hurt for this long is left to the game

        struct Player
        {
            uint8_t id;
            float distance;  // from the enemy
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

        // Which player it should go for now (-1: none of them, e.g. all downed or out of reach: leave it to the game).
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
                best = e.target;  // (not clearly ahead: stay)
            e.target = best;
            return best;
        }

        // Enemies nobody hurt for kForgetMs (and dead ones, through Forget), to be left to the game.
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
