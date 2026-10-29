#include "world.h"

#include <algorithm>
#include <cmath>

// The arena run: waves from run.json, a boon to pick between them, a boss at the end, a score.
// The server's (or the host's, or the only player's): clients see it in the snapshots.

namespace arena
{
    uint32_t ArenaWorld::runRandom()
    {
        // xorshift: the same seed plays the same run
        uint32_t x = run.seed ? run.seed : 1u;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        run.seed = x;
        return x;
    }

    void ArenaWorld::sumBoons(Fighter& f) const
    {
        f.boon = Fighter::Boons{};

        if (not content)
            return;

        for (const auto& id : f.boons)
        {
            auto it = content->upgrades.find(id);

            if (it == content->upgrades.end())
                continue;

            const UpgradeDef& u = it->second;
            f.boon.maxHealth += u.maxHealth;
            f.boon.damage += u.damage;
            f.boon.dashCharges += u.dashCharges;
            f.boon.manaCost += u.manaCost;
            f.boon.manaRegen += u.manaRegen;
            f.boon.staminaRegen += u.staminaRegen;
            f.boon.speed += u.speed;
            f.boon.cooldown += u.cooldown;
            f.boon.lifeOnHit += u.lifeOnHit;
            f.boon.statusStacks += u.statusStacks;
        }
    }

    void ArenaWorld::startRun(uint32_t seed)
    {
        const uint32_t s = seed ? seed : 1u;
        run = RunState{};
        run.seed = s;
        run.waves = static_cast<int>(content->run.waves.size());

        // A fresh arena (the run's own), nobody in it but the players, as new
        useRunArena = true;
        buildArena();
        projectiles.clear();
        zones.clear();
        pendingHits.clear();
        removeEnemies();
        placePlayers();

        for (auto& f : fighters)
        {
            if (f.team != Team::Player)
                continue;

            f.boons.clear();
            f.offer.clear();
            sumBoons(f);
            respawn(f);
        }

        event("run_start", content->run.centre, 1.0f);
        beginWave(1);
    }

    void ArenaWorld::stopRun()
    {
        run = RunState{};

        // Back to the training arena
        if (useRunArena)
        {
            useRunArena = false;
            buildArena();
            projectiles.clear();
            zones.clear();
            placePlayers();

            for (auto& f : fighters)
                if (f.team == Team::Player)
                    respawn(f);
        }

        for (auto& f : fighters)
        {
            f.offer.clear();
            f.boons.clear();
            sumBoons(f);
        }
    }

    void ArenaWorld::skipToWave(int wave)
    {
        if (not inRun())
            return;

        removeEnemies();
        run.bossId = 0;
        beginWave(std::clamp(wave, 1, std::max(1, run.waves)));
    }

    void ArenaWorld::beginWave(int wave)
    {
        const RunDef& def = content->run;

        if (wave < 1 or wave > static_cast<int>(def.waves.size()))
            return;

        run.wave = wave;
        run.phase = RunPhase::Warn;
        run.timer = def.warn;
        run.pending.clear();

        // A fresh arena for each wave after the first: what was broken is whole again, everyone back in place
        if (wave > 1 and def.resetEachWave)
        {
            buildArena();
            projectiles.clear();
            zones.clear();
            pendingHits.clear();

            for (auto& f : fighters)
            {
                if (f.team != Team::Player or not f.alive())
                    continue;

                f.body.position = f.spawnPosition;
                f.body.velocity = glm::vec3(0.0f);
                f.push = glm::vec3(0.0f);
                f.yaw = f.spawnYaw;
            }

            event("arena_reset", def.centre, 1.0f);
        }

        // The ones down get back up for the new wave
        int players = 0;

        for (auto& f : fighters)
        {
            if (f.team != Team::Player)
                continue;

            ++players;

            if (not f.alive())
                respawn(f);
        }

        // More of them with more players
        const float scale = 1.0f + def.coopScale * static_cast<float>(std::max(0, players - 1));
        const WaveDef& w = def.waves[static_cast<size_t>(wave - 1)];

        auto spawnPoint = [&](float near, float far) {
            const float a = static_cast<float>(runRandom() % 3600) * 0.1f * 0.0174533f;
            const float r = def.spawnRadius * (near + (far - near) * static_cast<float>(runRandom() % 1000) / 1000.0f);
            glm::vec3 p = def.centre + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * r;
            p.x = std::clamp(p.x, 2.0f, static_cast<float>(grid.width()) - 3.0f);
            p.z = std::clamp(p.z, 2.0f, static_cast<float>(grid.depth()) - 3.0f);

            // On the ground there
            const RayHit down = raycast(grid, content->blockTable, glm::vec3(p.x, static_cast<float>(grid.height()) - 1.5f, p.z), glm::vec3(0, -1, 0), static_cast<float>(grid.height()));
            p.y = down.hit ? static_cast<float>(down.cell.y) + 1.0f : def.centre.y;
            return p;
        };

        for (const auto& [enemy, range] : w.enemies)
        {
            const int span = std::max(0, range.y - range.x);
            const int count = static_cast<int>(std::lround(static_cast<float>(range.x + static_cast<int>(runRandom() % static_cast<uint32_t>(span + 1))) * scale));

            for (int k = 0; k < count; ++k)
                run.pending.push_back({enemy, spawnPoint(0.6f, 1.0f), false});
        }

        if (not w.boss.empty())
            run.pending.push_back({w.boss, spawnPoint(0.7f, 0.8f), true});

        for (const auto& s : run.pending)
            event(s.boss ? "boss_warn" : "spawn_warn", s.position, def.warn);
    }

    void ArenaWorld::offerBoons()
    {
        const RunDef& def = content->run;

        for (auto& f : fighters)
        {
            if (f.team != Team::Player)
                continue;

            // The ones not picked to their most yet, shuffled
            std::vector<std::string> open;
            for (const auto& [id, u] : content->upgrades)
                if (std::count(f.boons.begin(), f.boons.end(), id) < u.most)
                    open.push_back(id);

            for (size_t i = open.size(); i > 1; --i)
                std::swap(open[i - 1], open[runRandom() % i]);

            open.resize(std::min(open.size(), static_cast<size_t>(def.choices)));
            f.offer = open;
        }
    }

    void ArenaWorld::takeBoon(Fighter& f, const std::string& id)
    {
        const float healthBefore = f.boon.maxHealth;
        f.boons.push_back(id);
        f.offer.clear();
        sumBoons(f);

        // More health: there at once
        const float more = f.boon.maxHealth - healthBefore;
        f.maxHealth += more;
        f.health = std::min(f.maxHealth, f.health + more);

        event("boon", chest(f), 1.0f, f.id);
    }

    void ArenaWorld::stepRun(const std::map<int, InputFrame>& inputs)
    {
        const RunDef& def = content->run;
        const float dt = STEP_SECONDS;

        auto pickOf = [&](const Fighter& f) {
            auto it = inputs.find(f.id);
            return it != inputs.end() ? it->second.pick : 0;
        };

        if (run.phase != RunPhase::Victory and run.phase != RunPhase::Defeat)
            run.elapsed += dt;

        int players = 0, standing = 0;
        for (const auto& f : fighters)
            if (f.team == Team::Player)
            {
                ++players;
                standing += f.alive() ? 1 : 0;
            }

        switch (run.phase)
        {
            case RunPhase::Off:
            {
                // Asked for (a dedicated server): anyone's START begins one
                for (const auto& f : fighters)
                {
                    if (f.team == Team::Player and pickOf(f) == InputFrame::START)
                    {
                        startRun(runRandom() ^ static_cast<uint32_t>(steps * 2654435761u + 1u));
                        return;
                    }
                }
                break;
            }

            case RunPhase::Warn:
            {
                run.timer -= dt;

                if (run.timer > 0.0f)
                    break;

                // Out they come, facing the middle
                for (const auto& s : run.pending)
                {
                    glm::vec3 toCentre = def.centre - s.position;
                    toCentre.y = 0.0f;
                    const int id = spawnEnemy(s.enemy, s.position, glm::length(toCentre) > 0.1f ? glm::degrees(std::atan2(toCentre.x, toCentre.z)) : 0.0f);

                    if (s.boss)
                        run.bossId = id;

                    event(s.boss ? "boss_spawn" : "spawn", s.position, 1.0f, id);
                }

                run.pending.clear();
                run.phase = RunPhase::Fighting;
                break;
            }

            case RunPhase::Fighting:
            {
                run.enemiesLeft = 0;
                for (const auto& f : fighters)
                    run.enemiesLeft += f.team == Team::Enemy and f.alive() and not f.gone ? 1 : 0;

                if (players > 0 and standing == 0)
                {
                    run.phase = RunPhase::Defeat;
                    event("run_defeat", def.centre, static_cast<float>(run.wave));
                    break;
                }

                if (run.enemiesLeft > 0)
                    break;

                run.score += def.waveScore * run.wave;
                run.bossId = 0;

                if (run.wave >= run.waves)
                {
                    // The last one: a bonus for a quick run
                    run.score += static_cast<int>(std::lround(static_cast<float>(def.timeBonus) * std::max(0.0f, 1.0f - run.elapsed / std::max(1.0f, def.timePar))));
                    run.phase = RunPhase::Victory;
                    event("run_victory", def.centre, static_cast<float>(run.score));
                    break;
                }

                event("wave_clear", def.centre, static_cast<float>(run.wave));
                offerBoons();
                run.phase = RunPhase::Choosing;
                run.timer = def.choose;
                break;
            }

            case RunPhase::Choosing:
            {
                run.timer -= dt;
                bool waiting = false;

                for (auto& f : fighters)
                {
                    if (f.team != Team::Player or f.offer.empty())
                        continue;

                    const int pick = pickOf(f);

                    if (pick >= 1 and pick <= static_cast<int>(f.offer.size()))
                        takeBoon(f, f.offer[static_cast<size_t>(pick - 1)]);
                    else if (run.timer <= 0.0f)
                        takeBoon(f, f.offer.front());   // out of time: the first one
                    else
                        waiting = true;
                }

                if (not waiting)
                {
                    run.phase = RunPhase::Breather;
                    run.timer = def.breather;
                }
                break;
            }

            case RunPhase::Breather:
            {
                run.timer -= dt;

                if (run.timer <= 0.0f)
                    beginWave(run.wave + 1);
                break;
            }

            case RunPhase::Victory:
            case RunPhase::Defeat:
            {
                // Anyone: again
                for (const auto& f : fighters)
                {
                    if (f.team == Team::Player and pickOf(f) == InputFrame::START)
                    {
                        startRun(runRandom() ^ static_cast<uint32_t>(steps * 2654435761u));
                        return;
                    }
                }
                break;
            }
        }
    }
}
