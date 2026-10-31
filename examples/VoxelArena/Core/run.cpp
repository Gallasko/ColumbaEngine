#include "world.h"

#include <algorithm>
#include <cmath>

// The arena run: timed waves with enemies coming in all the while, shards they drop (money and XP),
// a shop between waves, bosses on some waves, a score. The server's (or the host's, or the only
// player's): clients see it in the snapshots.

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

    bool ArenaWorld::owns(const Fighter& f, const std::string& id) const
    {
        // Outside a run (and for enemies) everything is there
        return not inRun() or f.team != Team::Player or std::find(f.owned.begin(), f.owned.end(), id) != f.owned.end();
    }

    int ArenaWorld::xpFor(int level) const
    {
        return content->run.xpBase + content->run.xpStep * std::max(0, level - 1);
    }

    float ArenaWorld::waveDuration(int wave) const
    {
        const RunDef& def = content->run;
        const float t = def.waves > 1 ? static_cast<float>(wave - 1) / static_cast<float>(def.waves - 1) : 0.0f;
        return def.duration.x + (def.duration.y - def.duration.x) * t;
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
            f.boon.armor += u.armor;
            f.boon.crit += u.crit;
            f.boon.pickupRange += u.pickupRange;
            f.boon.regen += u.regen;
        }
    }

    // ---- Starting, stopping ---------------------------------------------------------------------

    void ArenaWorld::startRun(uint32_t seed)
    {
        const RunDef& def = content->run;
        run = RunState{};
        run.seed = seed ? seed : 1u;
        run.waves = def.waves;

        // A fresh arena (the run's own), nobody in it but the players, as new, with what a run starts with
        useRunArena = true;
        buildArena();
        projectiles.clear();
        zones.clear();
        pickups.clear();
        pendingHits.clear();
        removeEnemies();
        placePlayers();

        // inRun() is true from here: owns() asks what is owned
        run.phase = RunPhase::Fighting;

        for (auto& f : fighters)
        {
            if (f.team != Team::Player)
                continue;

            f.boons.clear();
            f.owned = def.startOwned;

            if (content->weapons.count(def.startWeapon))
            {
                f.owned.push_back(def.startWeapon);
                f.weapon = def.startWeapon;
            }

            f.shards = f.xp = f.levelUps = f.rerolls = 0;
            f.level = 1;
            f.shop.clear();
            f.levelOffer.clear();
            f.ready = false;
            sumBoons(f);
            respawn(f);
        }

        event("run_start", def.centre, 1.0f);

        // Each player picks a weapon first (when there is a choice): the first wave waits for everyone
        if (def.chooseWeapon and content->weaponOrder.size() > 1)
        {
            run.phase = RunPhase::Armory;
            run.wave = 1;
            return;
        }

        beginWave(1);
    }

    void ArenaWorld::chooseWeapon(Fighter& f, const std::string& id)
    {
        if (not content->weapons.count(id))
            return;

        // One weapon for the run: the one in hand before (the start kit's) goes
        f.owned.erase(std::remove_if(f.owned.begin(), f.owned.end(), [&](const std::string& o) { return content->weapons.count(o) > 0; }), f.owned.end());
        f.owned.push_back(id);
        f.weapon = id;
        f.ready = true;
        event("weapon_pick", chest(f), 1.0f, f.id);
    }

    void ArenaWorld::stopRun()
    {
        run = RunState{};
        pickups.clear();

        for (auto& f : fighters)
        {
            f.boons.clear();
            f.owned.clear();
            f.shop.clear();
            f.levelOffer.clear();
            f.shards = f.xp = f.levelUps = 0;
            f.level = 1;
            sumBoons(f);
        }

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
    }

    void ArenaWorld::skipToWave(int wave)
    {
        if (not inRun())
            return;

        removeEnemies();
        run.bossId = 0;
        beginWave(std::clamp(wave, 1, std::max(1, run.waves)));
    }

    // ---- Waves ----------------------------------------------------------------------------------

    glm::vec3 ArenaWorld::edgePoint(float away)
    {
        const RunDef& def = content->run;
        glm::vec3 best = def.centre;
        float bestDistance = -1.0f;

        // A few tries round the edge: the first far enough from the players, else the furthest
        for (int k = 0; k < 6; ++k)
        {
            const float a = static_cast<float>(runRandom() % 3600) * 0.1f * 0.0174533f;
            const float r = def.spawnRadius * (0.75f + 0.25f * static_cast<float>(runRandom() % 1000) / 1000.0f);
            glm::vec3 p = def.centre + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * r;
            p.x = std::clamp(p.x, 2.0f, static_cast<float>(grid.width()) - 3.0f);
            p.z = std::clamp(p.z, 2.0f, static_cast<float>(grid.depth()) - 3.0f);

            // On the ground there
            const RayHit down = raycast(grid, content->blockTable, glm::vec3(p.x, static_cast<float>(grid.height()) - 1.5f, p.z), glm::vec3(0, -1, 0), static_cast<float>(grid.height()));
            p.y = down.hit ? static_cast<float>(down.cell.y) + 1.0f : def.centre.y;

            float nearest = 1e9f;
            for (const auto& f : fighters)
                if (f.team == Team::Player and f.alive())
                    nearest = std::min(nearest, glm::length(p - f.body.position));

            if (nearest > bestDistance)
            {
                bestDistance = nearest;
                best = p;
            }

            if (nearest >= away)
                break;
        }

        return best;
    }

    void ArenaWorld::beginWave(int wave)
    {
        const RunDef& def = content->run;

        if (wave < 1 or wave > def.waves)
            return;

        run.wave = wave;
        run.phase = RunPhase::Fighting;
        run.timer = waveDuration(wave);
        run.pending.clear();
        run.spawnDue = 0.0f;
        run.bossId = 0;
        pickups.clear();

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
        for (auto& f : fighters)
        {
            if (f.team != Team::Player)
                continue;

            f.ready = false;

            if (not f.alive())
                respawn(f);
        }

        // A boss wave: it comes first, from far off, and the wave lasts until it falls
        auto boss = def.bosses.find(wave);
        run.bossWave = boss != def.bosses.end() and content->enemies.count(boss->second);

        if (run.bossWave)
        {
            run.pending.push_back({boss->second, edgePoint(10.0f), 2.0f, true, false});
            event("boss_warn", run.pending.back().position, 2.0f);
        }

        event("wave_start", def.centre, static_cast<float>(wave));
    }

    void ArenaWorld::endWave()
    {
        const RunDef& def = content->run;

        // Whoever is left goes, the shards still lying about go to the nearest player
        for (const auto& f : fighters)
            if (f.team == Team::Enemy and f.alive())
                event("despawn", chest(f), 1.0f);

        removeEnemies();
        run.pending.clear();
        run.bossId = 0;
        run.enemiesLeft = 0;

        for (const auto& p : pickups)
        {
            Fighter* nearest = nullptr;
            float best = 1e9f;

            for (auto& f : fighters)
                if (f.team == Team::Player and glm::length(f.body.position - p.position) < best)
                {
                    best = glm::length(f.body.position - p.position);
                    nearest = &f;
                }

            if (nearest)
                collect(*nearest, p.value);
        }

        pickups.clear();
        run.score += def.waveScore * run.wave;

        if (run.wave >= run.waves)
        {
            run.score += def.victoryScore;

            // Two players or more: a final duel first
            const auto players = std::count_if(fighters.begin(), fighters.end(), [](const Fighter& f) { return f.team == Team::Player; });
            if (def.duel.enabled and players >= 2)
            {
                beginDuel();
                return;
            }

            run.phase = RunPhase::Victory;
            event("run_victory", def.centre, static_cast<float>(run.score));
            return;
        }

        // The shop: each player's own
        run.phase = RunPhase::Shop;

        for (auto& f : fighters)
        {
            if (f.team != Team::Player)
                continue;

            f.ready = false;
            f.rerolls = 0;
            rollShop(f);
            rollLevel(f);
        }

        event("wave_clear", def.centre, static_cast<float>(run.wave));
    }

    void ArenaWorld::beginDuel()
    {
        const RunDef& def = content->run;

        run.phase = RunPhase::Duel;
        run.timer = def.duel.duration;
        run.warmup = def.duel.countdown;
        run.champion = 0;
        run.bossWave = false;
        run.bossId = 0;
        run.pending.clear();

        // The arena anew, nothing left in it but the players
        buildArena();
        projectiles.clear();
        zones.clear();
        pendingHits.clear();
        pickups.clear();
        removeEnemies();

        // Everyone back up, in a ring around the middle, facing it
        std::vector<Fighter*> players;
        for (auto& f : fighters)
            if (f.team == Team::Player)
                players.push_back(&f);

        for (size_t k = 0; k < players.size(); ++k)
        {
            Fighter& f = *players[k];
            const float angle = static_cast<float>(k) * 2.0f * 3.14159265f / static_cast<float>(players.size());
            f.spawnPosition = def.centre + glm::vec3(std::sin(angle), 0.0f, std::cos(angle)) * def.duel.radius;
            const glm::vec3 toCentre = def.centre - f.spawnPosition;
            f.spawnYaw = glm::degrees(std::atan2(toCentre.x, toCentre.z));
            f.ready = false;
            f.statuses.clear();
            respawn(f);
        }

        event("duel_start", def.centre, static_cast<float>(players.size()));
    }

    // ---- Shards ---------------------------------------------------------------------------------

    void ArenaWorld::spawnShards(const Fighter& from, int value)
    {
        if (value <= 0)
            return;

        // A few shards, popping out
        const int count = std::clamp(value, 1, 4);

        for (int k = 0; k < count; ++k)
        {
            Pickup p;
            p.id = nextPickup++;
            p.value = value / count + (k < value % count ? 1 : 0);
            p.position = chest(from);
            const float a = static_cast<float>(runRandom() % 628) * 0.01f;
            p.velocity = glm::vec3(std::cos(a) * 2.0f, 4.0f, std::sin(a) * 2.0f);
            pickups.push_back(p);
        }

        // Too many lying about: the oldest go into the next one
        while (pickups.size() > 160)
        {
            pickups[1].value += pickups[0].value;
            pickups.erase(pickups.begin());
        }
    }

    void ArenaWorld::collect(Fighter& f, int value)
    {
        f.shards += value;
        f.xp += value;

        while (f.xp >= xpFor(f.level))
        {
            f.xp -= xpFor(f.level);
            ++f.level;
            ++f.levelUps;
            event("level_up", chest(f), static_cast<float>(f.level), f.id);
        }
    }

    void ArenaWorld::stepPickups()
    {
        const float dt = STEP_SECONDS;

        for (size_t i = 0; i < pickups.size();)
        {
            Pickup& p = pickups[i];
            p.age += dt;

            // Who it goes to: the nearest player that has it in range (once it has popped out)
            if (p.target == 0 and p.age > 0.3f)
            {
                float best = 1e9f;

                for (const auto& f : fighters)
                {
                    if (f.team != Team::Player or not f.alive())
                        continue;

                    const float d = glm::length(chest(f) - p.position);

                    if (d <= content->run.pickupRange + f.boon.pickupRange and d < best)
                    {
                        best = d;
                        p.target = f.id;
                    }
                }
            }

            Fighter* to = p.target ? find(p.target) : nullptr;

            if (to and to->alive())
            {
                // Faster as it comes
                const glm::vec3 d = chest(*to) - p.position;
                const float distance = glm::length(d);

                if (distance < 0.6f)
                {
                    const int value = p.value;
                    const glm::vec3 at = p.position;
                    pickups.erase(pickups.begin() + static_cast<std::ptrdiff_t>(i));
                    collect(*to, value);
                    event("pickup", at, static_cast<float>(value), to->id);
                    continue;
                }

                const float speed = 6.0f + 18.0f * std::min(1.0f, p.age);
                p.velocity = d / distance * speed;
                p.position += p.velocity * std::min(dt, distance / speed);
            }
            else
            {
                p.target = 0;

                // Popped out, then down to the ground and still
                p.velocity.y -= 18.0f * dt;
                p.velocity.x *= 0.92f;
                p.velocity.z *= 0.92f;
                glm::vec3 next = p.position + p.velocity * dt;
                const float foot = next.y - 0.15f;

                if (foot < 0.0f or grid.isSolid(content->blockTable, static_cast<int>(std::floor(next.x)), static_cast<int>(std::floor(foot)), static_cast<int>(std::floor(next.z))))
                {
                    next.y = std::floor(std::max(foot, 0.0f)) + 1.15f;
                    p.velocity = glm::vec3(0.0f);
                }

                p.position = next;
            }

            ++i;
        }
    }

    // ---- The shop and level ups -----------------------------------------------------------------

    void ArenaWorld::rollShop(Fighter& f)
    {
        const RunDef& def = content->run;
        const float growth = 1.0f + def.priceGrowth * static_cast<float>(std::max(0, run.wave - 1));

        // Locked offers stay; the rest is rolled again
        std::vector<Fighter::ShopOffer> kept;
        for (const auto& o : f.shop)
            if (o.locked and not o.bought)
                kept.push_back(o);

        auto offered = [&](const std::string& id) {
            return std::any_of(kept.begin(), kept.end(), [&](const Fighter::ShopOffer& o) { return o.id == id; });
        };

        // What can be offered: stat items not at their most, unlocks not owned, nothing twice
        std::vector<std::pair<std::string, int>> open;

        for (const auto& [id, u] : content->upgrades)
            if (std::count(f.boons.begin(), f.boons.end(), id) < u.most and not offered(id))
                open.emplace_back(id, static_cast<int>(std::lround(static_cast<float>(u.price) * growth)));

        // The weapon was chosen at the start: no weapons, and only the skills of the one in hand
        const auto weapon = content->weapons.find(f.weapon);
        const std::string type = weapon != content->weapons.end() ? weapon->second.type : "";

        auto forSale = [&](const std::string& id) {
            if (def.chooseWeapon and content->weapons.count(id))
                return false;

            if (auto a = content->abilities.find(id); a != content->abilities.end() and not a->second.weapons.empty())
                return std::find(a->second.weapons.begin(), a->second.weapons.end(), type) != a->second.weapons.end();

            return true;
        };

        for (const auto& [id, price] : def.unlocks)
            if (not owns(f, id) and not offered(id) and forSale(id))
                open.emplace_back(id, static_cast<int>(std::lround(static_cast<float>(price) * growth)));

        for (size_t i = open.size(); i > 1; --i)
            std::swap(open[i - 1], open[runRandom() % i]);

        f.shop = kept;

        for (const auto& [id, price] : open)
        {
            if (static_cast<int>(f.shop.size()) >= def.shopSlots)
                break;

            f.shop.push_back({id, price, false, false});
        }
    }

    void ArenaWorld::rollLevel(Fighter& f)
    {
        f.levelOffer.clear();

        if (f.levelUps <= 0)
            return;

        std::vector<std::string> open;
        for (const auto& [id, u] : content->upgrades)
            if (std::count(f.boons.begin(), f.boons.end(), id) < u.most)
                open.push_back(id);

        for (size_t i = open.size(); i > 1; --i)
            std::swap(open[i - 1], open[runRandom() % i]);

        open.resize(std::min(open.size(), static_cast<size_t>(content->run.levelChoices)));
        f.levelOffer = open;
    }

    void ArenaWorld::takeBoon(Fighter& f, const std::string& id)
    {
        const float healthBefore = f.boon.maxHealth;
        f.boons.push_back(id);
        sumBoons(f);

        // More health: there at once
        const float more = f.boon.maxHealth - healthBefore;
        f.maxHealth += more;
        f.health = std::min(f.maxHealth, f.health + more);
    }

    int ArenaWorld::rerollPrice(const Fighter& f) const
    {
        const RunDef& def = content->run;
        return def.rerollPrice + def.rerollStep * f.rerolls + run.wave / 2;
    }

    void ArenaWorld::shopPick(Fighter& f, int pick)
    {
        const int offers = static_cast<int>(f.shop.size());

        if (pick >= InputFrame::BUY and pick < InputFrame::BUY + offers)
        {
            auto& o = f.shop[static_cast<size_t>(pick - InputFrame::BUY)];

            if (o.bought or f.shards < o.price)
                return;

            f.shards -= o.price;
            o.bought = true;
            o.locked = false;

            if (content->upgrades.count(o.id))
            {
                takeBoon(f, o.id);
            }
            else
            {
                f.owned.push_back(o.id);

                // A weapon bought is in hand at once
                if (content->weapons.count(o.id))
                    f.weapon = o.id;
            }

            event("buy", chest(f), 1.0f, f.id);
        }
        else if (pick >= InputFrame::LOCK and pick < InputFrame::LOCK + offers)
        {
            auto& o = f.shop[static_cast<size_t>(pick - InputFrame::LOCK)];

            if (not o.bought)
                o.locked = not o.locked;
        }
        else if (pick == InputFrame::REROLL)
        {
            const int price = rerollPrice(f);

            if (f.shards < price)
                return;

            f.shards -= price;
            ++f.rerolls;
            rollShop(f);
        }
        else if (pick >= InputFrame::LEVEL and pick < InputFrame::LEVEL + static_cast<int>(f.levelOffer.size()))
        {
            takeBoon(f, f.levelOffer[static_cast<size_t>(pick - InputFrame::LEVEL)]);
            --f.levelUps;
            rollLevel(f);
            event("boon", chest(f), 1.0f, f.id);
        }
        else if (pick == InputFrame::START)
        {
            f.ready = not f.ready;
        }
    }

    // ---- Each step ------------------------------------------------------------------------------

    void ArenaWorld::stepRun(const std::map<int, InputFrame>& inputs)
    {
        const RunDef& def = content->run;
        const float dt = STEP_SECONDS;

        auto pickOf = [&](const Fighter& f) {
            auto it = inputs.find(f.id);
            return it != inputs.end() ? it->second.pick : 0;
        };

        // Anyone's START: a run from the start
        auto startAgain = [&]() {
            for (const auto& f : fighters)
            {
                if (f.team == Team::Player and pickOf(f) == InputFrame::START)
                {
                    startRun(runRandom() ^ static_cast<uint32_t>(steps * 2654435761u + 1u));
                    return;
                }
            }
        };

        if (run.phase == RunPhase::Fighting or run.phase == RunPhase::Shop or run.phase == RunPhase::Duel)
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
                // Asked for (a dedicated server has no title screen)
                startAgain();
                break;

            case RunPhase::Fighting:
            {
                stepPickups();

                if (players > 0 and standing == 0)
                {
                    run.phase = RunPhase::Defeat;
                    pickups.clear();
                    event("run_defeat", def.centre, static_cast<float>(run.wave));
                    break;
                }

                run.timer = std::max(0.0f, run.timer - dt);

                // The ones warned of come out, facing the middle
                for (size_t i = 0; i < run.pending.size();)
                {
                    auto& s = run.pending[i];
                    s.in -= dt;

                    if (s.in > 0.0f)
                    {
                        ++i;
                        continue;
                    }

                    glm::vec3 toCentre = def.centre - s.position;
                    toCentre.y = 0.0f;
                    const int id = spawnEnemy(s.enemy, s.position, glm::length(toCentre) > 0.1f ? glm::degrees(std::atan2(toCentre.x, toCentre.z)) : 0.0f);

                    if (Fighter* e = find(id))
                    {
                        if (s.boss)
                            run.bossId = id;

                        // Tougher as the run goes on
                        e->maxHealth *= 1.0f + def.healthGrowth * static_cast<float>(run.wave - 1);
                        e->health = e->maxHealth;

                        // An elite: tougher, harder hitting (applyHit), worth more
                        if (s.elite)
                        {
                            e->elite = true;
                            e->maxHealth *= 2.5f;
                            e->health = e->maxHealth;
                            e->maxPoise *= 1.5f;
                            e->poise = e->maxPoise;
                        }
                    }

                    event(s.boss ? "boss_spawn" : "spawn", s.position, 1.0f, id);
                    run.pending.erase(run.pending.begin() + static_cast<std::ptrdiff_t>(i));
                }

                run.enemiesLeft = 0;
                for (const auto& f : fighters)
                    run.enemiesLeft += f.team == Team::Enemy and f.alive() and not f.gone ? 1 : 0;

                // More of them all the while, faster as the run goes on and with more players; half as many with a boss
                const float t = def.waves > 1 ? static_cast<float>(run.wave - 1) / static_cast<float>(def.waves - 1) : 0.0f;
                const float coop = 1.0f + def.coopScale * static_cast<float>(std::max(0, players - 1));
                const float rate = (def.spawnRate.x + (def.spawnRate.y - def.spawnRate.x) * t) * coop * (run.bossWave ? 0.5f : 1.0f);
                const int most = static_cast<int>(std::lround((static_cast<float>(def.maxAlive.x) + static_cast<float>(def.maxAlive.y - def.maxAlive.x) * t) * coop));

                run.spawnDue += rate * dt;

                while (run.spawnDue >= 1.0f)
                {
                    run.spawnDue -= 1.0f;

                    // None in the last moments of a wave, nor past the most there may be at once
                    if ((not run.bossWave and run.timer < 2.0f) or run.enemiesLeft + static_cast<int>(run.pending.size()) >= most)
                        continue;

                    // Who: one of the pools open at this wave, by weight
                    float total = 0.0f;
                    for (const auto& p : def.pools)
                        if (run.wave >= p.from and run.wave <= p.to and content->enemies.count(p.enemy))
                            total += p.weight;

                    if (total <= 0.0f)
                        break;

                    float roll = static_cast<float>(runRandom() % 10000) / 10000.0f * total;
                    std::string enemy;

                    for (const auto& p : def.pools)
                    {
                        if (run.wave < p.from or run.wave > p.to or not content->enemies.count(p.enemy))
                            continue;

                        enemy = p.enemy;
                        roll -= p.weight;

                        if (roll <= 0.0f)
                            break;
                    }

                    const bool elite = static_cast<float>(run.wave) >= def.eliteFrom and static_cast<float>(runRandom() % 1000) / 1000.0f < def.eliteChance;
                    run.pending.push_back({enemy, edgePoint(6.0f), def.warn, false, elite});
                    event(elite ? "elite_warn" : "spawn_warn", run.pending.back().position, def.warn);
                }

                // The wave ends with its time, a boss wave with its boss
                const Fighter* boss = run.bossId ? find(run.bossId) : nullptr;
                const bool bossDown = run.bossWave and run.bossId != 0 and (not boss or not boss->alive());

                if ((not run.bossWave and run.timer <= 0.0f) or bossDown)
                    endWave();
                break;
            }

            case RunPhase::Armory:
            {
                bool all = players > 0;

                for (auto& f : fighters)
                {
                    if (f.team != Team::Player)
                        continue;

                    const int pick = pickOf(f) - InputFrame::WEAPON;
                    if (pick >= 0 and pick < static_cast<int>(content->weaponOrder.size()))
                        chooseWeapon(f, content->weaponOrder[static_cast<size_t>(pick)]);

                    all = all and f.ready;
                }

                if (all)
                    beginWave(1);
                break;
            }

            case RunPhase::Shop:
            {
                bool all = players > 0;

                for (auto& f : fighters)
                {
                    if (f.team != Team::Player)
                        continue;

                    if (const int pick = pickOf(f))
                        shopPick(f, pick);

                    all = all and f.ready;
                }

                if (all)
                    beginWave(run.wave + 1);
                break;
            }

            case RunPhase::Duel:
            {
                // The countdown: nobody may hurt anybody yet (see hostile)
                if (run.warmup > 0.0f)
                {
                    run.warmup = std::max(0.0f, run.warmup - dt);
                    if (run.warmup <= 0.0f)
                        event("duel_fight", def.centre, 1.0f);
                    break;
                }

                run.timer = std::max(0.0f, run.timer - dt);

                // One life each: the last one standing, or when the time is up the most health left
                const Fighter* last = nullptr;
                for (const auto& f : fighters)
                    if (f.team == Team::Player and f.alive())
                        if (not last or f.health / std::max(1.0f, f.maxHealth) > last->health / std::max(1.0f, last->maxHealth))
                            last = &f;

                if (standing <= 1 or run.timer <= 0.0f)
                {
                    run.champion = last ? last->id : 0;
                    run.phase = RunPhase::Victory;
                    event("duel_end", last ? last->body.position : def.centre, static_cast<float>(run.champion));
                    event("run_victory", def.centre, static_cast<float>(run.score));
                }
                break;
            }

            case RunPhase::Victory:
            case RunPhase::Defeat:
                startAgain();
                break;
        }
    }
}
