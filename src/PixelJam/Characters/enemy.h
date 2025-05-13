#pragma once

#include "ECS/system.h"
#include "2D/simple2dobject.h"
#include "2D/collisionsystem.h"
#include "Input/inputcomponent.h"
#include "Systems/coresystems.h"
#include "Systems/basicsystems.h"
#include "../config.h"

namespace pg {

    struct EnemyFlag : public Ctor
    {
        EnemyFlag(float health = 3) : health(health) {}
        EnemyFlag(const EnemyFlag& rhs) : health(rhs.health), ecsRef(rhs.ecsRef), entityId(rhs.entityId) {}

        EnemyFlag& operator=(const EnemyFlag& rhs)
        {
            health = rhs.health;
            ecsRef = rhs.ecsRef;
            entityId = rhs.entityId;

            return *this;
        }

        virtual void onCreation(EntityRef entity)
        {
            ecsRef = entity->world();
            entityId = entity->id;
        }

        float health = 3;

        EntitySystem* ecsRef;
        _unique_id entityId;
    };

    struct EnemyBulletFlag : public Ctor
    {
        EnemyBulletFlag(float dmg = 1) : damage(dmg) {}
        EnemyBulletFlag(const EnemyBulletFlag& rhs) : damage(rhs.damage), ecsRef(rhs.ecsRef), entityId(rhs.entityId) {}

        EnemyBulletFlag& operator=(const EnemyBulletFlag& rhs)
        {
            damage = rhs.damage;
            ecsRef = rhs.ecsRef;
            entityId = rhs.entityId;

            return *this;
        }

        virtual void onCreation(EntityRef entity)
        {
            ecsRef = entity->world();
            entityId = entity->id;
        }

        float damage = 1;

        EntitySystem* ecsRef;
        _unique_id entityId;
    };

    // Holds current AI state
    enum class AIState { Patrol, Chase, Attack, Cooldown };
    struct AIStateComponent
    {
        AIStateComponent() : state(AIState::Patrol) {}
        AIStateComponent(AIState state) : state(state) {}
        AIStateComponent(const AIStateComponent& rhs) : state(rhs.state) {}

        AIStateComponent& operator=(const AIStateComponent& rhs)
        {
            state = rhs.state;
            return *this;
        }

        AIState state = AIState::Patrol;
    };

    // Defines a bullet pattern to use
    enum class BulletPattern { Radial, Spiral, Cone };
    struct PatternComponent
    {
        PatternComponent(BulletPattern pattern) : pattern(pattern) {}
        PatternComponent(const PatternComponent& rhs) : pattern(rhs.pattern) {}

        PatternComponent& operator=(const PatternComponent& rhs)
        {
            pattern = rhs.pattern;
            return *this;
        }

        BulletPattern pattern;

        float angle = 0.0f;
    };

    struct StartSpawnWaveEvent {};
    struct SpawnWaveEvent {};

    // System responsible for spawning waves of enemies
    struct EnemySpawnSystem : public System<InitSys, Listener<StartSpawnWaveEvent>, Listener<SpawnWaveEvent>>
    {
        virtual std::string getSystemName() const override { return "EnemySpawnSystem"; }

        void init() override {
            // create a timer entity to spawn enemies every few seconds
            auto spawnEntity = ecsRef->createEntity();
            spawnTimer = ecsRef->attach<Timer>(spawnEntity);
            spawnTimer->interval = 2000; // milliseconds
            spawnTimer->oneShot = true;
            spawnTimer->callback = makeCallable<SpawnWaveEvent>();
        }

        void onEvent(const StartSpawnWaveEvent&) override {
            spawnTimer->start();
        }

        void onEvent(const SpawnWaveEvent&) override {
            spawnWave();
        }

        void spawnWave()
        {
            // create 5 enemies in random positions
            for (int i = 0; i < 5; ++i)
            {
                auto ent = makeSimple2DShape(ecsRef, Shape2D::Square, 40.f, 40.f, {255,0,0,255});
                ent.get<PositionComponent>()->setZ(5);
                ecsRef->attach<EnemyFlag>(ent.entity, 5.f);

                std::vector<size_t> collidableLayer = {0, 3};

                ecsRef->attach<CollisionComponent>(ent.entity, 4, 1.0, collidableLayer);
                ecsRef->attach<AIStateComponent>(ent.entity);
                ecsRef->attach<PatternComponent>(ent.entity, static_cast<BulletPattern>(rand() % 3));

                // random start offset
                auto pos = ent.get<PositionComponent>();

                pos->setX(rand() % 800);
                pos->setY(rand() % 600);
            }
        }

        CompRef<Timer> spawnTimer;
    };

    // System responsible for enemy AI and shooting
    struct EnemyAISystem : public System<Own<PatternComponent>, Own<AIStateComponent>, InitSys, Listener<TickEvent>>
    {
        virtual std::string getSystemName() const override { return "EnemyAISystem"; }

        void init() override
        {
            registerGroup<EnemyFlag, AIStateComponent, PositionComponent, PatternComponent>();
        }

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        virtual void execute() override
        {
            if (deltaTime == 0.f)
                return;

            for (const auto& group : viewGroup<EnemyFlag, AIStateComponent, PositionComponent, PatternComponent>())
            {
                auto enemy = group->get<EnemyFlag>();
                auto ai = group->get<AIStateComponent>();
                auto pos = group->get<PositionComponent>();
                auto pat = group->get<PatternComponent>();

                switch (ai->state)
                {
                    case AIState::Patrol:
                        chasePlayer(pos, ai);
                        break;
                    case AIState::Chase:
                        moveTowardPlayer(pos, ai);
                        break;
                    case AIState::Attack:
                        shootPattern(enemy, pos, pat);
                        ai->state = AIState::Cooldown;
                        break;
                    case AIState::Cooldown:
                        cooldownBehavior(ai, deltaTime);
                        break;
                }
            }

            deltaTime = 0.f;
        }

        void chasePlayer(PositionComponent* pos, AIStateComponent* ai) {
            // immediately go to chase
            ai->state = AIState::Chase;
        }

        void moveTowardPlayer(PositionComponent* pos, AIStateComponent* ai)
        {
            auto playerPos = findPlayerPosition();
            auto dir = (playerPos - constant::Vector2D{pos->x, pos->y}).normalized();

            pos->setX(pos->x + dir.x * chaseSpeed);
            pos->setY(pos->y + dir.y * chaseSpeed);

            // once close enough
            if ((playerPos - constant::Vector2D{pos->x, pos->y}).length() < attackDistance)
                ai->state = AIState::Attack;
        }

        void shootPattern(EnemyFlag* enemy, PositionComponent* pos, PatternComponent* pat)
        {
            switch (pat->pattern)
            {
                case BulletPattern::Radial:
                    fireRadial(pos, 8, 200.f);
                    break;
                case BulletPattern::Spiral:
                    fireSpiral(pos, pat, 300.f);
                    break;
                case BulletPattern::Cone:
                    fireCone(pos, 5, 45.f, 250.f);
                    break;
            }
        }

        void cooldownBehavior(AIStateComponent* ai, float dt)
        {
            aiCooldownTimer += dt;

            if (aiCooldownTimer >= cooldownTime)
            {
                aiCooldownTimer = 0;
                ai->state = AIState::Patrol;
            }
        }

        constant::Vector2D findPlayerPosition()
        {
            // Assume single player
            constant::Vector2D p{0,0};

            auto playerEnt = ecsRef->getEntity("Player");

            if (playerEnt and playerEnt->has<PositionComponent>())
            {
                p = {playerEnt->get<PositionComponent>()->x + 25.f, playerEnt->get<PositionComponent>()->y + 25.f};
            }

            LOG_INFO("EnemyAISystem", "Player position: " << p.x << ", " << p.y);

            return p;
        }

        // ------- Bullet pattern helpers -------
        void fireRadial(PositionComponent* pos, int count, float speed)
        {
            for (int i = 0; i < count; ++i)
            {
                float angle = i * 2 * M_PI / count;
                spawnEnemyBullet(pos, angle, speed);
            }
        }

        void fireSpiral(PositionComponent* pos, PatternComponent* pat, float speed)
        {
            pat->angle += spiralRate;

            spawnEnemyBullet(pos, pat->angle, speed);
        }

        void fireCone(PositionComponent* pos, int count, float spreadDeg, float speed)
        {
            auto playerPos = findPlayerPosition();

            float base = atan2(playerPos.y - pos->y, playerPos.x - pos->x);
            float half = spreadDeg * M_PI/180.f / 2;

            for (int i = 0; i < count; ++i)
            {
                float a = base - half + (2 * half * i / (count - 1));
                spawnEnemyBullet(pos, a, speed);
            }
        }

        void spawnEnemyBullet(PositionComponent* pos, float angle, float speed)
        {
            auto b = makeSimple2DShape(ecsRef, Shape2D::Square, 8.f, 8.f, {255, 255, 0, 255});
            auto p = b.get<PositionComponent>();
            p->setX(pos->x + 20.f);
            p->setY(pos->y + 20.f);

            std::vector<size_t> collidableLayer = {0, 1};
            ecsRef->attach<CollisionComponent>(b.entity, 5, 1., collidableLayer);

            ecsRef->attach<MoveDirComponent>(b.entity, constant::Vector2D{cos(angle), sin(angle)}, speed);
            ecsRef->attach<EnemyBulletFlag>(b.entity, enemyBulletDamage);
        }

        // Configurable parameters
        float chaseSpeed = 1.5f;
        float attackDistance = 200.f;
        int cooldownTime = 1000; // ms
        long aiCooldownTimer = 0;
        float spiralRate = 0.1f;
        float enemyBulletDamage = 1.f;

        float deltaTime = 0.f;
    };

} // namespace pg
