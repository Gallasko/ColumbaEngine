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
            case Action::Dead: return "dead";
        }

        return "?";
    }

    // --------------------------------------------------------------------------------------------
    // Setup
    // --------------------------------------------------------------------------------------------

    void ArenaWorld::start(const Content& c, const Tuning& t)
    {
        content = &c;
        tuning = t;
        fighters.clear();
        hits.clear();
        time = 0.0f;
        steps = 0;

        buildArena();

        Fighter p;
        p.id = nextId++;
        p.team = Team::Player;
        p.type = "player";
        p.spawnPosition = c.arena.playerSpawn;
        p.spawnYaw = 0.0f;
        p.weapon = c.weapons.count(t.startWeapon) ? t.startWeapon : (c.weaponOrder.empty() ? "" : c.weaponOrder.front());
        fighters.push_back(p);
        respawn(fighters.back());

        for (const auto& s : c.arena.spawns)
            spawnEnemy(s.enemy, s.position, s.yaw);
    }

    void ArenaWorld::setContent(const Content& c, const Tuning& t, bool rebuildArena)
    {
        content = &c;
        tuning = t;

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

    void ArenaWorld::buildArena()
    {
        const ArenaDef& a = content->arena;
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
        for (auto& f : fighters)
            if (f.team == Team::Player)
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

        const Moveset* m = movesetOf(f);
        return m ? m->find(f.attack) : nullptr;
    }

    void ArenaWorld::respawn(Fighter& f)
    {
        if (f.team == Team::Player)
        {
            f.maxHealth = tuning.player.health;
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
        if (not content)
            return;

        for (auto& f : fighters)
        {
            if (f.team == Team::Player)
                stepPlayer(f, input);
            else
                stepEnemy(f);
        }

        separate();
        resolveHits();
        terrainFromAttacks();
        stepTerrain();

        time += STEP_SECONDS;
        ++steps;
    }

    void ArenaWorld::spendStamina(Fighter& f, float amount)
    {
        if (amount <= 0.0f)
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

        f.stamina = std::min(s.max, f.stamina + s.regen * STEP_SECONDS * (f.exhausted ? s.exhaustedRegenScale : 1.0f));

        if (f.exhausted and f.stamina >= s.max * 0.3f)
            f.exhausted = false;
    }

    void ArenaWorld::turnTowards(Fighter& f, const glm::vec3& dir, float degreesPerSecond)
    {
        if (glm::length(flat(dir)) < 1e-4f)
            return;

        const float diff = wrapDegrees(yawOf(dir) - f.yaw);
        const float maxTurn = degreesPerSecond * STEP_SECONDS;

        f.yaw = wrapDegrees(f.yaw + std::clamp(diff, -maxTurn, maxTurn));
    }

    void ArenaWorld::startAttack(Fighter& f, const std::string& name, const glm::vec3& aimDir, bool aimAssist)
    {
        const Moveset* m = movesetOf(f);
        const AttackDef* a = m ? m->find(name) : nullptr;

        if (not a)
            return;

        glm::vec3 aim = flat(aimDir);

        // Aim assist: snap to the enemy closest to the aim inside a cone
        if (f.team == Team::Player and aimAssist and glm::length(aim) > 1e-4f)
        {
            aim = glm::normalize(aim);
            float best = tuning.combat.aimAssistAngle;
            glm::vec3 bestDir = aim;

            for (const auto& o : fighters)
            {
                if (o.team == f.team or not o.alive())
                    continue;

                const glm::vec3 to = flat(o.body.position - f.body.position);
                const float dist = glm::length(to);

                if (dist > tuning.combat.aimAssistRange + o.body.width * 0.5f or dist < 1e-3f)
                    continue;

                const float angle = std::fabs(wrapDegrees(yawOf(to) - yawOf(aim)));

                if (angle < best)
                {
                    best = angle;
                    bestDir = to / dist;
                }
            }

            aim = bestDir;
        }

        if (glm::length(aim) > 1e-4f)
            f.yaw = yawOf(aim);

        if (f.team == Team::Player)
            spendStamina(f, a->staminaCost);

        f.action = Action::Attack;
        f.attack = name;
        f.frame = 0;
        f.hitThisSwing.clear();
        f.cellsHitThisSwing.clear();
        f.queued = 0;
        f.queuedAge = -1.0f;
        f.sprinting = false;
        f.jumping = false;
    }

    void ArenaWorld::startDodge(Fighter& f, const glm::vec3& dir)
    {
        glm::vec3 d = flat(dir);
        d = glm::length(d) > 1e-4f ? glm::normalize(d) : yawForward(f.yaw);

        spendStamina(f, tuning.dodge.cost);

        f.action = Action::Dodge;
        f.frame = 0;
        f.dodgeDir = d;
        f.yaw = yawOf(d);
        f.dodgeQueuedAge = -1.0f;
        f.queued = 0;
        f.queuedAge = -1.0f;
        f.jumping = false;
    }

    void ArenaWorld::stepPlayer(Fighter& f, const InputFrame& in)
    {
        const float dt = STEP_SECONDS;

        if (f.action == Action::Dead)
        {
            f.respawnTimer -= dt;

            if (f.respawnTimer <= 0.0f)
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
        const glm::vec3 aimChain = in.faceCamera ? camForward : (hasWish ? wish : yawForward(f.yaw));
        const Moveset* moveset = movesetOf(f);
        const bool canPay = f.stamina >= tuning.stamina.minToAct;
        const bool canDodgeHere = f.body.onGround or tuning.dodge.inAir;

        // ---- Decide the action ----
        switch (f.action)
        {
            case Action::Free:
            {
                if (f.dodgeQueuedAge >= 0.0f and canPay and canDodgeHere)
                {
                    startDodge(f, hasWish ? wish : yawForward(f.yaw));
                }
                else if (f.queued and canPay and moveset)
                {
                    const std::string& name = (f.queued == 2 and not moveset->heavy.empty()) ? moveset->heavy : moveset->light;
                    startAttack(f, name, aimIdle, not in.faceCamera);
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

                if (f.dodgeQueuedAge >= 0.0f and attack->dodgeCancelFrom >= 0 and f.frame >= attack->dodgeCancelFrom and canPay and canDodgeHere)
                {
                    startDodge(f, hasWish ? wish : yawForward(f.yaw));
                }
                else if (f.queued and f.frame >= attack->chainFrom)
                {
                    const std::string& next = f.queued == 2 ? attack->nextHeavy : attack->nextLight;

                    if (not next.empty() and canPay)
                        startAttack(f, next, aimChain, not in.faceCamera);
                }

                if (f.action == Action::Attack and f.frame >= currentAttack(f)->total())
                    f.action = Action::Free;

                break;
            }

            case Action::Dodge:
            {
                // A jump ends the roll once it reaches jumpFrom (its recovery by default): the jump
                // below then starts this same step, with the roll's speed
                if (f.jumpBuffer > 0.0f and f.frame >= tuning.dodge.jumpFrom and f.coyote > 0.0f)
                    f.action = Action::Free;
                else if (f.queued == 1 and f.frame >= tuning.dodge.attackFrom and moveset and not moveset->dodgeAttack.empty() and canPay)
                    startAttack(f, moveset->dodgeAttack, in.faceCamera ? camForward : (hasWish ? wish : f.dodgeDir), not in.faceCamera);
                else if (f.frame >= tuning.dodge.frames)
                    f.action = Action::Free;

                break;
            }

            case Action::Stagger:
            case Action::Land:
            {
                if (f.frame >= f.actionLength)
                    f.action = Action::Free;

                break;
            }

            case Action::Dead:
                break;
        }

        // A buffered press can start right away from the action that just ended
        if (f.action == Action::Free and f.dodgeQueuedAge >= 0.0f and canPay and canDodgeHere)
            startDodge(f, hasWish ? wish : yawForward(f.yaw));
        else if (f.action == Action::Free and f.queued and canPay and moveset)
            startAttack(f, (f.queued == 2 and not moveset->heavy.empty()) ? moveset->heavy : moveset->light, aimIdle, not in.faceCamera);

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
                    f.stamina = std::max(0.0f, f.stamina - tuning.stamina.sprintCost * dt);
                    f.staminaDelay = tuning.stamina.regenDelay;

                    if (f.stamina <= 0.0f)
                        f.exhausted = true;
                }

                float speed = f.sprinting ? tuning.player.sprintSpeed : tuning.player.walkSpeed;

                // Momentum: the speed taken off with is kept in the air, and for a moment after landing
                if (not f.body.onGround)
                    speed = std::max(speed, f.airSpeed);
                else if (f.landGrace > 0.0f)
                    speed = std::max(speed, f.landSpeed);

                wanted = wish * speed;

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
                    f.yaw = in.cameraYaw;
                else if (f.frame < a->startup and a->turnRate > 0.0f and hasWish)
                    turnTowards(f, wish, a->turnRate * 60.0f);

                if (a->movesAt(f.frame))
                    wanted = wish * (tuning.player.walkSpeed * a->moveSpeed);

                if (f.frame >= a->lungeFrom and f.frame < a->lungeTo)
                {
                    // The lunge adds to the steering, up to whichever of the two is faster
                    const float cap = std::max(a->lunge, glm::length(wanted));
                    wanted += yawForward(f.yaw) * a->lunge;

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

        stepCommon(f, wanted, canJump, &in);

        // ---- Landings: the landing roll, the wake-up roll and the hard landing ----
        if (f.landedSpeed > 0.0f or f.body.mantled)
        {
            const bool dodgeReady = (f.dodgeQueuedAge >= 0.0f or f.airDodgeAge >= 0.0f) and f.stamina >= tuning.stamina.minToAct;
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

        if (f.action != Action::Free)
            ++f.frame;
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

            if (f.respawnTimer <= 0.0f)
                respawn(f);

            return;
        }

        if (f.hitstop > 0)
        {
            --f.hitstop;
            return;
        }

        if (f.flash > 0)
            --f.flash;

        f.sinceHit += dt;

        if (f.sinceHit > def.poiseRegenDelay)
            f.poise = f.maxPoise;

        if (def.regenerates and f.sinceHit > 2.0f)
            f.health = f.maxHealth;

        Fighter* target = player();
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

        stepCommon(f, wanted, false, nullptr);

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
        if (f.action != Action::Dodge)
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

            if (f.action == Action::Stagger or rootedLanding or (f.action == Action::Attack and not moving))
                rate = std::max(rate, P.groundDecel * 1.5f);

            const glm::vec3 next = approach(current, flat(wanted), rate * dt);
            body.velocity.x = next.x;
            body.velocity.z = next.z;
        }
        else if (f.frame >= tuning.dodge.frames - tuning.dodge.recovery)
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
            body.velocity.y = std::sqrt(2.0f * P.gravity * P.jumpHeight);
            body.onGround = false;
            f.jumping = true;
            f.coyote = 0.0f;
            f.jumpBuffer = 0.0f;
        }

        // ---- Gravity: heavier on the way down, heavier still when jump is let go (variable height) ----
        float g = P.gravity;

        if (body.velocity.y > 0.0f)
        {
            if (f.jumping and in and not in->jumpHeld)
                g *= P.jumpCutGravityScale;
        }
        else
        {
            g *= P.fallGravityScale;
        }

        body.velocity.y = std::max(body.velocity.y - g * dt, -P.maxFallSpeed);

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

    const AbilityDef* ArenaWorld::ability(int index) const
    {
        if (not content or index < 0 or index >= static_cast<int>(tuning.abilities.size()))
            return nullptr;

        auto it = content->abilities.find(tuning.abilities[index]);
        return it == content->abilities.end() ? nullptr : &it->second;
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

        if (f.action != Action::Free)
            return;

        for (int i = 0; i < InputFrame::ABILITIES; ++i)
        {
            const AbilityDef* a = ability(i);

            if (not a)
                continue;

            const bool trigger = a->kind == "mine" ? in.abilityHeld[i] or in.abilityPressed[i] : in.abilityPressed[i];

            if (not trigger or f.abilityCooldown[i] > 0.0f)
                continue;

            // Like an attack: it needs some stamina to start, then costs what it costs
            if (a->staminaCost > 0.0f and f.stamina < tuning.stamina.minToAct)
                continue;

            bool used = false;

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
                    terrain.damage(hit.cell, a->damage);
                    f.mineTimer = 1.0f / a->rate;
                    used = true;
                }
            }

            if (used)
            {
                spendStamina(f, a->staminaCost);
                f.abilityCooldown[i] = a->cooldown;
            }
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

            for (const auto& box : a->hitboxes)
            {
                const glm::vec3 centre = attacker.body.position + localToWorld(attacker.yaw, box.offset);
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

                    if (bhi.x > lo.x and blo.x < hi.x and bhi.y > lo.y and blo.y < hi.y and bhi.z > lo.z and blo.z < hi.z)
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

        terrainEvents.broken.insert(terrainEvents.broken.end(), events.broken.begin(), events.broken.end());
        terrainEvents.fell.insert(terrainEvents.fell.end(), events.fell.begin(), events.fell.end());
        terrainEvents.landed.insert(terrainEvents.landed.end(), events.landed.begin(), events.landed.end());
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

            for (const auto& box : a->hitboxes)
            {
                const glm::vec3 centre = attacker.body.position + localToWorld(attacker.yaw, box.offset);

                for (auto& target : fighters)
                {
                    if (target.team == attacker.team or not target.alive() or target.invulnerable(tuning))
                        continue;

                    if (std::find(attacker.hitThisSwing.begin(), attacker.hitThisSwing.end(), target.id) != attacker.hitThisSwing.end())
                        continue;

                    if (boxHitsBody(centre, box.size, attacker.yaw, target.body))
                    {
                        attacker.hitThisSwing.push_back(target.id);
                        applyHit(attacker, target, *a);

                        HitEvent& e = hits.back();
                        e.point = (centre + target.body.position + glm::vec3(0, target.body.height * 0.5f, 0)) * 0.5f;
                    }
                }
            }
        }
    }

    void ArenaWorld::applyHit(Fighter& attacker, Fighter& target, const AttackDef& a)
    {
        HitEvent e;
        e.attacker = attacker.id;
        e.target = target.id;
        e.attack = a.name;
        e.damage = a.damage;
        e.shake = a.shake;
        e.hitstop = static_cast<int>(std::lround(a.hitstop * tuning.feel.hitstopScale));

        const bool spared = godMode and target.team == Team::Player;

        if (not spared)
            target.health -= a.damage;

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
            target.action = Action::Dead;
            target.frame = 0;

            if (target.team == Team::Player)
            {
                target.respawnTimer = tuning.player.respawnSeconds;
            }
            else
            {
                auto it = content->enemies.find(target.type);
                target.respawnTimer = it != content->enemies.end() ? it->second.respawnSeconds : 3.0f;
            }

            e.killed = true;
        }

        hits.push_back(e);
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
                out.push_back({f.body.position + localToWorld(f.yaw, box.offset), box.size, f.yaw, false, active, false, f.id});
        }

        return out;
    }
}
