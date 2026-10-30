#include "world.h"

#include <algorithm>
#include <cmath>

namespace arena
{
    namespace
    {
        constexpr float DEG = 3.14159265358979f / 180.0f;

        float wrapDegrees(float a)
        {
            a = std::fmod(a + 180.0f, 360.0f);
            if (a < 0.0f)
                a += 360.0f;
            return a - 180.0f;
        }

        float yawOf(const glm::vec3& dir)
        {
            return std::atan2(dir.x, dir.z) / DEG;
        }

        glm::vec3 flat(const glm::vec3& v)
        {
            return glm::vec3(v.x, 0.0f, v.z);
        }

        glm::vec3 rightOf(float yawDegrees)
        {
            const float r = yawDegrees * DEG;
            return glm::vec3(-std::cos(r), 0.0f, std::sin(r));
        }

        /** Moves v towards target by at most maxDelta (vector length). */
        glm::vec3 approach(const glm::vec3& v, const glm::vec3& target, float maxDelta)
        {
            const glm::vec3 d = target - v;
            const float len = glm::length(d);

            if (len <= maxDelta or len < 1e-6f)
                return target;

            return v + d * (maxDelta / len);
        }

        /** Yaw-rotated box against an upright axis-aligned box: separating axes on XZ, interval on Y. */
        bool boxHitsBody(const glm::vec3& centre, const glm::vec3& size, float yaw, const Body& body)
        {
            const float boxLo = centre.y - size.y * 0.5f, boxHi = centre.y + size.y * 0.5f;

            if (boxHi < body.position.y or boxLo > body.position.y + body.height)
                return false;

            const glm::vec2 c(centre.x, centre.z);
            const glm::vec2 b(body.position.x, body.position.z);
            const glm::vec3 r3 = rightOf(yaw), f3 = yawForward(yaw);
            const glm::vec2 r(r3.x, r3.z), f(f3.x, f3.z);
            const float hx = size.x * 0.5f, hz = size.z * 0.5f, hb = body.width * 0.5f;

            const glm::vec2 axes[4] = { {1, 0}, {0, 1}, r, f };

            for (const auto& L : axes)
            {
                const float ra = hx * std::fabs(glm::dot(r, L)) + hz * std::fabs(glm::dot(f, L));
                const float rb = hb * std::fabs(L.x) + hb * std::fabs(L.y);

                if (std::fabs(glm::dot(b - c, L)) > ra + rb)
                    return false;
            }

            return true;
        }
    }

    glm::vec3 yawForward(float yawDegrees)
    {
        const float r = yawDegrees * DEG;
        return glm::vec3(std::sin(r), 0.0f, std::cos(r));
    }

    glm::vec3 localToWorld(float yawDegrees, const glm::vec3& local)
    {
        return rightOf(yawDegrees) * local.x + glm::vec3(0, local.y, 0) + yawForward(yawDegrees) * local.z;
    }

    const char* actionName(Action a)
    {
        switch (a)
        {
            case Action::Free: return "free";
            case Action::Attack: return "attack";
            case Action::Dodge: return "dodge";
            case Action::Stagger: return "stagger";
            case Action::Land: return "land";
            case Action::AirDash: return "airdash";
            case Action::Dead: return "dead";
        }

        return "?";
    }

    // --------------------------------------------------------------------------------------------
    // Setup
    // --------------------------------------------------------------------------------------------

    void ArenaWorld::start(const Content& c, const Tuning& t, bool withLocalPlayer)
    {
        content = &c;
        tuning = t;
        fighters.clear();
        hits.clear();
        zones.clear();
        spellCache.clear();
        useRunArena = false;
        run = RunState{};
        time = 0.0f;
        steps = 0;

        buildArena();
        history.clear();
        pendingHits.clear();
        projectiles.clear();
        localPlayerId = 0;

        if (withLocalPlayer)
            localPlayerId = addPlayer(0);

        for (const auto& s : c.arena.spawns)
            spawnEnemy(s.enemy, s.position, s.yaw);
    }

    void ArenaWorld::setContent(const Content& c, const Tuning& t, bool rebuildArena)
    {
        content = &c;
        tuning = t;
        spellCache.clear();   // made from the old keywords

        if (rebuildArena)
            buildArena();
        else
            terrain.setTable(&content->blockTable);

        for (auto& f : fighters)
        {
            if (f.team == Team::Player)
            {
                f.maxHealth = t.player.health;
                f.maxPoise = t.player.poise;
                f.body.width = t.player.width;
                f.body.height = t.player.height;

                if (not c.weapons.count(f.weapon))
                    f.weapon = c.weapons.count(t.startWeapon) ? t.startWeapon : (c.weaponOrder.empty() ? "" : c.weaponOrder.front());
            }
            else if (auto it = c.enemies.find(f.type); it != c.enemies.end())
            {
                f.maxHealth = it->second.health;
                f.maxPoise = it->second.poise;
                f.body.width = it->second.width;
                f.body.height = it->second.height;
            }

            f.health = std::min(f.health, f.maxHealth);
            f.poise = std::min(f.poise, f.maxPoise);

            // An attack the new content no longer has ends here
            if (f.action == Action::Attack and not currentAttack(f))
                f.action = Action::Free;
        }

        // Enemies whose definition is gone leave the arena
        fighters.erase(std::remove_if(fighters.begin(), fighters.end(), [&](const Fighter& f) {
            return f.team == Team::Enemy and not c.enemies.count(f.type);
        }), fighters.end());
    }

    void ArenaWorld::placePlayers()
    {
        int k = 0;

        for (auto& f : fighters)
        {
            if (f.team != Team::Player)
                continue;

            // In a row across the spawn point (right, left, further right...), so nobody stands in front of anyone
            const float side = static_cast<float>((k + 1) / 2) * (k % 2 == 1 ? -1.0f : 1.0f);
            f.spawnPosition = arena().playerSpawn + glm::vec3(side * 1.8f, 0.0f, 0.0f);
            ++k;
        }
    }

    void ArenaWorld::buildArena()
    {
        const ArenaDef& a = arena();
        ++arenaBuilds;
        grid = VoxelGrid(a.size.x, a.size.y, a.size.z);
        terrain.reset(&grid, &content->blockTable);

        for (const auto& s : a.shapes.array)
        {
            const std::string ref = s.str("block", "");
            const BlockId block = (ref == "air") ? AIR : content->block(resolveId(ref, a.mod));

            float from[3] = {0, 0, 0}, to[3] = {0, 0, 0};
            const Json* jf = s.find("from");
            const Json* jt = s.find("to");

            if (not jf or not jt or jf->array.size() != 3 or jt->array.size() != 3)
                continue;

            for (int i = 0; i < 3; ++i)
            {
                from[i] = static_cast<float>(jf->array[i].number);
                to[i] = static_cast<float>(jt->array[i].number);
            }

            const glm::ivec3 lo(from[0], from[1], from[2]);
            const glm::ivec3 hi(to[0], to[1], to[2]);

            if (s.flag("hollow", false))
            {
                // Four walls, no roof and no floor
                grid.fill(lo, glm::ivec3(hi.x, hi.y, lo.z), block);
                grid.fill(glm::ivec3(lo.x, lo.y, hi.z), hi, block);
                grid.fill(lo, glm::ivec3(lo.x, hi.y, hi.z), block);
                grid.fill(glm::ivec3(hi.x, lo.y, lo.z), hi, block);
            }
            else
            {
                grid.fill(lo, hi, block);
            }
        }
    }

    int ArenaWorld::spawnEnemy(const std::string& enemyId, const glm::vec3& position, float yaw)
    {
        if (not content or not content->enemies.count(enemyId))
            return 0;

        Fighter e;
        e.id = nextId++;
        e.team = Team::Enemy;
        e.type = enemyId;
        e.spawnPosition = position;
        e.spawnYaw = yaw;
        fighters.push_back(e);
        respawn(fighters.back());

        return e.id;
    }

    int ArenaWorld::addPlayer(int owner)
    {
        int count = 0;
        for (const auto& f : fighters)
            count += f.team == Team::Player;

        Fighter p;
        p.id = nextId++;
        p.team = Team::Player;
        p.type = "player";
        p.owner = owner;

        // In a row across the spawn point (right, left, further right...), so nobody stands in front of anyone
        const float side = static_cast<float>((count + 1) / 2) * (count % 2 == 1 ? -1.0f : 1.0f);
        p.spawnPosition = arena().playerSpawn + glm::vec3(side * 1.8f, 0.0f, 0.0f);
        p.spawnYaw = 0.0f;
        p.weapon = content->weapons.count(tuning.startWeapon) ? tuning.startWeapon : (content->weaponOrder.empty() ? "" : content->weaponOrder.front());
        // The spells to begin with: the tuning's ready-made ones
        for (const auto& id : tuning.playerSpells)
            if (auto spell = content->spells.find(id); spell != content->spells.end())
                p.spells.push_back(spell->second.recipe);
        p.spells.resize(SPELL_SLOTS);

        fighters.push_back(p);
        respawn(fighters.back());

        return p.id;
    }

    void ArenaWorld::removeFighter(int id)
    {
        fighters.erase(std::remove_if(fighters.begin(), fighters.end(), [&](const Fighter& f) { return f.id == id; }), fighters.end());

        if (localPlayerId == id)
            localPlayerId = 0;
    }

    const Fighter* ArenaWorld::nearestPlayer(const glm::vec3& at) const
    {
        const Fighter* best = nullptr;
        float bestDistance = 1e9f;

        for (const auto& f : fighters)
        {
            if (f.team != Team::Player or not f.alive())
                continue;

            const float d = glm::length(f.body.position - at);

            if (d < bestDistance)
            {
                bestDistance = d;
                best = &f;
            }
        }

        return best;
    }

    Fighter* ArenaWorld::nearestPlayer(const glm::vec3& at)
    {
        return const_cast<Fighter*>(static_cast<const ArenaWorld*>(this)->nearestPlayer(at));
    }

    void ArenaWorld::removeEnemies()
    {
        fighters.erase(std::remove_if(fighters.begin(), fighters.end(), [](const Fighter& f) { return f.team == Team::Enemy; }), fighters.end());
    }

    bool ArenaWorld::equip(const std::string& weaponId)
    {
        Fighter* p = player();

        if (not p or not content or not content->weapons.count(weaponId))
            return false;

        p->weapon = weaponId;

        if (p->action == Action::Attack)
            p->action = Action::Free;

        return true;
    }

    Fighter* ArenaWorld::player()
    {
        if (localPlayerId != 0)
            if (Fighter* f = find(localPlayerId))
                return f;

        for (auto& f : fighters)
            if (f.team == Team::Player and f.owner == 0)
                return &f;

        return nullptr;
    }

    const Fighter* ArenaWorld::player() const
    {
        return const_cast<ArenaWorld*>(this)->player();
    }

    Fighter* ArenaWorld::find(int id)
    {
        for (auto& f : fighters)
            if (f.id == id)
                return &f;

        return nullptr;
    }

    const Moveset* ArenaWorld::movesetOf(const Fighter& f) const
    {
        if (not content)
            return nullptr;

        if (f.team == Team::Player)
        {
            auto it = content->weapons.find(f.weapon);
            return it == content->weapons.end() ? nullptr : &it->second.moveset;
        }

        auto it = content->enemies.find(f.type);
        return it == content->enemies.end() ? nullptr : &it->second.moveset;
    }

    const AttackDef* ArenaWorld::currentAttack(const Fighter& f) const
    {
        if (f.action != Action::Attack)
            return nullptr;

        return attackDef(f, f.attack);
    }

    void ArenaWorld::respawn(Fighter& f)
    {
        if (f.team == Team::Player)
        {
            f.maxHealth = tuning.player.health + f.boon.maxHealth;
            f.maxPoise = tuning.player.poise;
            f.body.width = tuning.player.width;
            f.body.height = tuning.player.height;
        }
        else if (auto it = content->enemies.find(f.type); it != content->enemies.end())
        {
            f.maxHealth = it->second.health;
            f.maxPoise = it->second.poise;
            f.body.width = it->second.width;
            f.body.height = it->second.height;
        }

        f.health = f.maxHealth;
        f.poise = f.maxPoise;
        f.stamina = tuning.stamina.max;
        f.exhausted = false;
        f.staminaDelay = 0.0f;
        f.action = Action::Free;
        f.frame = 0;
        f.queued = 0;
        f.queuedAge = -1.0f;
        f.dodgeQueuedAge = -1.0f;
        f.hitstop = 0;
        f.flash = 0;
        f.push = glm::vec3(0.0f);
        f.mana = tuning.mana.max;
        f.manaDelay = 0.0f;
        f.dashCharges = static_cast<float>(maxDashes(f));
        f.statuses.clear();
        f.airJumpsUsed = f.airDashesUsed = f.wallLock = 0;
        f.body.position = f.spawnPosition;
        f.body.velocity = glm::vec3(0.0f);
        f.body.onGround = false;
        f.yaw = f.spawnYaw;
        f.stepOffset = 0.0f;
        f.sinceHit = 100.0f;
        f.sinceAttack = 100.0f;
    }

    // --------------------------------------------------------------------------------------------
    // Step
    // --------------------------------------------------------------------------------------------

    void ArenaWorld::step(const InputFrame& input)
    {
        std::map<int, InputFrame> inputs;

        if (const Fighter* p = player())
            inputs[p->id] = input;

        step(inputs);
    }

    void ArenaWorld::step(const std::map<int, InputFrame>& inputs)
    {
        if (not content)
            return;

        static const InputFrame idle;

        // A network client steps only what it predicts: the rest arrives from the server
        if (not authority)
        {
            for (auto& f : fighters)
            {
                auto it = inputs.find(f.id);

                if (it != inputs.end() and f.team == Team::Player)
                    stepPlayer(f, it->second);
            }

            // The pieces the server says are falling carry the predicted player as they do on the server
            rideFallingPieces();

            time += STEP_SECONDS;
            ++steps;
            return;
        }

        for (auto& f : fighters)
        {
            if (f.team == Team::Player)
            {
                auto it = inputs.find(f.id);
                stepPlayer(f, it != inputs.end() ? it->second : idle);
            }
            else
            {
                stepEnemy(f);
            }
        }

        separate();
        fireProjectiles();
        stepProjectiles();
        stepZones();

        if (inRun() or runOnRequest)
            stepRun(inputs);

        // A run's dead enemies leave the arena
        fighters.erase(std::remove_if(fighters.begin(), fighters.end(), [](const Fighter& f) { return f.gone; }), fighters.end());
        resolvePendingHits();
        resolveHits();
        terrainFromAttacks();
        stepTerrain();
        recordHistory();

        time += STEP_SECONDS;
        ++steps;
    }

    void ArenaWorld::spendDash(Fighter& f)
    {
        if (not (infiniteStamina and f.team == Team::Player))
            f.dashCharges = std::max(0.0f, f.dashCharges - 1.0f);
    }

    void ArenaWorld::spendStamina(Fighter& f, float amount)
    {
        if (amount <= 0.0f or (infiniteStamina and f.team == Team::Player))
            return;

        f.stamina = std::max(0.0f, f.stamina - amount);
        f.staminaDelay = tuning.stamina.regenDelay;

        if (f.stamina <= 0.0f)
            f.exhausted = true;
    }

    void ArenaWorld::regenStamina(Fighter& f)
    {
        const auto& s = tuning.stamina;

        if (f.staminaDelay > 0.0f)
        {
            f.staminaDelay -= STEP_SECONDS;
            return;
        }

        if (f.sprinting or f.action == Action::Attack or f.action == Action::Dodge)
            return;

        f.stamina = std::min(s.max, f.stamina + s.regen * (1.0f + f.boon.staminaRegen) * STEP_SECONDS * (f.exhausted ? s.exhaustedRegenScale : 1.0f));

        if (f.exhausted and f.stamina >= s.max * 0.3f)
            f.exhausted = false;
    }

    void ArenaWorld::regenMana(Fighter& f)
    {
        if (f.manaDelay > 0.0f)
            f.manaDelay -= STEP_SECONDS;
        else
            f.mana = std::min(tuning.mana.max, f.mana + tuning.mana.regen * (1.0f + f.boon.manaRegen) * STEP_SECONDS);

        // Health back over time: a run's regen items
        if (f.boon.regen > 0.0f and f.alive())
            f.health = std::min(f.maxHealth, f.health + f.boon.regen * STEP_SECONDS);
    }

    float ArenaWorld::speedScale(const Fighter& f) const
    {
        float slow = 0.0f;

        for (const auto& s : f.statuses)
            if (auto it = content->statuses.find(s.id); it != content->statuses.end())
                slow += it->second.slow * static_cast<float>(s.stacks);

        return std::clamp(1.0f - slow, 0.1f, 1.0f) * (1.0f + f.boon.speed);
    }

    float ArenaWorld::heightAboveGround(const Fighter& f) const
    {
        const RayHit down = raycast(grid, content->blockTable, f.body.position + glm::vec3(0, 0.05f, 0), glm::vec3(0, -1, 0), 32.0f);
        return down.hit ? down.distance - 0.05f : 32.0f;
    }

    glm::vec3 ArenaWorld::touchedWall(const Fighter& f) const
    {
        // A solid cell against a side of the body, at the knees or the chest
        const float reach = f.body.width * 0.5f + tuning.air.wallReach;
        const glm::vec3 sides[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};

        for (const auto& side : sides)
            for (float h : {0.4f, f.body.height * 0.7f})
            {
                const glm::vec3 probe = f.body.position + glm::vec3(0, h, 0) + side * reach;

                if (grid.isSolid(content->blockTable, static_cast<int>(std::floor(probe.x)), static_cast<int>(std::floor(probe.y)), static_cast<int>(std::floor(probe.z))))
                    return -side;
            }

        return glm::vec3(0.0f);
    }

    const std::string& ArenaWorld::attackFor(const Fighter& f, const Moveset& m, int queued) const
    {
        // In the air: the air attacks, when the moveset has them
        if (not f.body.onGround)
        {
            if (queued == 2 and not m.airHeavy.empty())
                return m.airHeavy;
            if (queued == 1 and not m.airLight.empty())
                return m.airLight;
        }
        // Sprinting (or just stopped): a light press is the dash attack
        else if (queued == 1 and not m.dashAttack.empty() and f.sinceSprint <= tuning.combat.dashAttackWindow)
        {
            return m.dashAttack;
        }

        return (queued == 2 and not m.heavy.empty()) ? m.heavy : m.light;
    }

    void ArenaWorld::turnTowards(Fighter& f, const glm::vec3& dir, float degreesPerSecond)
    {
        if (glm::length(flat(dir)) < 1e-4f)
            return;

        const float diff = wrapDegrees(yawOf(dir) - f.yaw);
        const float maxTurn = degreesPerSecond * STEP_SECONDS;

        f.yaw = wrapDegrees(f.yaw + std::clamp(diff, -maxTurn, maxTurn));
    }

    void ArenaWorld::hitboxInWorld(const Fighter& f, const HitBox& box, glm::vec3& centre, glm::vec3& size) const
    {
        const float p = f.attackPitch * DEG;
        const float c = std::cos(p), s = std::sin(p);

        const glm::vec3 local(box.offset.x, box.offset.y - box.offset.z * s, box.offset.z * c);
        centre = f.body.position + localToWorld(f.yaw, local);
        size = glm::vec3(box.size.x, box.size.y * std::fabs(c) + box.size.z * std::fabs(s), box.size.z * std::fabs(c) + box.size.y * std::fabs(s));
    }

    void ArenaWorld::startAttack(Fighter& f, const std::string& name, const glm::vec3& aimDir, bool aimAssist, float pitch)
    {
        const AttackDef* a = attackDef(f, name);

        if (not a)
            return;

        glm::vec3 aim = flat(aimDir);

        // Aim assist: snap to the enemy closest to the aim inside a cone
        if (f.team == Team::Player and aimAssist and glm::length(aim) > 1e-4f)
        {
            aim = glm::normalize(aim);
            float best = tuning.combat.aimAssistAngle;
            glm::vec3 bestDir = aim;
            const Fighter* picked = nullptr;

            for (const auto& o : fighters)
            {
                if (o.team == f.team or not o.alive())
                    continue;

                // Where the attacker sees it (lag compensation applies to aiming too)
                const glm::vec3 to = flat(bodyFor(f, o).position - f.body.position);
                const float dist = glm::length(to);

                if (dist > tuning.combat.aimAssistRange + o.body.width * 0.5f or dist < 1e-3f)
                    continue;

                const float angle = std::fabs(wrapDegrees(yawOf(to) - yawOf(aim)));

                if (angle < best)
                {
                    best = angle;
                    bestDir = to / dist;
                    picked = &o;
                }
            }

            aim = bestDir;
            assistTarget = picked;
        }

        if (glm::length(aim) > 1e-4f)
            f.yaw = yawOf(aim);

        // Up or down: given (first person), else at the target (aim assist's, or the player for an enemy)
        const Fighter* target = f.team == Team::Player ? assistTarget : nearestPlayer(f.body.position);
        assistTarget = nullptr;

        if (std::isnan(pitch))
        {
            pitch = 0.0f;

            if (target and target->alive())
            {
                const float drop = chest(f).y - chest(*target).y;
                const float across = std::max(0.3f, glm::length(flat(target->body.position - f.body.position)));
                pitch = std::atan2(drop, across) / DEG;
            }
        }

        f.attackPitch = std::clamp(pitch, -tuning.combat.maxAttackPitch, tuning.combat.maxAttackPitch);

        if (f.team == Team::Player)
            spendStamina(f, a->staminaCost);
        else
            event("windup", chest(f), static_cast<float>(a->startup), f.id);

        f.action = Action::Attack;
        f.attack = name;
        f.frame = 0;
        f.hitThisSwing.clear();
        f.cellsHitThisSwing.clear();
        f.queued = 0;
        f.queuedAge = -1.0f;
        f.sprinting = false;
        f.jumping = false;

        // An air slash hangs: the fall is caught as it starts
        if (not f.body.onGround and a->gravityScale < 1.0f)
            f.body.velocity.y = std::max(f.body.velocity.y, 0.0f) * 0.5f;
    }

    void ArenaWorld::startDodge(Fighter& f, const glm::vec3& dir)
    {
        glm::vec3 d = flat(dir);
        d = glm::length(d) > 1e-4f ? glm::normalize(d) : yawForward(f.yaw);

        spendDash(f);
        event("dodge", f.body.position, 1.0f, f.id);

        f.action = Action::Dodge;
        f.frame = 0;
        f.dodgeDir = d;
        f.yaw = yawOf(d);
        f.dodgeQueuedAge = -1.0f;
        f.queued = 0;
        f.queuedAge = -1.0f;
        f.jumping = false;
    }

    void ArenaWorld::startAirDash(Fighter& f, const glm::vec3& dir)
    {
        glm::vec3 d = flat(dir);
        d = glm::length(d) > 1e-4f ? glm::normalize(d) : yawForward(f.yaw);

        spendDash(f);
        event("air_dash", f.body.position, 1.0f, f.id);

        f.action = Action::AirDash;
        f.frame = 0;
        f.dodgeDir = d;
        f.yaw = yawOf(d);
        f.dodgeQueuedAge = -1.0f;
        f.airDodgeAge = -1.0f;   // used up here: no landing roll waiting
        f.queued = 0;
        f.queuedAge = -1.0f;
        f.jumping = false;
        ++f.airDashesUsed;
    }

    void ArenaWorld::stepPlayer(Fighter& f, const InputFrame& in)
    {
        const float dt = STEP_SECONDS;

        if (f.action == Action::Dead)
        {
            f.respawnTimer -= dt;

            // In a run, down until the next wave
            if (f.respawnTimer <= 0.0f and not inRun())
                respawn(f);

            return;
        }

        const AttackDef* attack = currentAttack(f);

        // Presses are remembered for a moment, so one made slightly early still counts
        if (in.lightPressed or in.heavyPressed)
        {
            const bool tooEarly = attack and f.frame < attack->bufferFrom;

            if (not tooEarly)
            {
                f.queued = in.heavyPressed ? 2 : 1;
                f.queuedAge = 0.0f;
            }
        }

        if (glm::length(in.aimDir) > 1e-4f)
            f.aimDir = glm::normalize(in.aimDir);

        // What its owner sees (lag compensation): set before anything this step aims or hits
        f.viewTick = in.viewTick;

        // Another weapon: part of the input, so a server and a predicting client do the same
        if (in.equip > 0 and static_cast<size_t>(in.equip) <= content->weaponOrder.size() and owns(f, content->weaponOrder[static_cast<size_t>(in.equip - 1)]))
        {
            f.weapon = content->weaponOrder[static_cast<size_t>(in.equip - 1)];

            if (f.action == Action::Attack)
                f.action = Action::Free;
        }

        if (in.dodgePressed)
        {
            f.dodgeQueuedAge = 0.0f;

            // Pressed in the air: kept longer than other presses, waiting for the landing
            if (not f.body.onGround)
                f.airDodgeAge = 0.0f;
        }

        if (in.jumpPressed)
            f.jumpBuffer = tuning.player.jumpBuffer;

        // Frozen by hitstop: inputs are kept, nothing else moves
        if (f.hitstop > 0)
        {
            --f.hitstop;
            return;
        }

        if (f.flash > 0)
            --f.flash;

        stepStatuses(f);

        if (not f.alive())
            return;

        // Back on the ground: the air jumps and dashes are there again
        if (f.body.onGround)
            f.airJumpsUsed = f.airDashesUsed = 0;

        if (f.queuedAge >= 0.0f and (f.queuedAge += dt) > tuning.combat.inputBuffer)
        {
            f.queued = 0;
            f.queuedAge = -1.0f;
        }

        if (f.dodgeQueuedAge >= 0.0f and (f.dodgeQueuedAge += dt) > tuning.combat.inputBuffer)
            f.dodgeQueuedAge = -1.0f;

        if (f.airDodgeAge >= 0.0f and (f.airDodgeAge += dt) > tuning.player.landingRollWindow)
            f.airDodgeAge = -1.0f;

        f.sinceHit += dt;

        // Camera relative wish direction
        const glm::vec3 camForward = yawForward(in.cameraYaw);
        glm::vec3 wish = rightOf(in.cameraYaw) * in.move.x + camForward * in.move.y;

        if (glm::length(wish) > 1.0f)
            wish = glm::normalize(wish);

        const bool hasWish = glm::length(wish) > 0.1f;

        // Where an attack goes: the view in first person, else the stick (or the camera when idle)
        const glm::vec3 aimIdle = in.faceCamera ? camForward : (hasWish ? wish : camForward);
        const float aimPitch = in.faceCamera ? in.cameraPitch : NAN;
        const glm::vec3 aimChain = in.faceCamera ? camForward : (hasWish ? wish : yawForward(f.yaw));
        const Moveset* moveset = movesetOf(f);
        const bool canPay = f.stamina >= tuning.stamina.minToAct;
        const bool canDash = f.dashCharges >= 1.0f or infiniteStamina;   // a roll or an air dash
        const bool canDodgeHere = f.body.onGround or tuning.dodge.inAir;

        // In the air, a dodge is an air dash; about to land, it waits for the landing roll instead
        // (landing within landingRollWindow at this fall speed, or nearer the ground than dashMinHeight)
        bool canAirDash = f.team == Team::Player and not f.body.onGround and f.airDashesUsed < tuning.air.dashes and canDash and f.dodgeQueuedAge >= 0.0f;

        if (canAirDash)
        {
            // Time to fall that far from this speed, under the falling gravity
            const float height = heightAboveGround(f);
            const float g = tuning.player.gravity * tuning.player.fallGravityScale;
            const float down = -f.body.velocity.y;
            const float toLand = down > 0.0f ? (-down + std::sqrt(down * down + 2.0f * g * height)) / g : 100.0f;
            canAirDash = height >= tuning.air.dashMinHeight and toLand > tuning.player.landingRollWindow;
        }

        // ---- Decide the action ----
        switch (f.action)
        {
            case Action::Free:
            {
                if (f.dodgeQueuedAge >= 0.0f and canDash and canDodgeHere)
                {
                    startDodge(f, hasWish ? wish : yawForward(f.yaw));
                }
                else if (f.dodgeQueuedAge >= 0.0f and canAirDash)
                {
                    startAirDash(f, hasWish ? wish : yawForward(f.yaw));
                }
                else if (f.queued and canPay and moveset)
                {
                    startAttack(f, attackFor(f, *moveset, f.queued), aimIdle, not in.faceCamera, aimPitch);
                }
                break;
            }

            case Action::Attack:
            {
                if (not attack)
                {
                    f.action = Action::Free;
                    break;
                }

                if (f.dodgeQueuedAge >= 0.0f and attack->dodgeCancelFrom >= 0 and f.frame >= attack->dodgeCancelFrom and canDash and canDodgeHere)
                {
                    startDodge(f, hasWish ? wish : yawForward(f.yaw));
                }
                else if (f.queued and f.frame >= attack->chainFrom)
                {
                    const std::string& next = f.queued == 2 ? attack->nextHeavy : attack->nextLight;

                    if (not next.empty() and canPay)
                        startAttack(f, next, aimChain, not in.faceCamera, aimPitch);
                }

                if (f.action == Action::Attack and f.frame >= currentAttack(f)->total())
                    f.action = Action::Free;

                break;
            }

            case Action::Dodge:
            {
                // A jump ends the roll once it reaches jumpFrom: the jump below then starts this same
                // step, with the roll's speed
                if (f.jumpBuffer > 0.0f and f.frame >= tuning.dodge.jumpFrom and f.coyote > 0.0f)
                    f.action = Action::Free;
                else if (f.queued == 1 and f.frame >= tuning.dodge.attackFrom and moveset and not moveset->dodgeAttack.empty() and canPay)
                    startAttack(f, moveset->dodgeAttack, in.faceCamera ? camForward : (hasWish ? wish : f.dodgeDir), not in.faceCamera, aimPitch);
                else if (f.frame >= tuning.dodge.frames)
                    f.action = Action::Free;

                break;
            }

            case Action::AirDash:
            {
                if (f.frame >= tuning.air.dashFrames)
                {
                    f.action = Action::Free;
                    f.airSpeed = std::max(f.airSpeed, tuning.air.dashSpeed * 0.6f);   // some of it carries on
                }

                break;
            }

            case Action::Stagger:
            case Action::Land:
            {
                // A light hit's stagger is short work: a roll gets out of it after a few frames
                if (f.action == Action::Stagger and not f.heavyStagger and f.frame >= tuning.player.hitstunDodgeFrom and
                    f.dodgeQueuedAge >= 0.0f and canDash and canDodgeHere and f.body.onGround)
                    startDodge(f, hasWish ? wish : yawForward(f.yaw));
                else if (f.frame >= f.actionLength)
                    f.action = Action::Free;

                break;
            }

            case Action::Dead:
                break;
        }

        // A buffered press can start right away from the action that just ended
        if (f.action == Action::Free and f.dodgeQueuedAge >= 0.0f and canDash and canDodgeHere)
            startDodge(f, hasWish ? wish : yawForward(f.yaw));
        else if (f.action == Action::Free and f.dodgeQueuedAge >= 0.0f and canAirDash)
            startAirDash(f, hasWish ? wish : yawForward(f.yaw));
        else if (f.action == Action::Free and f.queued and canPay and moveset)
            startAttack(f, attackFor(f, *moveset, f.queued), aimIdle, not in.faceCamera, aimPitch);

        useAbilities(f, in);

        // ---- Movement for the action ----
        glm::vec3 wanted(0.0f);
        bool canJump = false;
        f.sprinting = false;

        switch (f.action)
        {
            case Action::Free:
            {
                const bool sprint = in.sprintHeld and hasWish and not f.exhausted and f.stamina > 0.0f;

                if (sprint)
                {
                    f.sprinting = true;
                    if (not infiniteStamina)
                        f.stamina = std::max(0.0f, f.stamina - tuning.stamina.sprintCost * dt);
                    f.staminaDelay = tuning.stamina.regenDelay;

                    if (f.stamina <= 0.0f)
                        f.exhausted = true;
                }

                float speed = (f.sprinting ? tuning.player.sprintSpeed : tuning.player.walkSpeed) * speedScale(f);

                // Momentum: the speed taken off with is kept in the air, and for a moment after landing
                if (not f.body.onGround)
                    speed = std::max(speed, f.airSpeed);
                else if (f.landGrace > 0.0f)
                    speed = std::max(speed, f.landSpeed);

                wanted = wish * speed;

                // Just off a wall: the push away from it is not steered back
                if (f.wallLock > 0)
                {
                    --f.wallLock;
                    const float into = glm::dot(wanted, f.wallNormal);

                    if (into < 0.0f)
                        wanted -= f.wallNormal * into;
                }

                if (in.faceCamera)
                    f.yaw = in.cameraYaw;
                else
                    turnTowards(f, wish, tuning.player.turnSpeed);
                canJump = true;
                break;
            }

            case Action::Attack:
            {
                const AttackDef* a = currentAttack(f);

                if (not a)
                    break;

                // First person: the swing follows the view for its whole length, or the hitbox would
                // land where the view was when the attack started
                if (in.faceCamera)
                {
                    f.yaw = in.cameraYaw;
                    f.attackPitch = std::clamp(in.cameraPitch, -tuning.combat.maxAttackPitch, tuning.combat.maxAttackPitch);
                }
                else if (f.frame < a->startup and a->turnRate > 0.0f and hasWish)
                    turnTowards(f, wish, a->turnRate * 60.0f);

                if (a->movesAt(f.frame))
                    wanted = wish * (tuning.player.walkSpeed * a->moveSpeed * speedScale(f));

                // A leap: thrown along the facing and up, once (a backflip out of a finisher)
                if (a->leapAt >= 0 and f.frame == a->leapAt)
                {
                    const glm::vec3 along = yawForward(f.yaw) * a->leap.x;
                    f.body.velocity = glm::vec3(along.x, a->leap.y, along.z);
                    f.body.onGround = false;
                    f.airSpeed = std::fabs(a->leap.x);
                    f.jumping = false;
                    event("leap", f.body.position, 1.0f, f.id);
                }

                // A plunge hangs through its startup (or flies its leap), then dives
                if (a->plungeSpeed > 0.0f and not f.body.onGround)
                {
                    if (f.frame < a->startup)
                        f.body.velocity.y = a->leapAt >= 0 ? f.body.velocity.y : 0.0f;
                    else
                        f.body.velocity.y = -a->plungeSpeed;

                    wanted = glm::vec3(0.0f);
                }

                if (f.frame >= a->lungeFrom and f.frame < a->lungeTo)
                {
                    // The lunge adds to the steering, up to whichever of the two is faster (a charge makes it longer)
                    const float lunge = a->lunge * (1.0f + f.chargeRatio(a) * (a->charge.lunge - 1.0f));
                    const float cap = std::max(lunge, glm::length(wanted));
                    wanted += yawForward(f.yaw) * lunge;

                    if (glm::length(wanted) > cap)
                        wanted = glm::normalize(wanted) * cap;
                }

                break;
            }

            case Action::Land:
            {
                // A slow, not a root, unless the tuning says 0
                wanted = wish * (tuning.player.walkSpeed * tuning.player.hardLandingMoveSpeed);

                if (in.faceCamera)
                    f.yaw = in.cameraYaw;
                else
                    turnTowards(f, wish, tuning.player.turnSpeed);

                break;
            }

            case Action::AirDash:
            {
                // A straight burst, no fall
                f.body.velocity.x = f.dodgeDir.x * tuning.air.dashSpeed;
                f.body.velocity.z = f.dodgeDir.z * tuning.air.dashSpeed;
                f.body.velocity.y = 0.0f;
                wanted = f.dodgeDir * tuning.air.dashSpeed;
                break;
            }

            case Action::Dodge:
            {
                const auto& d = tuning.dodge;
                const int rollFrames = std::max(1, d.frames - d.recovery);

                if (f.frame < rollFrames)
                {
                    const float t = static_cast<float>(f.frame) / static_cast<float>(rollFrames);
                    const float speed = d.speed + (d.endSpeed - d.speed) * t;

                    // Dodges set the speed outright: a roll must not depend on the speed it started from
                    f.body.velocity.x = f.dodgeDir.x * speed;
                    f.body.velocity.z = f.dodgeDir.z * speed;
                    wanted = f.dodgeDir * speed;
                }

                break;
            }

            default:
                break;
        }

        f.sinceSprint = f.sprinting ? 0.0f : f.sinceSprint + dt;

        const bool diving = f.action == Action::Attack and currentAttack(f) and currentAttack(f)->plungeSpeed > 0.0f and not f.body.onGround;

        stepCommon(f, wanted, canJump, &in);

        // A plunge reaching the ground: its strike
        if (diving and f.body.onGround)
            event("plunge_land", f.body.position, f.landedSpeed, f.id);

        // ---- Landings: the landing roll, the wake-up roll and the hard landing ----
        if (f.landedSpeed > 4.0f)
            event(f.landedSpeed >= tuning.player.hardLandingSpeed ? "hard_land" : "land", f.body.position, f.landedSpeed, f.id);

        if (f.landedSpeed > 0.0f or f.body.mantled)
        {
            const bool dodgeReady = (f.dodgeQueuedAge >= 0.0f or f.airDodgeAge >= 0.0f) and (f.dashCharges >= 1.0f or infiniteStamina);
            const glm::vec3 rollDir = hasWish ? wish : yawForward(f.yaw);
            f.airDodgeAge = -1.0f;

            // The counter is bumped below: -1 makes the new action start at frame 0 next step
            if (f.action == Action::Stagger and f.airborneStagger and dodgeReady)
            {
                startDodge(f, rollDir);
                f.frame = -1;
            }
            else if (f.action == Action::Free and dodgeReady)
            {
                // Any landing with a dodge waiting rolls on touchdown; a hard one is spared by it
                startDodge(f, rollDir);
                f.frame = -1;
            }
            else if (f.action == Action::Free and f.landedSpeed >= tuning.player.hardLandingSpeed)
            {
                f.action = Action::Land;
                f.actionLength = tuning.player.hardLandingFrames;
                f.sprinting = false;
                f.frame = -1;
            }
        }

        if (f.action == Action::Stagger)
            f.airborneStagger = f.airborneStagger or not f.body.onGround;
        else
            f.airborneStagger = false;
        regenStamina(f);
        regenMana(f);
        f.dashCharges = std::min(static_cast<float>(maxDashes(f)), f.dashCharges + STEP_SECONDS / std::max(0.05f, tuning.dash.recharge));

        // A roll that started this step: when its owner pressed it, for the dodge grace
        if (f.action == Action::Dodge and f.frame <= 0)
            f.dodgeStamp = in.stamp >= 0 ? in.stamp : static_cast<int>(steps);

        // A diving plunge holds its first active frame until it lands: the strike is the landing
        const AttackDef* now = currentAttack(f);
        const bool holding = f.action == Action::Attack and now and now->plungeSpeed > 0.0f and f.frame >= now->startup and not f.body.onGround;

        // A charge holds while its key is: up to its length
        const bool charging = f.action == Action::Attack and now and now->charge.at >= 0 and f.frame == now->charge.at and f.chargeSlot >= 0 and
                              f.chargeSlot < InputFrame::ABILITIES and in.abilityHeld[f.chargeSlot] and f.chargeFrames < now->charge.frames;

        if (charging)
            ++f.chargeFrames;

        if (f.action != Action::Free and not holding and not charging)
            ++f.frame;

        // The attack's own invulnerable frames, as the dodge's: on the frame just played
        const AttackDef* after = currentAttack(f);
        f.attackIframes = f.action == Action::Attack and after and after->iframeFrom >= 0 and f.frame - 1 >= after->iframeFrom and f.frame - 1 < after->iframeTo;
    }

    void ArenaWorld::stepEnemy(Fighter& f)
    {
        const float dt = STEP_SECONDS;
        auto defIt = content->enemies.find(f.type);

        if (defIt == content->enemies.end())
            return;

        const EnemyDef& def = defIt->second;

        if (f.action == Action::Dead)
        {
            f.respawnTimer -= dt;

            // In a run, the dead leave once they have fallen (1.5 s); training: they come back
            if (inRun())
            {
                if (++f.frame > 90)
                    f.gone = true;
            }
            else if (f.respawnTimer <= 0.0f)
            {
                respawn(f);
            }

            return;
        }

        if (f.hitstop > 0)
        {
            --f.hitstop;
            return;
        }

        if (f.flash > 0)
            --f.flash;

        stepStatuses(f);

        if (not f.alive())
            return;

        f.sinceHit += dt;

        if (f.sinceHit > def.poiseRegenDelay)
            f.poise = f.maxPoise;

        if (def.regenerates and f.sinceHit > 2.0f)
            f.health = f.maxHealth;

        Fighter* target = nearestPlayer(f.body.position);
        const bool targetAlive = target and target->alive();

        glm::vec3 toTarget = targetAlive ? flat(target->body.position - f.body.position) : yawForward(f.yaw);
        const float distance = glm::length(toTarget);
        const glm::vec3 dirToTarget = distance > 1e-4f ? toTarget / distance : yawForward(f.yaw);

        glm::vec3 wanted(0.0f);

        switch (f.action)
        {
            case Action::Free:
            {
                f.sinceAttack += dt;

                BrainOrder order;

                if (brain and aiEnabled and not def.script.empty() and targetAlive)
                {
                    BrainView view;
                    view.self = &f;
                    view.target = target;
                    view.def = &def;
                    view.distance = distance;
                    // Yaw grows to the left: flip it so that + means the target is on the right
                    view.angle = -wrapDegrees(yawOf(dirToTarget) - f.yaw);
                    view.time = time;
                    view.hunting = inRun();

                    // Is a block in the way? (a raised wall, a pillar): from chest to chest
                    const glm::vec3 from = f.body.position + glm::vec3(0, f.body.height * 0.6f, 0);
                    const glm::vec3 to = target->body.position + glm::vec3(0, target->body.height * 0.6f, 0);
                    const float span = glm::length(to - from);

                    if (span > 1e-3f)
                    {
                        const RayHit hit = raycast(grid, content->blockTable, from, (to - from) / span, span);
                        view.blocked = hit.hit;
                        view.blockedDistance = hit.hit ? hit.distance : 0.0f;
                    }

                    // The group around it
                    for (const auto& o : fighters)
                    {
                        if (o.team != Team::Enemy or o.id == f.id or not o.alive())
                            continue;

                        if (glm::length(o.body.position - f.body.position) < 15.0f)
                            ++view.alliesNear;
                        if (o.action == Action::Attack)
                            ++view.alliesAttacking;
                        if (glm::length(flat(o.body.position - target->body.position)) < 3.5f)
                            ++view.alliesNearTarget;
                    }

                    const glm::vec3 fromTarget = flat(f.body.position - target->body.position);
                    if (glm::length(fromTarget) > 1e-4f)
                        view.targetFacing = std::fabs(wrapDegrees(yawOf(fromTarget) - target->yaw));

                    std::string error;

                    if (not brain->think(def.script, view, order, error))
                    {
                        f.aiLastError = error;
                        order = BrainOrder{};
                    }
                    else
                    {
                        f.aiLastError.clear();
                    }
                }
                else
                {
                    order.face = targetAlive and not def.script.empty();
                }

                f.aiMove = order.move;
                f.aiStrafe = order.strafe;
                f.aiSpeed = std::clamp(order.speed, 0.0f, 3.0f);
                f.aiFace = order.face;
                f.aiRadius = std::clamp(order.radius, 0.5f, 15.0f);

                if (not order.attack.empty() and def.moveset.find(order.attack))
                {
                    startAttack(f, order.attack, dirToTarget);
                    break;
                }

                const float speed = def.walkSpeed * f.aiSpeed;

                if (f.aiMove == "chase")
                    wanted = dirToTarget * speed;
                else if (f.aiMove == "retreat")
                    wanted = -dirToTarget * speed;
                else if (f.aiMove == "strafe")
                    wanted = glm::normalize(glm::cross(glm::vec3(0, 1, 0), dirToTarget)) * (f.aiStrafe >= 0.0f ? -1.0f : 1.0f) * speed;
                else if (f.aiMove == "flank" and targetAlive)
                {
                    // Round the target towards its back, keeping aiRadius from it: tangent to go round,
                    // radial to hold the circle
                    const glm::vec3 rel = flat(f.body.position - target->body.position);
                    const float r = std::max(0.01f, glm::length(rel));
                    const glm::vec3 out = rel / r;
                    const float behind = yawOf(-yawForward(target->yaw));
                    const float delta = wrapDegrees(behind - yawOf(out));
                    const glm::vec3 tangent = glm::normalize(glm::cross(glm::vec3(0, 1, 0), out)) * (delta >= 0.0f ? 1.0f : -1.0f);
                    const glm::vec3 radial = out * std::clamp((f.aiRadius - r) * 0.8f, -1.0f, 1.0f);
                    glm::vec3 dir = tangent * std::min(1.0f, std::fabs(delta) / 25.0f) + radial;

                    if (glm::length(dir) > 1e-4f)
                        wanted = glm::normalize(dir) * speed * std::min(1.0f, glm::length(dir));
                }

                if (f.aiFace)
                    turnTowards(f, dirToTarget, def.turnSpeed);
                else
                    turnTowards(f, wanted, def.turnSpeed);

                break;
            }

            case Action::Attack:
            {
                const AttackDef* a = currentAttack(f);

                if (not a or f.frame >= a->total())
                {
                    f.action = Action::Free;
                    f.sinceAttack = 0.0f;
                    break;
                }

                // Tracking during the wind-up only: once the swing starts, it is committed
                if (f.frame < a->startup and a->turnRate > 0.0f and targetAlive)
                    turnTowards(f, dirToTarget, a->turnRate * 60.0f);

                if (f.frame >= a->lungeFrom and f.frame < a->lungeTo)
                    wanted = yawForward(f.yaw) * a->lunge;

                // A leap: thrown along the facing (back, for a negative one) and up, once. It keeps pushing along
                // through its flight: a leap into a step rises up the face of it and carries on over the top
                if (a->leapAt >= 0 and f.frame == a->leapAt and f.body.onGround)
                {
                    const glm::vec3 along = yawForward(f.yaw) * a->leap.x;
                    f.body.velocity = glm::vec3(along.x, a->leap.y, along.z);
                    f.body.onGround = false;
                    event("leap", f.body.position, 1.0f, f.id);
                }
                else if (a->leapAt >= 0 and f.frame > a->leapAt and not f.body.onGround)
                {
                    wanted = yawForward(f.yaw) * a->leap.x;
                }

                break;
            }

            case Action::Stagger:
            {
                if (f.frame >= f.actionLength)
                {
                    f.action = Action::Free;
                    f.sinceAttack = 0.0f;
                }

                break;
            }

            default:
                break;
        }

        stepCommon(f, wanted * speedScale(f), false, nullptr);

        if (f.action != Action::Free)
            ++f.frame;
    }

    void ArenaWorld::stepCommon(Fighter& f, glm::vec3 wanted, bool canJump, const InputFrame* in)
    {
        const float dt = STEP_SECONDS;
        const auto& P = tuning.player;
        Body& body = f.body;

        // The speed this step started with: a jump takes off with it, whatever the brakes did since
        const float speedBefore = glm::length(flat(body.velocity));

        // ---- Horizontal: accelerate towards the wanted velocity ----
        if (f.action != Action::Dodge and f.action != Action::AirDash)
        {
            const glm::vec3 current = flat(body.velocity);
            float rate;

            if (glm::length(wanted) < 1e-4f)
                rate = body.onGround ? P.groundDecel : P.airDecel;
            else if (glm::dot(current, wanted) < 0.0f)
                rate = body.onGround ? P.turnAccel : P.airAccel;
            else
                rate = body.onGround ? P.groundAccel : P.airAccel;

            // Speed above the run speed, still going the same way: let it bleed off rather than brake
            // (free movement only: an attack that slows you down does it at once)
            if (f.team == Team::Player and f.action == Action::Free and body.onGround and glm::length(wanted) > 1e-4f and glm::dot(current, wanted) > 0.0f and glm::length(current) > glm::length(wanted))
                rate = P.overspeedDecel;

            // Staggers, hard landings and rooted attacks stop on the spot, they do not slide like a run would.
            // An attack that lets the player move keeps the run's acceleration (and its momentum)
            const AttackDef* attack = currentAttack(f);
            const bool moving = f.team == Team::Player and attack and attack->movesAt(f.frame);

            const bool rootedLanding = f.action == Action::Land and P.hardLandingMoveSpeed <= 0.0f;

            // (an attack in the air keeps its flight: a leap out of it carries on)
            if (f.action == Action::Stagger or rootedLanding or (f.action == Action::Attack and not moving and body.onGround))
                rate = std::max(rate, P.groundDecel * 1.5f);

            glm::vec3 next = approach(current, flat(wanted), rate * dt);

            // A dash: its lunge is the speed at once
            if (attack and attack->dash and f.frame >= attack->lungeFrom and f.frame < attack->lungeTo)
                next = flat(wanted);

            body.velocity.x = next.x;
            body.velocity.z = next.z;
        }
        else if (f.action == Action::Dodge and f.frame >= tuning.dodge.frames - tuning.dodge.recovery)
        {
            const glm::vec3 next = approach(flat(body.velocity), glm::vec3(0.0f), P.groundDecel * dt);
            body.velocity.x = next.x;
            body.velocity.z = next.z;
        }

        // ---- Jump: coyote time and a buffered press ----
        if (body.onGround)
        {
            f.coyote = P.coyoteTime;

            if (body.velocity.y <= 0.0f)
                f.jumping = false;
        }
        else
        {
            f.coyote -= dt;
        }

        if (f.jumpBuffer > 0.0f)
            f.jumpBuffer -= dt;

        const bool groundBefore = body.onGround;
        bool jumped = false;

        if (canJump and f.jumpBuffer > 0.0f and f.coyote > 0.0f)
        {
            jumped = true;
            f.airSpeed = std::max(speedBefore, glm::length(flat(body.velocity)));

            // The ground's brake ran above this step: the jump takes off with the speed from before it
            const glm::vec3 now = flat(body.velocity);
            if (glm::length(now) > 1e-4f and glm::length(now) < speedBefore)
            {
                const glm::vec3 kept = glm::normalize(now) * speedBefore;
                body.velocity.x = kept.x;
                body.velocity.z = kept.z;
            }
            body.velocity.y = std::sqrt(2.0f * P.gravity * P.jumpHeight);
            event("jump", body.position, 1.0f, f.id);
            body.onGround = false;
            f.jumping = true;
            f.coyote = 0.0f;
            f.jumpBuffer = 0.0f;
        }
        else if (canJump and f.team == Team::Player and f.jumpBuffer > 0.0f and not body.onGround)
        {
            const auto& A = tuning.air;
            const glm::vec3 wall = touchedWall(f);

            if (glm::length(wall) > 0.5f)
            {
                // Off the wall: pushed away and up, the air jumps given back
                body.velocity.x = wall.x * A.wallJumpPush;
                body.velocity.z = wall.z * A.wallJumpPush;
                body.velocity.y = std::sqrt(2.0f * P.gravity * A.wallJumpHeight);
                f.airSpeed = A.wallJumpPush;
                f.wallLock = A.wallJumpLockFrames;
                f.wallNormal = wall;
                f.airJumpsUsed = 0;
                f.yaw = yawOf(wall);
                jumped = true;
                event("wall_jump", body.position, 1.0f, f.id);
            }
            else if (f.airJumpsUsed < A.jumps)
            {
                // A second jump: the fall stops, and the speed so far turns to where the stick points
                const float speed = std::max(glm::length(flat(body.velocity)), glm::length(flat(wanted)));
                const glm::vec3 dir = glm::length(flat(wanted)) > 0.1f ? glm::normalize(flat(wanted)) : glm::vec3(0.0f);

                if (glm::length(dir) > 0.5f)
                {
                    body.velocity.x = dir.x * speed;
                    body.velocity.z = dir.z * speed;
                }

                body.velocity.y = std::sqrt(2.0f * P.gravity * A.jumpHeight);
                f.airSpeed = std::max(f.airSpeed, speed);
                ++f.airJumpsUsed;
                jumped = true;
                event("air_jump", body.position, 1.0f, f.id);
            }

            if (jumped)
            {
                f.jumping = true;
                f.jumpBuffer = 0.0f;
            }
        }

        // ---- Gravity: heavier on the way down, heavier still when jump is let go (variable height) ----
        float g = P.gravity;
        float maxFall = P.maxFallSpeed;

        if (body.velocity.y > 0.0f)
        {
            if (f.jumping and in and not in->jumpHeld)
                g *= P.jumpCutGravityScale;
        }
        else
        {
            g *= P.fallGravityScale;
        }

        const AttackDef* attack = f.action == Action::Attack ? currentAttack(f) : nullptr;

        if (attack)
            g *= attack->gravityScale;

        // Crouch held in the air: down fast
        if (f.team == Team::Player and in and in->crouchHeld and not body.onGround and f.action == Action::Free)
        {
            g = P.gravity * tuning.air.fastFallGravityScale;
            maxFall = tuning.air.fastFallMaxSpeed;
        }

        // An air dash, and a plunge (its own speed), fly straight
        const bool noGravity = f.action == Action::AirDash or
                               (attack and attack->plungeSpeed > 0.0f and not body.onGround and (attack->leapAt < 0 or f.frame >= attack->startup));

        if (not noGravity)
            body.velocity.y = std::max(body.velocity.y - g * dt, -maxFall);

        // ---- Knockback decays by itself ----
        const float pushLen = glm::length(f.push);

        if (pushLen > 0.0f)
        {
            const float loss = tuning.feel.knockbackFriction * dt * (body.onGround ? 1.0f : 0.3f);
            f.push = pushLen <= loss ? glm::vec3(0.0f) : f.push * ((pushLen - loss) / pushLen);
        }

        MoveOptions opt;
        const bool mobile = f.action == Action::Free or f.action == Action::Dodge;
        opt.stepHeight = mobile ? P.stepHeight : 0.0f;

        if (f.team == Team::Player)
        {
            opt.airStepHeight = mobile ? P.mantleHeight : 0.0f;
            opt.cornerNudge = P.cornerCorrection;
        }

        const float fallSpeed = -body.velocity.y;
        moveBody(grid, content->blockTable, body, (body.velocity + f.push) * dt, opt);

        // Landing and takeoff, for the momentum and the landing moves
        f.landedSpeed = 0.0f;

        if (body.onGround)
        {
            if (not groundBefore and not jumped)
            {
                f.landedSpeed = std::max(1e-3f, fallSpeed);
                f.landGrace = P.landingGrace;
                f.landSpeed = std::max(f.airSpeed, glm::length(flat(body.velocity)));
            }
            else if (f.landGrace > 0.0f)
            {
                f.landGrace -= dt;
            }
        }
        else if (groundBefore and not jumped)
        {
            // Walked or rolled off a ledge: that speed is the takeoff speed
            f.airSpeed = glm::length(flat(body.velocity));
        }

        if (body.hitX)
            f.push.x = 0.0f;
        if (body.hitZ)
            f.push.z = 0.0f;

        f.stepOffset -= body.steppedUp;
        f.stepOffset *= std::exp(-P.stepSmoothing * dt);

        if (body.position.y < -20.0f)
            respawn(f);
    }

    // --------------------------------------------------------------------------------------------
    // Terrain
    // --------------------------------------------------------------------------------------------

    const AbilityDef* ArenaWorld::ability(int index, const Fighter* f) const
    {
        if (not content or index < 0 or index >= static_cast<int>(tuning.abilities.size()))
            return nullptr;

        if (not f)
            f = player();

        // The weapon's type decides which of the slot's abilities is there
        std::string type;
        if (f)
            if (auto w = content->weapons.find(f->weapon); w != content->weapons.end())
                type = w->second.type;

        for (const auto& id : tuning.abilities[index])
        {
            auto it = content->abilities.find(id);

            if (it == content->abilities.end())
                continue;

            const auto& allowed = it->second.weapons;

            // A run's weapon skills are bought first
            if (f and it->second.kind == "attack" and not owns(*f, id))
                continue;

            if (allowed.empty() or std::find(allowed.begin(), allowed.end(), type) != allowed.end())
                return &it->second;
        }

        return nullptr;
    }

    const AttackDef* ArenaWorld::attackDef(const Fighter& f, const std::string& name) const
    {
        if (not content)
            return nullptr;

        if (not name.empty() and name[0] == '@')
        {
            auto it = content->abilities.find(name.substr(1));
            return it != content->abilities.end() and it->second.kind == "attack" ? &it->second.attack : nullptr;
        }

        const Moveset* m = movesetOf(f);
        return m ? m->find(name) : nullptr;
    }

    bool ArenaWorld::aimedCell(const Fighter& f, const glm::vec3& origin, const glm::vec3& dir, float range, RayHit& out) const
    {
        if (glm::length(dir) < 1e-4f)
            return false;

        // The ray starts at the camera, behind the player: reach past the player by the range
        const glm::vec3 head = f.body.position + glm::vec3(0, f.body.height * 0.9f, 0);
        const float maxDist = glm::length(head - origin) + range + 1.0f;
        out = raycast(grid, content->blockTable, origin, glm::normalize(dir), maxDist);

        if (not out.hit or not grid.inBounds(out.cell.x, out.cell.y, out.cell.z))
            return false;

        // In reach of the player (not of the camera), measured to the nearest point of the block
        const glm::vec3 lo(out.cell), hi = lo + glm::vec3(1.0f);
        const glm::vec3 nearest = glm::clamp(head, lo, hi);
        return glm::length(nearest - head) <= range;
    }

    bool ArenaWorld::cellOccupied(const glm::ivec3& c) const
    {
        for (const auto& f : fighters)
        {
            if (not f.alive())
                continue;

            const glm::vec3 lo = f.body.boxMin(), hi = f.body.boxMax();

            if (hi.x > c.x and lo.x < c.x + 1 and hi.y > c.y and lo.y < c.y + 1 and hi.z > c.z and lo.z < c.z + 1)
                return true;
        }

        return false;
    }

    void ArenaWorld::rideFallingPieces()
    {
        // Whoever stands on a falling piece rides it down (it is out of the grid while it falls: without
        // this they would drop through it, faster than it, and be crushed when it lands on them)
        for (auto& f : fighters)
        {
            if (not f.alive() or f.body.velocity.y > 1.0f)
                continue;

            const glm::vec3 blo = f.body.boxMin(), bhi = f.body.boxMax();
            float top = -1e9f, speed = 0.0f;

            for (const auto& piece : terrain.falling)
                for (const auto& l : piece.occupied)
                {
                    const glm::vec3 lo = piece.cellMin(l), hi = lo + glm::vec3(1.0f);

                    // Under the feet (or just sunk into its top), across the body's footprint
                    if (bhi.x > lo.x + 0.05f and blo.x < hi.x - 0.05f and bhi.z > lo.z + 0.05f and blo.z < hi.z - 0.05f and hi.y <= blo.y + 0.4f and hi.y >= blo.y - 0.6f and hi.y > top)
                    {
                        top = hi.y;
                        speed = piece.speed;
                    }
                }

            // On it, going down at its speed (not faster: the rider would sink in as it lands)
            if (top > -1e8f)
            {
                f.body.position.y = top;
                f.body.velocity.y = -speed;
                f.body.onGround = true;
            }
        }

    }

    void ArenaWorld::liftOut(const glm::ivec3& c)
    {
        for (auto& f : fighters)
        {
            if (not f.alive())
                continue;

            const glm::vec3 lo = f.body.boxMin(), hi = f.body.boxMax();

            if (not (hi.x > c.x and lo.x < c.x + 1 and hi.y > c.y and lo.y < c.y + 1 and hi.z > c.z and lo.z < c.z + 1))
                continue;

            // Out to the side first (a boulder landing on someone shoves them out from under it),
            // nearest free spot at the same height; then up (a pit filling in lifts its prisoner)
            auto freeAt = [&](const glm::vec3& p) {
                Body b = f.body;
                b.position = p;
                return not overlapsSolid(grid, content->blockTable, b.boxMin(), b.boxMax());
            };

            bool moved = false;

            // Mostly above it (riding a piece down as it lands): up onto it
            if (lo.y > static_cast<float>(c.y) + 0.4f and freeAt(glm::vec3(f.body.position.x, static_cast<float>(c.y) + 1.0f, f.body.position.z)))
            {
                f.body.position.y = static_cast<float>(c.y) + 1.0f;
                f.body.velocity.y = 0.0f;
                f.body.onGround = true;
                continue;
            }

            for (float r = 0.5f; r <= 3.0f and not moved; r += 0.5f)
            {
                for (int k = 0; k < 8; ++k)
                {
                    const float angle = static_cast<float>(k) * 0.7853982f;
                    const glm::vec3 p = f.body.position + glm::vec3(std::cos(angle), 0.0f, std::sin(angle)) * r;

                    if (freeAt(p))
                    {
                        f.body.position = p;
                        moved = true;
                        break;
                    }
                }
            }

            for (int i = 0; i < 12 and not moved and overlapsSolid(grid, content->blockTable, f.body.boxMin(), f.body.boxMax()); ++i)
            {
                const float before = f.body.position.y;
                f.body.position.y = std::floor(f.body.position.y) + 1.0f;
                f.stepOffset -= f.body.position.y - before;
            }

            f.body.velocity.y = std::max(f.body.velocity.y, 0.0f);
        }
    }

    void ArenaWorld::useAbilities(Fighter& f, const InputFrame& in)
    {
        for (int i = 0; i < InputFrame::ABILITIES; ++i)
            f.abilityCooldown[i] = std::max(0.0f, f.abilityCooldown[i] - STEP_SECONDS);

        f.mineTimer = std::max(0.0f, f.mineTimer - STEP_SECONDS);

        // A weapon skill also cancels the recovery of a basic attack (the moveset's, once it has hit)
        const AttackDef* busy = currentAttack(f);
        const bool inRecovery = busy and not f.attack.empty() and f.attack[0] != '@' and f.frame - 1 >= busy->startup + busy->active;

        if (f.action != Action::Free and not inRecovery)
            return;

        for (int i = 0; i < InputFrame::ABILITIES; ++i)
        {
            // A spell key: the player's own spell, cast by the server (a client sees it in the snapshots)
            if (const int spell = spellSlotAt(i); spell >= 0)
            {
                if (authority and f.action == Action::Free and in.abilityPressed[i] and f.abilityCooldown[i] <= 0.0f)
                    castSpell(f, spell, in, i);
                continue;
            }

            const AbilityDef* a = ability(i, &f);

            // The terrain and spells belong to the server: a client sees them in the next snapshot. A
            // weapon skill is a move of the player's own: predicted like an attack
            if (not a or (not authority and a->kind != "attack"))
                continue;

            if (f.action != Action::Free and a->kind != "attack")
                continue;

            const bool trigger = a->kind == "mine" ? in.abilityHeld[i] or in.abilityPressed[i] : in.abilityPressed[i];

            if (not trigger or f.abilityCooldown[i] > 0.0f)
                continue;

            // Like an attack: it needs some stamina to start, then costs what it costs
            if (a->staminaCost > 0.0f and f.stamina < tuning.stamina.minToAct)
                continue;

            bool used = false;

            if (a->kind == "attack")
            {
                // Aimed as an attack is: the view in first person, else the stick (or the camera)
                const glm::vec3 camForward = yawForward(in.cameraYaw);
                glm::vec3 wish = rightOf(in.cameraYaw) * in.move.x + camForward * in.move.y;
                const glm::vec3 aim = in.faceCamera ? camForward : (glm::length(wish) > 0.1f ? wish : camForward);

                startAttack(f, "@" + a->id, aim, not in.faceCamera, in.faceCamera ? in.cameraPitch : NAN);
                used = f.action == Action::Attack and f.attack == "@" + a->id;
                f.chargeSlot = i;
                f.chargeFrames = 0;
            }

            if (a->kind == "wall")
            {
                // Across the view, `distance` ahead, standing on whatever ground is there
                glm::vec3 fwd(in.aimDir.x, 0.0f, in.aimDir.z);
                fwd = glm::length(fwd) > 1e-4f ? glm::normalize(fwd) : yawForward(f.yaw);
                const glm::vec3 centre = f.body.position + fwd * a->distance;
                const glm::ivec3 across = std::fabs(fwd.x) > std::fabs(fwd.z) ? glm::ivec3(0, 0, 1) : glm::ivec3(1, 0, 0);
                const BlockId block = content->block(a->block);
                const int feet = static_cast<int>(std::floor(f.body.position.y + 0.5f));

                for (int k = 0; k < a->width; ++k)
                {
                    const glm::ivec3 column = glm::ivec3(std::floor(centre.x), 0, std::floor(centre.z)) + across * (k - (a->width - 1) / 2);

                    int base = feet;
                    for (int y = feet; y >= feet - 3; --y)
                    {
                        if (grid.isSolid(content->blockTable, column.x, y, column.z))
                        {
                            base = y + 1;
                            break;
                        }
                    }

                    for (int h = 0; h < a->height; ++h)
                    {
                        const glm::ivec3 cell(column.x, base + h, column.z);

                        if (grid.inBounds(cell.x, cell.y, cell.z) and grid.get(cell.x, cell.y, cell.z) == AIR and not cellOccupied(cell))
                        {
                            terrain.placeFor(cell, block, a->lifetime);
                            used = true;
                        }
                    }
                }
            }
            else if (a->kind == "pit")
            {
                RayHit hit;

                if (aimedCell(f, in.aimOrigin, in.aimDir, a->range, hit))
                {
                    // Centred on the point aimed at, dug down from the block hit
                    const glm::vec3 point = in.aimOrigin + glm::normalize(in.aimDir) * hit.distance;
                    const int x0 = static_cast<int>(std::floor(point.x - a->width * 0.5f + 0.5f));
                    const int z0 = static_cast<int>(std::floor(point.z - a->width * 0.5f + 0.5f));

                    for (int dx = 0; dx < a->width; ++dx)
                        for (int dz = 0; dz < a->width; ++dz)
                            for (int dy = 0; dy < a->depth; ++dy)
                            {
                                const glm::ivec3 cell(x0 + dx, hit.cell.y - dy, z0 + dz);

                                if (terrain.breakable(grid.get(cell.x, cell.y, cell.z)))
                                {
                                    terrain.digFor(cell, a->lifetime);
                                    used = true;
                                }
                            }
                }
            }
            else if (a->kind == "mine" and f.mineTimer <= 0.0f)
            {
                RayHit hit;

                if (aimedCell(f, in.aimOrigin, in.aimDir, a->range, hit) and terrain.breakable(grid.get(hit.cell.x, hit.cell.y, hit.cell.z)))
                {
                    const BlockId block = grid.get(hit.cell.x, hit.cell.y, hit.cell.z);

                    if (not terrain.damage(hit.cell, a->damage))
                        event("chip", glm::vec3(hit.cell) + glm::vec3(0.5f) + glm::vec3(hit.normal) * 0.5f, 1.0f, f.id, block);
                    f.mineTimer = 1.0f / a->rate;
                    used = true;
                }
            }

            if (used)
            {
                spendStamina(f, a->staminaCost);
                f.abilityCooldown[i] = a->cooldown * std::max(0.3f, 1.0f - f.boon.cooldown);


                if (a->kind == "wall")
                    event("raise", f.body.position + glm::normalize(glm::vec3(in.aimDir.x, 0.0f, in.aimDir.z) + glm::vec3(1e-4f, 0, 0)) * a->distance, 1.0f, f.id, content->block(a->block));
                else if (a->kind == "pit")
                    event("dig", in.aimOrigin + glm::normalize(in.aimDir) * 5.0f, 1.0f, f.id);
            }
        }
    }

    // --------------------------------------------------------------------------------------------
    // Spells: a recipe made into stages; each stage's form comes out of the one before it
    // --------------------------------------------------------------------------------------------

    void ArenaWorld::setSpells(Fighter& f, const std::vector<SpellRecipe>& spells)
    {
        f.spells = spells;
        f.spells.resize(SPELL_SLOTS);
    }

    std::shared_ptr<const Spell> ArenaWorld::spellOf(const SpellRecipe& recipe) const
    {
        std::string key = recipe.element;
        for (const auto& k : recipe.keywords)
            key += "|" + k;

        auto it = spellCache.find(key);

        if (it != spellCache.end())
            return it->second;

        auto spell = std::make_shared<const Spell>(compileSpell(*content, recipe));
        spellCache[key] = spell;
        return spell;
    }

    int ArenaWorld::spellSlotAt(int index) const
    {
        if (index < 0 or index >= static_cast<int>(tuning.abilities.size()))
            return -1;

        for (const auto& id : tuning.abilities[index])
            if (id.rfind("spell:", 0) == 0)
                return std::atoi(id.c_str() + 6) - 1;

        return -1;
    }

    bool ArenaWorld::castSpell(Fighter& f, int slot, const InputFrame& in, int abilityIndex)
    {
        if (slot < 0 or slot >= static_cast<int>(f.spells.size()))
            return false;

        const auto spell = spellOf(f.spells[slot]);
        const bool free = infiniteMana and f.team == Team::Player;
        const float cost = spell->mana * std::max(0.3f, 1.0f - f.boon.manaCost);

        if (not spell->valid() or spell->stages.empty() or (f.mana < cost and not free))
            return false;

        // In a run: only with the element and the keywords bought
        if (not owns(f, "element:" + f.spells[slot].element))
            return false;

        for (const auto& k : f.spells[slot].keywords)
            if (not owns(f, k))
                return false;

        if (not free)
        {
            f.mana -= cost;
            f.manaDelay = tuning.mana.regenDelay;
        }

        f.abilityCooldown[abilityIndex] = spell->cooldown * std::max(0.3f, 1.0f - f.boon.cooldown);

        const glm::vec3 aim = glm::length(in.aimDir) > 1e-4f ? glm::normalize(in.aimDir) : yawForward(f.yaw);

        // The body turns to the spell
        if (glm::length(flat(aim)) > 1e-4f)
            f.yaw = yawOf(flat(aim));

        event(std::string(elementName(spell->element)) + "_cast", chest(f), 1.0f, f.id);
        runStage(spell, 0, chest(f), aim, f.id, f.team, &in);
        return true;
    }

    namespace
    {
        /** Copy k of n placed around a point (n > 1): a ring, a little inside the radius. */
        glm::vec3 copyAt(const glm::vec3& centre, int k, int n, float radius)
        {
            if (n <= 1)
                return centre;

            const float a = 6.2831853f * static_cast<float>(k) / static_cast<float>(n);
            return centre + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * radius * 0.9f;
        }

        /** Copy k of n in a fan about the vertical, `degrees` apart. */
        glm::vec3 fanned(const glm::vec3& dir, int k, int n, float degrees)
        {
            const float turn = glm::radians(degrees * (static_cast<float>(k) - static_cast<float>(n - 1) * 0.5f));
            return glm::normalize(glm::vec3(dir.x * std::cos(turn) + dir.z * std::sin(turn), dir.y, -dir.x * std::sin(turn) + dir.z * std::cos(turn)));
        }
    }

    void ArenaWorld::runStage(const std::shared_ptr<const Spell>& spell, int stage, const glm::vec3& point, const glm::vec3& dir, int owner, Team team, const InputFrame* aim)
    {
        if (not spell or stage < 0 or stage >= static_cast<int>(spell->stages.size()))
            return;

        const SpellStage& st = spell->stages[static_cast<size_t>(stage)];
        const std::string& shape = st.form.shape;
        const std::string element = elementName(spell->element);
        const Fighter* caster = find(owner);
        const bool first = aim and caster;

        if (shape == "bolt")
        {
            glm::vec3 from = point, d = dir;

            // The first: from the chest at what the crosshair is on (the camera is behind: its own ray would miss)
            if (first)
            {
                const glm::vec3 a = glm::normalize(aim->aimDir);
                from = chest(*caster) + glm::vec3(a.x, 0.0f, a.z) * 0.5f;
                const RayHit onto = raycast(grid, content->blockTable, aim->aimOrigin, a, 80.0f);
                const glm::vec3 target = aim->aimOrigin + a * (onto.hit ? onto.distance : 80.0f);
                d = glm::length(target - from) > 0.5f ? glm::normalize(target - from) : a;
            }

            for (int k = 0; k < st.count; ++k)
            {
                Projectile p;
                p.id = nextProjectile++;
                p.owner = owner;
                p.team = team;
                p.source = element;
                p.attack = st.form.id;
                p.def.enabled = true;
                p.def.speed = st.speed;
                p.def.gravity = st.gravity;
                p.def.radius = st.radius;
                p.def.lifetime = st.range / std::max(0.5f, st.speed);
                p.def.damage = st.damage;
                p.def.poiseDamage = st.poiseDamage;
                p.def.knockback = st.knockback;
                p.def.terrainDamage = st.terrainDamage;
                p.def.hitstop = 3;
                p.def.element = spell->element;
                p.def.status = spell->status;
                p.def.terrainRadius = 1.0f;
                if (auto m = st.form.model.find(element); m != st.form.model.end())
                    p.def.model = m->second;
                p.def.modelScale = st.form.modelScale;
                p.spell = spell;
                p.stage = stage;
                p.position = from;
                p.velocity = fanned(d, k, st.count, 7.0f) * st.speed;
                projectiles.push_back(p);
            }
        }
        else if (shape == "beam")
        {
            glm::vec3 start = point, hand = point;

            // The first: along the crosshair's ray, from beside the caster
            if (first)
            {
                const glm::vec3 a = glm::normalize(aim->aimDir);
                start = aim->aimOrigin + a * std::max(0.0f, glm::dot(chest(*caster) - aim->aimOrigin, a));
                hand = chest(*caster) + glm::vec3(a.x, 0.0f, a.z) * 0.4f;
            }

            const glm::vec3 a = first ? glm::normalize(aim->aimDir) : dir;

            for (int k = 0; k < st.count; ++k)
                strikeBeam(spell, stage, start, hand, fanned(a, k, st.count, 5.0f), owner, team);
        }
        else if (shape == "nova")
        {
            const glm::vec3 centre = first ? chest(*caster) : point;
            const AttackDef hit = hitOf(st.form.id, st.damage, st.poiseDamage, st.knockback, 3, 0.3f, spell->element, spell->status);

            for (int k = 0; k < st.count; ++k)
            {
                const glm::vec3 c = copyAt(centre, k, st.count, st.radius);
                hitAround(hit, c, st.radius, owner, team, false);
                elementOnTerrain(spell->element, c, st.radius * 0.6f);
                event(element + "_nova", c, st.radius, 0, AIR, owner);
                runStage(spell, stage + 1, c, dir, owner, team, nullptr);
            }
        }
        else if (shape == "zone")
        {
            glm::vec3 centre = point;

            // The first: on the ground where the crosshair is, within the form's range
            if (first)
            {
                const glm::vec3 a = glm::normalize(aim->aimDir);
                const RayHit onto = raycast(grid, content->blockTable, aim->aimOrigin, a, st.range);
                centre = aim->aimOrigin + a * (onto.hit ? onto.distance : st.range) - a * 0.05f;
            }

            for (int k = 0; k < st.count; ++k)
            {
                glm::vec3 c = copyAt(centre, k, st.count, st.radius);

                // Down onto the ground below
                const RayHit down = raycast(grid, content->blockTable, c + glm::vec3(0, 0.6f, 0), glm::vec3(0, -1, 0), 10.0f);
                if (down.hit)
                    c.y = c.y + 0.6f - down.distance;

                Zone z;
                z.id = nextZone++;
                z.owner = owner;
                z.team = team;
                z.element = spell->element;
                z.position = c;
                z.dir = dir;
                z.radius = st.radius;
                z.duration = z.remaining = st.duration;
                z.damage = st.damage;
                z.poiseDamage = st.poiseDamage;
                z.spell = spell;
                z.stage = stage;
                zones.push_back(z);

                elementOnTerrain(spell->element, c + glm::vec3(0, 0.3f, 0), st.radius);
                event(element + "_zone", c, st.radius, 0, AIR, owner);
            }
        }
    }

    void ArenaWorld::hitAround(const AttackDef& hit, const glm::vec3& centre, float radius, int owner, Team team, bool flatArea)
    {
        // The caster if still there, else a stand-in at the centre (knockback goes away from the centre)
        Fighter standIn;
        standIn.team = team;
        Fighter* o = find(owner);
        Fighter& attacker = o ? *o : standIn;
        const glm::vec3 before = attacker.body.position;
        const int hitstopBefore = attacker.hitstop;

        for (auto& t : fighters)
        {
            if (t.team == team or not t.alive() or t.invulnerable(tuning))
                continue;

            // A zone: a disc on the ground (feet in it); a nova: a ball
            const bool inside = flatArea
                ? glm::length(flat(t.body.position - centre)) <= radius + t.body.width * 0.5f and t.body.position.y > centre.y - 1.0f and t.body.position.y < centre.y + 2.0f
                : glm::length(chest(t) - centre) <= radius + t.body.width * 0.5f;

            if (not inside)
                continue;

            glm::vec3 away = flat(t.body.position - centre);
            away = glm::length(away) > 1e-4f ? glm::normalize(away) : glm::vec3(0, 0, 1);
            attacker.body.position = t.body.position - away;
            hitOrDefer(attacker, t, hit, chest(t));
        }

        attacker.body.position = before;
        attacker.hitstop = hitstopBefore;   // nobody freezes for a spell landing away from them
    }

    void ArenaWorld::chainFrom(const AttackDef& hit, Fighter& firstStruck, int chains, int owner, Team team, Element element)
    {
        // The nearest not yet struck within 6 m of the last, `chains` times
        std::vector<int> struck = {firstStruck.id};
        glm::vec3 from = chest(firstStruck);
        Fighter standIn;
        standIn.team = team;
        Fighter* o = find(owner);
        Fighter& attacker = o ? *o : standIn;
        const int hitstopBefore = attacker.hitstop;

        for (int k = 0; k < chains; ++k)
        {
            Fighter* next = nullptr;
            float nearest = 6.0f;

            for (auto& t : fighters)
            {
                if (t.team == team or not t.alive() or t.invulnerable(tuning) or std::find(struck.begin(), struck.end(), t.id) != struck.end())
                    continue;

                const float d = glm::length(chest(t) - from);

                if (d < nearest)
                {
                    nearest = d;
                    next = &t;
                }
            }

            if (not next)
                break;

            struck.push_back(next->id);
            event(std::string(elementName(element)) + "_beam", from, 1.0f, next->id, AIR, owner);
            events.back().line = true;
            events.back().to = chest(*next);
            from = chest(*next);
            hitOrDefer(attacker, *next, hit, from);
        }

        attacker.hitstop = hitstopBefore;
    }

    void ArenaWorld::strikeBeam(const std::shared_ptr<const Spell>& spell, int stage, const glm::vec3& start, const glm::vec3& hand, const glm::vec3& dir, int owner, Team team)
    {
        const SpellStage& st = spell->stages[static_cast<size_t>(stage)];
        const Fighter* caster = find(owner);

        const RayHit wall = raycast(grid, content->blockTable, start, dir, st.range);
        float best = wall.hit ? wall.distance : st.range;
        Fighter* first = nullptr;

        for (auto& o : fighters)
        {
            if (o.team == team or not o.alive() or o.invulnerable(tuning))
                continue;

            // Where the caster saw it (lag compensation), grown a little: a beam is hard to miss by a hair
            const Body& seen = caster ? bodyFor(*caster, o) : o.body;
            const glm::vec3 lo = seen.boxMin() - glm::vec3(0.3f), hi = seen.boxMax() + glm::vec3(0.3f);
            float t0 = 0.0f, t1 = best;
            bool inside = true;

            for (int k = 0; k < 3 and inside; ++k)
            {
                if (std::fabs(dir[k]) < 1e-8f)
                {
                    inside = start[k] >= lo[k] and start[k] <= hi[k];
                    continue;
                }

                float p = (lo[k] - start[k]) / dir[k], q = (hi[k] - start[k]) / dir[k];
                if (p > q)
                    std::swap(p, q);
                t0 = std::max(t0, p);
                t1 = std::min(t1, q);
                inside = t0 <= t1;
            }

            if (inside and t0 < best)
            {
                best = t0;
                first = &o;
            }
        }

        const std::string element = elementName(spell->element);
        const glm::vec3 end = first ? chest(*first) : start + dir * best;

        event(element + "_beam", hand, 1.0f, first ? first->id : 0, AIR, owner);
        events.back().line = true;
        events.back().to = end;

        if (first)
        {
            const AttackDef hit = hitOf(st.form.id, st.damage, st.poiseDamage, st.knockback, 2, 0.2f, spell->element, spell->status);
            Fighter standIn;
            standIn.team = team;
            standIn.body.position = start;
            Fighter* o = find(owner);
            Fighter& attacker = o ? *o : standIn;
            const int hitstopBefore = attacker.hitstop;
            hitOrDefer(attacker, *first, hit, end);
            attacker.hitstop = hitstopBefore;   // a beam does not freeze its caster
            chainFrom(hit, *first, st.chains, owner, team, spell->element);
        }
        else if (wall.hit and grid.inBounds(wall.cell.x, wall.cell.y, wall.cell.z))
        {
            // Stone does not take volt well (the blocks' "weakness")
            const BlockId block = grid.get(wall.cell.x, wall.cell.y, wall.cell.z);
            const float scale = block < content->blockTable.size() ? content->blockTable[block].weakness[static_cast<int>(spell->element)] : 1.0f;

            if (not terrain.damage(wall.cell, st.terrainDamage * scale))
                event("chip", end, 1.0f, owner, block);

            elementOnTerrain(spell->element, end, 1.0f);
        }

        // The next form where it struck (out of the wall a little)
        const glm::vec3 at = first or not wall.hit ? end : end + glm::vec3(wall.normal) * 0.3f;
        runStage(spell, stage + 1, at, dir, owner, team, nullptr);
    }

    void ArenaWorld::stepZones()
    {
        for (size_t i = 0; i < zones.size();)
        {
            Zone& z = zones[i];
            z.remaining -= STEP_SECONDS;
            z.tick -= STEP_SECONDS;

            // Twice a second, whoever stands in it
            if (z.tick <= 0.0f)
            {
                z.tick += 0.5f;
                const StatusHit status = z.spell ? z.spell->status : StatusHit{};
                const AttackDef hit = hitOf("zone", z.damage * 0.5f, z.poiseDamage * 0.5f, 0.0f, 0, 0.0f, z.element, status);
                hitAround(hit, z.position, z.radius, z.owner, z.team, true);
            }

            if (z.remaining > 0.0f)
            {
                ++i;
                continue;
            }

            // Over: the spell's next form comes out of it
            const Zone done = z;
            zones.erase(zones.begin() + static_cast<std::ptrdiff_t>(i));
            runStage(done.spell, done.stage + 1, done.position + glm::vec3(0, 0.8f, 0), done.dir, done.owner, done.team, nullptr);
        }
    }

    void ArenaWorld::impact(const Projectile& p, const glm::vec3& point, Fighter* struck, const RayHit* wall)
    {
        const AttackDef hit = hitOf(p.attack.empty() ? p.source : p.attack, p.def.damage, p.def.poiseDamage, p.def.knockback, p.def.hitstop, p.def.shake, p.def.element, p.def.status);

        // Who it hits: the one struck, or everyone close enough to a burst
        std::vector<Fighter*> targets;

        if (p.def.burst > 0.0f)
        {
            for (auto& o : fighters)
                if (o.team != p.team and o.alive() and not o.invulnerable(tuning) and glm::length(chest(o) - point) <= p.def.burst + o.body.width * 0.5f)
                    targets.push_back(&o);
        }
        else if (struck)
        {
            targets.push_back(struck);
        }

        // The shooter if it is still there, else a stand-in where the shot came from
        Fighter standIn;
        standIn.team = p.team;
        Fighter* owner = find(p.owner);
        Fighter& attacker = owner ? *owner : standIn;
        const glm::vec3 shooterPos = attacker.body.position;
        const int hitstopBefore = attacker.hitstop;
        const glm::vec3 flight = glm::length(p.velocity) > 1e-4f ? glm::normalize(p.velocity) : glm::vec3(0, 0, 1);

        for (Fighter* t : targets)
        {
            // Knockback away from the impact (the way it flew, for the one struck)
            glm::vec3 away = t == struck ? flight : t->body.position - point;
            away = glm::length(flat(away)) > 1e-4f ? glm::normalize(flat(away)) : flat(flight);
            attacker.body.position = t->body.position - away;
            hitOrDefer(attacker, *t, hit, point);

            if (p.def.element == Element::None)
                event("arrow_hit_fighter", point, 1.0f, t->id, AIR, p.owner);
        }

        attacker.body.position = shooterPos;
        attacker.hitstop = hitstopBefore;   // nobody is frozen by a shot landing far away

        BlockId block = AIR;

        if (wall and grid.inBounds(wall->cell.x, wall->cell.y, wall->cell.z))
        {
            block = grid.get(wall->cell.x, wall->cell.y, wall->cell.z);
            const float scale = block < content->blockTable.size() ? content->blockTable[block].weakness[static_cast<int>(p.def.element)] : 1.0f;
            terrain.damage(wall->cell, p.def.terrainDamage * scale);

            if (p.def.element == Element::None)
                event("arrow_hit", point, 0.5f, 0, block);
        }

        if (p.def.element != Element::None)
        {
            event(std::string(elementName(p.def.element)) + "_burst", point, std::max(p.def.burst, 0.5f), struck ? struck->id : 0, block, p.owner);
            elementOnTerrain(p.def.element, point, p.def.terrainRadius);
        }

        // A spell's bolt: its hit jumps on (chain), then the next form comes out of where it landed,
        // off the wall it struck (bounced), or on through the enemy
        if (p.spell and p.stage >= 0 and p.stage < static_cast<int>(p.spell->stages.size()))
        {
            const SpellStage& st = p.spell->stages[static_cast<size_t>(p.stage)];

            if (struck and st.chains > 0)
                chainFrom(hit, *struck, st.chains, p.owner, p.team, p.def.element);

            glm::vec3 at = point, dir = flight;

            if (wall)
            {
                const glm::vec3 n(wall->normal);
                at = point + n * 0.3f;
                dir = glm::length(n) > 0.5f ? glm::reflect(flight, n) : -flight;
            }

            runStage(p.spell, p.stage + 1, at, dir, p.owner, p.team, nullptr);
        }
    }

    void ArenaWorld::elementOnTerrain(Element element, const glm::vec3& point, float radius)
    {
        const glm::ivec3 c(glm::floor(point));
        const int r = static_cast<int>(std::ceil(radius)) + 1;

        if (element == Element::Fire)
        {
            // What burns, close enough: the block hit too (the impact sits on its face)
            for (int y = -r; y <= r; ++y)
                for (int z = -r; z <= r; ++z)
                    for (int x = -r; x <= r; ++x)
                    {
                        const glm::ivec3 cell = c + glm::ivec3(x, y, z);

                        if (glm::length(glm::vec3(cell) + glm::vec3(0.5f) - point) <= std::max(radius, 0.9f))
                            terrain.ignite(cell, tuning.elements.burnSeconds, tuning.elements.spreadSeconds);
                    }
        }
        else if (element == Element::Ice)
        {
            // A dug pit close by freezes over: ice in the top cell of each of its columns, gone before
            // the pit fills back in (it would stay a hole forever otherwise)
            const BlockId ice = content->block("base:ice");
            bool froze = false;

            if (ice == AIR or radius <= 0.0f)
                return;

            for (int z = -r; z <= r; ++z)
                for (int x = -r; x <= r; ++x)
                {
                    if (glm::length(glm::vec2(x, z)) > radius)
                        continue;

                    for (int y = c.y + 2; y >= c.y - 3; --y)
                    {
                        const glm::ivec3 cell(c.x + x, y, c.z + z);
                        const float left = terrain.dugRemaining(cell);

                        // Not part of a pit: further down
                        if (left < 0.0f)
                            continue;

                        // The pit's top cell: frozen already, or about to fill anyway, the column is done
                        if (grid.get(cell.x, cell.y, cell.z) != AIR or left <= 0.2f)
                            break;

                        if (not cellOccupied(cell))
                        {
                            terrain.placeFor(cell, ice, std::min(tuning.elements.iceLifetime, left - 0.1f));
                            froze = true;
                        }

                        break;
                    }
                }

            if (froze)
                event("freeze", point, radius, 0, ice);
        }
    }

    void ArenaWorld::terrainFromAttacks()
    {
        for (auto& attacker : fighters)
        {
            if (not attacker.alive() or attacker.hitstop > 0)
                continue;

            const AttackDef* a = currentAttack(attacker);

            if (not a or a->terrainDamage <= 0.0f or not a->isActive(attacker.frame - 1))
                continue;

            for (const auto& raw : a->hitboxes)
            {
                glm::vec3 centre, size;
                hitboxInWorld(attacker, raw, centre, size);
                const HitBox box{centre, size};
                const float reach = 0.5f * glm::length(glm::vec2(box.size.x, box.size.z));
                const glm::ivec3 lo(glm::floor(centre - glm::vec3(reach, box.size.y * 0.5f, reach)));
                const glm::ivec3 hi(glm::floor(centre + glm::vec3(reach, box.size.y * 0.5f, reach)));

                for (int y = lo.y; y <= hi.y; ++y)
                    for (int z = lo.z; z <= hi.z; ++z)
                        for (int x = lo.x; x <= hi.x; ++x)
                        {
                            if (not grid.inBounds(x, y, z) or not terrain.breakable(grid.get(x, y, z)))
                                continue;

                            // The cell as a one-block body, against the rotated hitbox
                            Body cell;
                            cell.position = glm::vec3(x + 0.5f, static_cast<float>(y), z + 0.5f);
                            cell.width = 1.0f;
                            cell.height = 1.0f;

                            if (not boxHitsBody(centre, box.size, attacker.yaw, cell))
                                continue;

                            const size_t index = grid.index(x, y, z);

                            if (std::find(attacker.cellsHitThisSwing.begin(), attacker.cellsHitThisSwing.end(), index) != attacker.cellsHitThisSwing.end())
                                continue;

                            attacker.cellsHitThisSwing.push_back(index);
                            terrain.damage(glm::ivec3(x, y, z), a->terrainDamage);
                        }
            }
        }
    }

    void ArenaWorld::stepTerrain()
    {
        // Where the falling pieces are, to shake the camera when one lands near the player
        std::map<int, glm::vec3> pieceCentres;
        for (const auto& piece : terrain.falling)
            pieceCentres[piece.id] = glm::vec3(piece.origin) - glm::vec3(0, piece.drop, 0);

        TerrainEvents events;
        terrain.step(STEP_SECONDS, time, tuning.terrain.gravity, events);

        rideFallingPieces();

        // Falling pieces crush whoever they fall on, once each
        for (auto& piece : terrain.falling)
        {
            if (piece.speed < 2.0f)
                continue;

            for (auto& f : fighters)
            {
                if (not f.alive() or std::find(piece.crushed.begin(), piece.crushed.end(), f.id) != piece.crushed.end())
                    continue;

                const glm::vec3 blo = f.body.boxMin(), bhi = f.body.boxMax();
                bool touching = false;

                for (const auto& l : piece.occupied)
                {
                    const glm::vec3 lo = piece.cellMin(l), hi = lo + glm::vec3(1.0f);

                    // Coming down onto them: the cell's bottom above their feet (not someone standing on top)
                    if (bhi.x > lo.x and blo.x < hi.x and bhi.y > lo.y and blo.y < hi.y and bhi.z > lo.z and blo.z < hi.z and lo.y > blo.y + 0.3f)
                    {
                        touching = true;
                        break;
                    }
                }

                if (not touching)
                    continue;

                piece.crushed.push_back(f.id);

                const auto& T = tuning.terrain;
                const float damage = std::min(T.crushMax, T.crushDamage + T.crushPerBlock * static_cast<float>(piece.occupied.size()));

                AttackDef crush;
                crush.name = "crush";
                crush.damage = damage;
                crush.poiseDamage = T.crushPoise;
                crush.knockback = 3.0f;
                crush.hitstop = 4;
                crush.shake = T.crushShake;

                // A stand-in attacker at the piece's centre, so the knockback pushes out from under it
                Fighter rock;
                rock.id = 0;
                rock.team = f.team == Team::Player ? Team::Enemy : Team::Player;
                glm::vec3 centre(0.0f);
                for (const auto& l : piece.occupied)
                    centre += piece.cellMin(l) + glm::vec3(0.5f);
                rock.body.position = centre / static_cast<float>(piece.occupied.size());
                rock.yaw = f.yaw + 180.0f;

                applyHit(rock, f, crush);
                hits.back().point = f.body.position + glm::vec3(0, f.body.height, 0);
            }
        }

        // Blocks back in the grid (a pit filling in, a piece landing) lift whoever stands in them
        for (const auto& c : events.filled)
            liftOut(c);

        if (const Fighter* p = player())
        {
            for (int id : events.landed)
            {
                auto it = pieceCentres.find(id);

                if (it != pieceCentres.end() and glm::length(it->second - p->body.position) < 14.0f)
                    terrainShake += tuning.terrain.crushShake * 0.5f;
            }
        }

        for (size_t i = 0; i < events.broken.size(); ++i)
        {
            const BlockId id = i < events.brokenBlocks.size() ? events.brokenBlocks[i] : AIR;
            event("break", glm::vec3(events.broken[i]) + glm::vec3(0.5f), 1.0f, 0, id);
        }

        // Set alight: strength is how long it burns (the flames are each machine's to draw)
        for (const auto& c : events.ignited)
            event("ignite", glm::vec3(c) + glm::vec3(0.5f), tuning.elements.burnSeconds, 0, grid.get(c.x, c.y, c.z));

        for (int id : events.fell)
            for (const auto& piece : terrain.falling)
                if (piece.id == id)
                    event("piece_fall", glm::vec3(piece.origin), static_cast<float>(piece.occupied.size()));

        for (int id : events.landed)
        {
            auto it = pieceCentres.find(id);
            if (it != pieceCentres.end())
                event("piece_land", it->second, 1.0f);
        }

        terrainEvents.broken.insert(terrainEvents.broken.end(), events.broken.begin(), events.broken.end());
        terrainEvents.fell.insert(terrainEvents.fell.end(), events.fell.begin(), events.fell.end());
        terrainEvents.landed.insert(terrainEvents.landed.end(), events.landed.begin(), events.landed.end());
    }

    // --------------------------------------------------------------------------------------------
    // Projectiles and events
    // --------------------------------------------------------------------------------------------

    void ArenaWorld::event(const std::string& kind, const glm::vec3& position, float strength, int fighter, BlockId block, int source)
    {
        // Nobody drains the list in a headless run: keep it from growing without end
        if (events.size() > 4096)
            events.erase(events.begin(), events.begin() + 2048);

        WorldEvent e;
        e.kind = kind;
        e.position = position;
        e.strength = strength;
        e.fighter = fighter;
        e.block = block;
        e.source = source;
        e.tick = static_cast<int>(steps) + 1;
        e.color = block != AIR and block < content->blockTable.size() ? content->blockTable[block].color : glm::vec3(1.0f);
        events.push_back(std::move(e));
    }

    void ArenaWorld::fireProjectiles()
    {
        for (auto& f : fighters)
        {
            if (not f.alive() or f.hitstop > 0)
                continue;

            const AttackDef* a = currentAttack(f);

            // The first active frame was just played (the counter is one ahead)
            if (not a or f.frame - 1 != a->startup)
                continue;

            if (not a->projectile.enabled)
            {
                event("swing", chest(f), a->damage, f.id);
                continue;
            }

            const ProjectileDef& def = a->projectile;
            Projectile p;
            p.id = nextProjectile++;
            p.owner = f.id;
            p.team = f.team;
            p.source = f.team == Team::Player ? f.weapon : f.type;
            p.attack = a->name;
            p.def = def;
            p.position = f.body.position + localToWorld(f.yaw, def.spawn);

            glm::vec3 dir = yawForward(f.yaw);
            const Fighter* target = f.team == Team::Enemy ? nearestPlayer(f.body.position) : nullptr;

            if (target and target->alive())
            {
                // At the chest, where it will be (by lead), raised for the drop over the flight time
                const float flight = glm::length(chest(*target) - p.position) / def.speed;
                glm::vec3 aim = chest(*target) + flat(target->body.velocity) * (flight * def.lead);
                aim.y += 0.5f * def.gravity * flight * flight;
                dir = glm::normalize(aim - p.position);
            }
            else if (f.team == Team::Player)
            {
                dir = f.aimDir;
            }

            p.velocity = dir * def.speed;
            projectiles.push_back(p);
            event("shoot", p.position, 1.0f, f.id);
        }
    }

    void ArenaWorld::stepProjectiles()
    {
        const float dt = STEP_SECONDS;

        for (auto it = projectiles.begin(); it != projectiles.end();)
        {
            Projectile& p = *it;
            p.age += dt;
            p.velocity.y -= p.def.gravity * dt;

            const glm::vec3 move = p.velocity * dt;
            const float length = glm::length(move);

            if (p.age > p.def.lifetime or length < 1e-6f)
            {
                it = projectiles.erase(it);
                continue;
            }

            const glm::vec3 dir = move / length;

            // The nearest of: a fighter of the other side (its box grown by the radius), a block
            float best = length;
            Fighter* victim = nullptr;

            for (auto& f : fighters)
            {
                if (f.team == p.team or not f.alive() or f.invulnerable(tuning))
                    continue;

                const glm::vec3 lo = f.body.boxMin() - glm::vec3(p.def.radius), hi = f.body.boxMax() + glm::vec3(p.def.radius);
                float t0 = 0.0f, t1 = best;
                bool inside = true;

                for (int k = 0; k < 3 and inside; ++k)
                {
                    if (std::fabs(dir[k]) < 1e-8f)
                    {
                        inside = p.position[k] >= lo[k] and p.position[k] <= hi[k];
                        continue;
                    }

                    float a = (lo[k] - p.position[k]) / dir[k];
                    float b = (hi[k] - p.position[k]) / dir[k];
                    if (a > b)
                        std::swap(a, b);
                    t0 = std::max(t0, a);
                    t1 = std::min(t1, b);
                    inside = t0 <= t1;
                }

                if (inside and t0 < best)
                {
                    best = t0;
                    victim = &f;
                }
            }

            const RayHit wall = raycast(grid, content->blockTable, p.position, dir, length);

            if (wall.hit and wall.distance < best)
            {
                // Stopped by the arena: a raised wall takes the arrow
                const Projectile shot = p;
                it = projectiles.erase(it);
                impact(shot, shot.position + dir * wall.distance, nullptr, &wall);
                continue;
            }

            if (victim)
            {
                const Projectile shot = p;
                it = projectiles.erase(it);
                impact(shot, shot.position + dir * best, victim, nullptr);
                continue;
            }

            p.position += move;
            ++it;
        }
    }

    // --------------------------------------------------------------------------------------------
    // Hits
    // --------------------------------------------------------------------------------------------

    void ArenaWorld::resolveHits()
    {
        for (auto& attacker : fighters)
        {
            if (not attacker.alive() or attacker.hitstop > 0)
                continue;

            const AttackDef* a = currentAttack(attacker);

            // The frame counter was advanced at the end of the step: the frame just played is frame - 1
            if (not a or not a->isActive(attacker.frame - 1))
                continue;

            for (const auto& raw : a->hitboxes)
            {
                glm::vec3 centre, size;
                hitboxInWorld(attacker, raw, centre, size);
                const HitBox box{centre, size};

                for (auto& target : fighters)
                {
                    if (target.team == attacker.team or not target.alive() or target.invulnerable(tuning))
                        continue;

                    if (std::find(attacker.hitThisSwing.begin(), attacker.hitThisSwing.end(), target.id) != attacker.hitThisSwing.end())
                        continue;

                    // Where the attacker saw the target (lag compensation for a remote player)
                    const Body& seen = bodyFor(attacker, target);

                    if (boxHitsBody(centre, box.size, attacker.yaw, seen))
                    {
                        attacker.hitThisSwing.push_back(target.id);

                        // A charged skill hits harder
                        AttackDef hit = *a;
                        const float charge = attacker.chargeRatio(a);
                        hit.damage *= 1.0f + charge * (a->charge.damage - 1.0f);
                        hit.poiseDamage *= 1.0f + charge * (a->charge.damage - 1.0f);

                        hitOrDefer(attacker, target, hit, (centre + seen.position + glm::vec3(0, seen.height * 0.5f, 0)) * 0.5f);
                    }
                }
            }
        }
    }

    const Body& ArenaWorld::bodyFor(const Fighter& attacker, const Fighter& target) const
    {
        // A remote player swings at what its screen showed: enemies as they were at its view tick
        if (attacker.team != Team::Player or attacker.viewTick < 0 or target.team != Team::Enemy)
            return target.body;

        for (auto it = history.rbegin(); it != history.rend(); ++it)
        {
            if (static_cast<int64_t>(it->tick) > attacker.viewTick)
                continue;

            for (const auto& [id, body] : it->bodies)
                if (id == target.id)
                    return body;

            break;
        }

        return target.body;
    }

    void ArenaWorld::recordHistory()
    {
        Snapshot s;
        s.tick = steps;
        s.bodies.reserve(fighters.size());

        for (const auto& f : fighters)
            s.bodies.emplace_back(f.id, f.body);

        history.push_back(std::move(s));

        if (history.size() > HISTORY)
            history.erase(history.begin());
    }

    void ArenaWorld::hitOrDefer(Fighter& attacker, Fighter& target, const AttackDef& attack, const glm::vec3& point)
    {
        // A hit on a remote player waits a moment: its dodge may already be on the way
        auto grace = defenderGrace.find(target.owner);

        if (target.team == Team::Player and target.owner != 0 and grace != defenderGrace.end() and grace->second > 0)
        {
            pendingHits.push_back({attacker, target.id, attack, point, steps, steps + static_cast<uint64_t>(grace->second)});
            return;
        }

        applyHit(attacker, target, attack);
        hits.back().point = point;
    }

    void ArenaWorld::resolvePendingHits()
    {
        for (auto it = pendingHits.begin(); it != pendingHits.end();)
        {
            if (it->resolveAt > steps)
            {
                ++it;
                continue;
            }

            PendingHit hit = *it;
            it = pendingHits.erase(it);

            Fighter* target = find(hit.target);

            if (not target or not target->alive())
                continue;

            // Dodged in time by its own clock: the roll was pressed early enough that the hit fell in its i-frames
            if (target->action == Action::Dodge and target->dodgeStamp >= 0)
            {
                const int64_t into = static_cast<int64_t>(hit.tick) - target->dodgeStamp;

                if (into >= tuning.dodge.iframeStart and into < tuning.dodge.iframeEnd)
                    continue;
            }

            Fighter* live = find(hit.attacker.id);
            Fighter& attacker = live ? *live : hit.attacker;
            applyHit(attacker, *target, hit.attack);
            hits.back().point = hit.point;
        }
    }

    void ArenaWorld::applyHit(Fighter& attacker, Fighter& target, const AttackDef& a)
    {
        // A player's boons, and the target's take on the element
        float damage = a.damage * (attacker.team == Team::Player ? 1.0f + attacker.boon.damage : 1.0f);
        bool crit = false;

        // A critical hit: twice the damage, by chance (a hash of the step and the two, not the run's random: a client
        // predicting must not move it)
        if (attacker.boon.crit > 0.0f)
        {
            uint32_t h = static_cast<uint32_t>(steps) * 2654435761u ^ static_cast<uint32_t>(attacker.id) * 40503u ^ static_cast<uint32_t>(target.id) * 97u;
            h ^= h >> 15;
            h *= 2246822519u;
            h ^= h >> 13;
            crit = static_cast<float>(h % 1000u) / 1000.0f < attacker.boon.crit;
            damage *= crit ? 2.0f : 1.0f;
        }

        // An elite hits harder; armor takes some off
        if (attacker.elite)
            damage *= 1.4f;
        damage *= 1.0f - std::clamp(target.boon.armor, 0.0f, 0.7f);

        if (target.team == Team::Enemy)
            if (auto def = content->enemies.find(target.type); def != content->enemies.end())
                damage *= def->second.weakness[static_cast<int>(a.element)];

        HitEvent e;
        e.attacker = attacker.id;
        e.target = target.id;
        e.attack = a.name;
        e.damage = damage;
        e.shake = a.shake;
        e.hitstop = static_cast<int>(std::lround(a.hitstop * tuning.feel.hitstopScale));

        const bool spared = godMode and target.team == Team::Player;

        if (not spared)
            target.health -= damage;

        if (attacker.boon.lifeOnHit > 0.0f and attacker.alive() and damage > 0.0f)
            attacker.health = std::min(attacker.maxHealth, attacker.health + attacker.boon.lifeOnHit);

        target.sinceHit = 0.0f;
        target.flash = tuning.feel.hitFlashFrames;

        // Poise: an attack in progress may add armor, so a big swing is not interrupted by a jab
        const AttackDef* targetAttack = currentAttack(target);
        const float armor = targetAttack ? targetAttack->armor : 0.0f;
        target.poise -= a.poiseDamage;

        if (target.poise + armor <= 0.0f)
        {
            int length = tuning.player.hitstunFrames;

            if (target.team == Team::Enemy)
            {
                auto it = content->enemies.find(target.type);
                length = it != content->enemies.end() ? it->second.staggerFrames : 30;
            }

            target.action = Action::Stagger;
            target.frame = 0;
            target.actionLength = length;
            target.poise = target.maxPoise;
            target.queued = 0;
            target.queuedAge = -1.0f;
            target.heavyStagger = a.poiseDamage >= tuning.player.heavyHitPoise;
            e.staggered = true;
        }

        // Knockback away from the attacker
        float scale = 1.0f;

        if (target.team == Team::Enemy)
        {
            auto it = content->enemies.find(target.type);
            scale = it != content->enemies.end() ? it->second.knockbackScale : 1.0f;
        }

        glm::vec3 away = flat(target.body.position - attacker.body.position);
        away = glm::length(away) > 1e-4f ? glm::normalize(away) : yawForward(attacker.yaw);
        target.push += away * a.knockback * scale;

        if (a.knockup > 0.0f)
        {
            target.body.velocity.y = std::max(target.body.velocity.y, a.knockup * scale);
            target.body.onGround = false;
        }

        attacker.hitstop = std::max(attacker.hitstop, e.hitstop);
        target.hitstop = std::max(target.hitstop, e.hitstop);

        if (target.health <= 0.0f)
        {
            target.health = 0.0f;
            e.killed = true;
        }

        hits.push_back(e);
        if (crit)
            event("crit", chest(target), e.damage, target.id, AIR, e.attacker);
        event(e.killed ? "kill" : (e.staggered ? "stagger" : "hit"), target.body.position + glm::vec3(0, target.body.height * 0.6f, 0), e.damage, target.id, AIR, e.attacker);

        if (e.killed)
            die(target, attacker.id);
        else if (not a.status.status.empty())
            applyStatus(target, a.status.status, a.status.stacks + attacker.boon.statusStacks);
    }

    void ArenaWorld::die(Fighter& target, int)
    {
        if (inRun() and target.team == Team::Enemy and target.alive())
        {
            ++run.kills;
            if (auto def = content->enemies.find(target.type); def != content->enemies.end())
            {
                run.score += def->second.score * (target.elite ? 2 : 1);
                spawnShards(target, def->second.drops * (target.elite ? 3 : 1));
            }
        }

        target.health = 0.0f;
        target.action = Action::Dead;
        target.frame = 0;
        target.statuses.clear();

        if (target.team == Team::Player)
        {
            target.respawnTimer = tuning.player.respawnSeconds;
        }
        else
        {
            auto it = content->enemies.find(target.type);
            target.respawnTimer = it != content->enemies.end() ? it->second.respawnSeconds : 3.0f;
        }
    }

    AttackDef ArenaWorld::hitOf(const std::string& name, float damage, float poiseDamage, float knockback, int hitstop, float shake, Element element, const StatusHit& status) const
    {
        AttackDef hit;
        hit.name = name;
        hit.damage = damage;
        hit.poiseDamage = poiseDamage;
        hit.knockback = knockback;
        hit.hitstop = hitstop;
        hit.shake = shake;
        hit.element = element;
        hit.status = status;
        return hit;
    }

    void ArenaWorld::applyStatus(Fighter& target, const std::string& id, int stacks)
    {
        auto defIt = content->statuses.find(id);

        if (defIt == content->statuses.end() or not target.alive())
            return;

        const StatusDef& def = defIt->second;

        auto holds = [&](const std::string& other) {
            return std::any_of(target.statuses.begin(), target.statuses.end(), [&](const StatusState& s) { return s.id == other; });
        };

        // Already what it turns into (frozen): it does not build up again on top
        if (not def.atMax.empty() and holds(def.atMax))
            return;

        auto it = std::find_if(target.statuses.begin(), target.statuses.end(), [&](const StatusState& s) { return s.id == id; });

        if (it == target.statuses.end())
        {
            target.statuses.push_back({id, def.duration, 0});
            it = target.statuses.end() - 1;
        }

        it->stacks = std::min(def.maxStacks, it->stacks + std::max(1, stacks));
        it->remaining = def.duration;

        // Full: it turns into the next one (chilled through -> frozen)
        if (it->stacks >= def.maxStacks and not def.atMax.empty())
        {
            target.statuses.erase(it);
            applyStatus(target, def.atMax, 1);
            return;
        }

        // An interrupt: a stagger of its own length, whatever the poise. An attack with armor shrugs
        // a light one off (a brute's slam goes through a shock); a heavy one stops anything
        const AttackDef* busy = currentAttack(target);
        const bool shrugged = busy and busy->armor > 0.0f and not def.heavy;

        if (def.stagger > 0 and not shrugged)
        {
            target.action = Action::Stagger;
            target.frame = 0;
            target.actionLength = def.stagger;
            target.heavyStagger = def.heavy;
            target.queued = 0;
            target.queuedAge = -1.0f;
        }

        event("status", chest(target), static_cast<float>(it->stacks), target.id);
    }

    void ArenaWorld::stepStatuses(Fighter& f)
    {
        for (auto it = f.statuses.begin(); it != f.statuses.end();)
        {
            auto defIt = content->statuses.find(it->id);

            // Damage over time: the server's to deal (a client only counts the time down)
            if (authority and defIt != content->statuses.end() and defIt->second.damagePerSecond > 0.0f and not (godMode and f.team == Team::Player))
            {
                f.health -= defIt->second.damagePerSecond * static_cast<float>(it->stacks) * STEP_SECONDS;

                if (f.health <= 0.0f)
                {
                    HitEvent e;
                    e.target = f.id;
                    e.attack = it->id;
                    e.killed = true;
                    hits.push_back(e);
                    event("kill", chest(f), 0.0f, f.id);
                    die(f, 0);
                    return;
                }
            }

            it->remaining -= STEP_SECONDS;
            it = it->remaining <= 0.0f ? f.statuses.erase(it) : std::next(it);
        }
    }

    void ArenaWorld::separate()
    {
        for (size_t i = 0; i < fighters.size(); ++i)
        {
            for (size_t j = i + 1; j < fighters.size(); ++j)
            {
                Fighter& a = fighters[i];
                Fighter& b = fighters[j];

                if (not a.alive() or not b.alive())
                    continue;

                // Rolling through an enemy is allowed while invulnerable
                if (a.invulnerable(tuning) or b.invulnerable(tuning))
                    continue;

                const float ay0 = a.body.position.y, ay1 = ay0 + a.body.height;
                const float by0 = b.body.position.y, by1 = by0 + b.body.height;

                if (ay1 <= by0 or by1 <= ay0)
                    continue;

                glm::vec3 d = flat(b.body.position - a.body.position);
                const float dist = glm::length(d);
                const float minDist = (a.body.width + b.body.width) * 0.5f;

                if (dist >= minDist)
                    continue;

                d = dist > 1e-4f ? d / dist : glm::vec3(1, 0, 0);
                const float overlap = minDist - dist;

                // Only the horizontal push, through the physics so nobody is pushed into a wall
                const bool aOnGround = a.body.onGround, bOnGround = b.body.onGround;
                moveBody(grid, content->blockTable, a.body, -d * overlap * 0.5f);
                moveBody(grid, content->blockTable, b.body, d * overlap * 0.5f);
                a.body.onGround = aOnGround;
                b.body.onGround = bOnGround;
            }
        }
    }

    std::vector<ArenaWorld::DebugBox> ArenaWorld::debugBoxes() const
    {
        std::vector<DebugBox> out;

        for (const auto& f : fighters)
        {
            if (not f.alive())
                continue;

            out.push_back({f.body.position + glm::vec3(0, f.body.height * 0.5f, 0), glm::vec3(f.body.width, f.body.height, f.body.width), 0.0f, true, false, f.invulnerable(tuning), f.id});

            const AttackDef* a = currentAttack(f);

            if (not a)
                continue;

            const int frame = f.frame - 1;
            const bool active = a->isActive(frame);
            const bool windup = frame < a->startup;

            if (not active and not windup)
                continue;

            for (const auto& box : a->hitboxes)
            {
                glm::vec3 centre, size;
                hitboxInWorld(f, box, centre, size);
                out.push_back({centre, size, f.yaw, false, active, false, f.id});
            }
        }

        return out;
    }
}
