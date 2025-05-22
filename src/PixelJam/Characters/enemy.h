#pragma once

#include "ECS/system.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "2D/collisionsystem.h"

#include "Input/inputcomponent.h"

#include "Systems/coresystems.h"
#include "Systems/basicsystems.h"

#include "Weapons/weapon.h"

#include "../config.h"

// Todo remove this
// This is to import outlinedTTFText
#include "player.h"

#include "Systems/tween.h"

namespace pg
{
    constexpr float ENEMYINVICIBILITYTIMEMS = 200.f;


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

        float invicibilityTimeLeft = 0;
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
    enum class AIState { Patrol, Chase, ShotWideUp, Attack, Cooldown };
    struct AIStateComponent : public Component
    {
        AIStateComponent() : state(AIState::Patrol) {}
        AIStateComponent(AIState state) : state(state) {}
        AIStateComponent(const AIStateComponent& rhs) :
            state(rhs.state),
            elapsedTime(rhs.elapsedTime),
            chaseSpeed(rhs.chaseSpeed),
            idealDistance(rhs.idealDistance),
            orbitThreshold(rhs.orbitThreshold),
            attackDistance(rhs.attackDistance),
            cooldownTime(rhs.cooldownTime),
            wideUpTime(rhs.wideUpTime),
            feelersLength(rhs.feelersLength),
            feelerAngle(rhs.feelerAngle),
            avoidanceStrength(rhs.avoidanceStrength),
            isBoss(rhs.isBoss),
            orbitDirection(rhs.orbitDirection) {}

        AIStateComponent& operator=(const AIStateComponent& rhs)
        {
            state = rhs.state;
            elapsedTime = rhs.elapsedTime;
            chaseSpeed = rhs.chaseSpeed;
            idealDistance = rhs.idealDistance;
            orbitThreshold = rhs.orbitThreshold;
            attackDistance = rhs.attackDistance;
            cooldownTime = rhs.cooldownTime;
            wideUpTime = rhs.wideUpTime;
            orbitDirection = rhs.orbitDirection;
            feelersLength = rhs.feelersLength;
            feelerAngle = rhs.feelerAngle;
            avoidanceStrength = rhs.avoidanceStrength;
            isBoss = rhs.isBoss;
            return *this;
        }

        AIState state = AIState::Patrol;

        float elapsedTime = 0.f;

        // Configurable parameters
        float chaseSpeed = 1.5f;
        float idealDistance = 250.f;      // px
        float orbitThreshold = 20.f;      // px
        float attackDistance = 200.f;
        float cooldownTime = 1000.0f; // ms
        float wideUpTime = 500.0f;
        float feelersLength = 30;
        float feelerAngle = 15;
        float avoidanceStrength = 0.5;
        bool isBoss;

        float orbitDirection = (rand() % 2 == 0) ? -1.0f : 1.0f;
    };

    struct EnemyComponentsData
    {
        bool canSpawn = true;

        int weaponId = 0;
        Weapon weapon;

        EnemyFlag flag;
        AIStateComponent ai;
    };

    struct EnemySpawnData
    {
        EnemyComponentsData enemy;

        float x = 0.0f;
        float y = 0.0f;
    };

    struct SpawnEnemiesEvent
    {
        std::vector<EnemySpawnData> enemies;
    };

    struct EnemyDeathEvent { _unique_id entityId; };

    struct StartSpawnWaveEvent {};
    struct SpawnWaveEvent {};

    // Todo add this event in the main ecs
    struct RemoveEntityEvent
    {
        RemoveEntityEvent(_unique_id prefabId) : prefabId(prefabId) {}
        RemoveEntityEvent(const RemoveEntityEvent& rhs) : prefabId(rhs.prefabId) {}

        RemoveEntityEvent& operator=(const RemoveEntityEvent& rhs)
        {
            prefabId = rhs.prefabId;
            return *this;
        }

        _unique_id prefabId;
    };

    // System responsible for spawning waves of enemies
    struct EnemySpawnSystem : public System<InitSys, Listener<StartSpawnWaveEvent>, Listener<SpawnWaveEvent>, Listener<SpawnEnemiesEvent>, QueuedListener<RemoveEntityEvent>>
    {
        std::unordered_map<std::string, AsepriteFile> anims;

        EnemySpawnSystem(const std::unordered_map<std::string, AsepriteFile>& anims) : anims(anims) {}

        virtual std::string getSystemName() const override { return "EnemySpawnSystem"; }

        void onProcessEvent(const RemoveEntityEvent& event) override {
            ecsRef->removeEntity(event.prefabId);
        }

        void init() override {
            // create a timer entity to spawn enemies every few seconds
            auto spawnEntity = ecsRef->createEntity();
            spawnTimer = ecsRef->attach<Timer>(spawnEntity);
            spawnTimer->interval = 2000; // milliseconds
            spawnTimer->oneShot = true;
            spawnTimer->callback = makeCallable<SpawnWaveEvent>();
        }

        void onEvent(const StartSpawnWaveEvent&) override {
            // spawnTimer->start();
        }

        void onEvent(const SpawnWaveEvent&) override {
            // spawnWave();
        }

        void onEvent(const SpawnEnemiesEvent& event) override
        {
            for (auto& spawnData : event.enemies)
            {
                Weapon weapon = spawnData.enemy.weapon;

                LOG_INFO("Enemy", "Spawning enemy at (" << spawnData.x << ", " << spawnData.y << ")");
                LOG_INFO("Enemy", "Weapon: " << weapon.name);
                LOG_INFO("Enemy", "Weapon damage: " << weapon.damage);
                LOG_INFO("Enemy", "Weapon ammo: " << weapon.ammo);
                LOG_INFO("Enemy", "Weapon projectileSpeed: " << weapon.projectileSpeed);
                LOG_INFO("Enemy", "Weapon projectileLifeTime: " << weapon.projectileLifeTime);
                LOG_INFO("Enemy", "Weapon projectileSize: " << weapon.projectileSize);
                LOG_INFO("Enemy", "Weapon bulletCount: " << weapon.bulletCount);
                LOG_INFO("Enemy", "Weapon barrelSize: " << weapon.barrelSize);
                LOG_INFO("Enemy", "Weapon reloadTimeMs: " << weapon.reloadTimeMs);
                LOG_INFO("Enemy", "Weapon bulletSpreadAngle: " << weapon.bulletSpreadAngle);

                std::string textureName;

                switch (weapon.pattern)
                {
                    case BulletPattern::Radial:
                        textureName = anims["raider-variant-002"].frames[0].textureName;
                        break;

                    case BulletPattern::Cone:
                        textureName = anims["raider-variant-001"].frames[0].textureName;
                        break;

                    case BulletPattern::AtPlayer:
                    default:
                        textureName = anims["raider"].frames[0].textureName;
                        break;
                }

                // auto ent = makeSimple2DShape(ecsRef, Shape2D::Square, 40.f, 40.f, {255, 0, 0, 255});
                auto ent = makeUiTexture(ecsRef, 64.f, 64.f, textureName);
                ent.get<PositionComponent>()->setZ(5);
                ecsRef->attach<EnemyFlag>(ent.entity, spawnData.enemy.flag);

                ent.get<Texture2DComponent>()->setViewport(1);

                std::vector<size_t> collidableLayer = {0, 3, 4};

                ecsRef->attach<CollisionComponent>(ent.entity, 4, 1.0, collidableLayer);
                ecsRef->attach<AIStateComponent>(ent.entity, spawnData.enemy.ai);

                ecsRef->attach<WeaponComponent>(ent.entity, weapon);

                // random start offset
                auto pos = ent.get<PositionComponent>();

                pos->setX(spawnData.x);
                pos->setY(spawnData.y);
            }
        }

        // Todo remove this obselete
        void spawnWave()
        {
            // create 5 enemies in random positions
            // for (int i = 0; i < 5; ++i)
            // {
            //     auto ent = makeSimple2DShape(ecsRef, Shape2D::Square, 40.f, 40.f, {255, 0, 0, 255});
            //     ent.get<PositionComponent>()->setZ(5);
            //     ecsRef->attach<EnemyFlag>(ent.entity, 5.f);

            //     std::vector<size_t> collidableLayer = {0, 3, 4};

            //     ecsRef->attach<CollisionComponent>(ent.entity, 4, 1.0, collidableLayer);
            //     ecsRef->attach<AIStateComponent>(ent.entity);

            //     Weapon weapon;

            //     weapon.pattern = static_cast<BulletPattern>(rand() % 3);
            //     weapon.bulletCount = 6;
            //     weapon.bulletSpreadAngle = 50.0f;

            //     ecsRef->attach<WeaponComponent>(ent.entity, weapon);

            //     // random start offset
            //     auto pos = ent.get<PositionComponent>();

            //     pos->setX(rand() % 800);
            //     pos->setY(rand() % 600);
            // }
        }

        CompRef<Timer> spawnTimer;
    };

    // System responsible for enemy AI and shooting
    struct EnemyAISystem : public System<Ref<WeaponComponent>, Own<AIStateComponent>, InitSys, Listener<TickEvent>>
    {
        virtual std::string getSystemName() const override { return "EnemyAISystem"; }

        void init() override
        {
            registerGroup<EnemyFlag, AIStateComponent, PositionComponent, WeaponComponent>();
        }

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        virtual void execute() override
        {
            if (deltaTime == 0.f)
                return;

            for (const auto& group : viewGroup<EnemyFlag, AIStateComponent, PositionComponent, WeaponComponent>())
            {
                auto enemy = group->get<EnemyFlag>();
                auto ai = group->get<AIStateComponent>();
                auto pos = group->get<PositionComponent>();
                auto pat = group->get<WeaponComponent>();

                if (enemy->invicibilityTimeLeft > 0.f)
                {
                    auto ent = group->entity;

                    // auto sprite = ent->get<Simple2DObject>();

                    // if (ent->has<Simple2DObject>())
                    // {
                    //     sprite->setColors({255, 255, 255, 255});
                    // }

                    enemy->invicibilityTimeLeft -= deltaTime;

                    if (enemy->invicibilityTimeLeft <= 0.f)
                    {
                        // if (ent->has<Simple2DObject>())
                        // {
                        //     sprite->setColors({255, 0, 0, 255});
                        // }

                        enemy->invicibilityTimeLeft = 0.f;
                    }
                }

                switch (ai->state)
                {
                    case AIState::Patrol:
                        chasePlayer(pos, ai);
                        break;
                    case AIState::Chase:
                        chaseAndOrbit(pos, ai, deltaTime);
                        break;
                    case AIState::ShotWideUp:
                        wideUp(pos, ai, deltaTime);
                        break;
                    case AIState::Attack:
                        shootPattern(enemy, pos, pat);
                        ai->state = AIState::Cooldown;
                        break;
                    case AIState::Cooldown:
                        cooldownBehavior(ai);
                        break;
                }
            }

            deltaTime = 0.f;
        }

        void chasePlayer(PositionComponent*, AIStateComponent* ai) {
            // immediately go to chase
            ai->state = AIState::Chase;
        }

        void chaseAndOrbit(PositionComponent* pos, AIStateComponent* ai, float dt);

        void wideUp(PositionComponent* pos, AIStateComponent* ai, float dt)
        {
            ai->elapsedTime += dt;

            // if (ai->elapsedTime > 150.0f and ai->elapsedTime < 300.0f)
            // {
            //     auto shape = ecsRef->getComponent<Simple2DObject>(pos->id);

            //     if (shape)
            //     {
            //         shape->setColors({255, 255, 0, 255});
            //     }
            // }

            // if (ai->elapsedTime > 300.0f)
            // {
            //     auto shape = ecsRef->getComponent<Simple2DObject>(pos->id);

            //     if (shape)
            //     {
            //         shape->setColors({255, 0, 0, 255});
            //     }
            // }

            if (ai->elapsedTime > ai->wideUpTime)
            {
                ai->elapsedTime = 0.0f;
                ai->state = AIState::Attack;
            }
        }

        constant::Vector2D findPlayerPosition()
        {
            // Assume single player
            constant::Vector2D p{0, 0};

            auto playerEnt = ecsRef->getEntity("Player");

            if (playerEnt and playerEnt->has<PositionComponent>())
            {
                // Todo replace 32 by player width/height
                p = {playerEnt->get<PositionComponent>()->x + 32.f, playerEnt->get<PositionComponent>()->y + 32.f};
            }

            return p;
        }

        void shootPattern(EnemyFlag*, PositionComponent* pos, WeaponComponent* weaponComp)
        {
            auto playerPos = findPlayerPosition();

            // Todo replace 32 by enemy width/height
            constant::Vector2D toPlayer{ playerPos.x - (pos->x + 32.f), playerPos.y - (pos->y + 32.f) };

            auto& weapon = weaponComp->weapon;

            auto fireDir = weapon.fireDirections(toPlayer);

            bool fired = true;

            for (const auto& dir : fireDir)
            {
                if (weapon.ammo != 0)
                {
                    spawnEnemyBullet(pos, dir, weapon);

                    if (weapon.ammo != -1)
                        weapon.ammo--;
                }
                else
                {
                    fired = false;
                }
            }

            if (fired == false)
            {
                // Todo fix ttf may be off by one when loading the atlas
                auto ent = makeOutlinedTTFText(ecsRef, pos->x - pos->width / 2.0f, pos->y - 10, 6, "res/font/Inter/static/Inter_28pt-Light.ttf", "Out of ammo", 0.3, {255, 176, 176, 255}, {0, 0, 0, 255}, 2, 1);

                // Todo add the possibility to make composed tween (here I want to fade out the text AND move it up)
                ecsRef->attach<TweenComponent>(ent.entity, TweenComponent {
                    pos->y - 10,
                    pos->y - 25,
                    400.0f,
                    [ent](const TweenValue& value){ ent.get<PositionComponent>()->setY(std::get<float>(value)); },
                    makeCallable<RemoveEntityEvent>(ent.entity.id)
                });

                // Todo fix prefab vertical center here doesn't work
                // ent.get<UiAnchor>()->setVerticalCenter(anchor->verticalCenter);
            }
        }

        void cooldownBehavior(AIStateComponent* ai)
        {
            ai->elapsedTime = 0.0f;

            ai->state = AIState::Patrol;
        }

        void spawnEnemyBullet(PositionComponent* pos, constant::Vector2D dir, const Weapon& weapon)
        {
            auto b = makeSimple2DShape(ecsRef, Shape2D::Square, weapon.projectileSize, weapon.projectileSize, {255, 255, 0, 255});
            auto p = b.get<PositionComponent>();
            p->setX(pos->x + 20.f);
            p->setY(pos->y + 20.f);
            p->setZ(50);

            b.get<Simple2DObject>()->setViewport(1);

            std::vector<size_t> collidableLayer = {0, 1};
            ecsRef->attach<CollisionComponent>(b.entity, 5, 0.6, collidableLayer);

            ecsRef->attach<MoveDirComponent>(b.entity, dir, weapon.projectileSpeed, weapon.projectileLifeTime, true);
            ecsRef->attach<EnemyBulletFlag>(b.entity, weapon.damage);
        }

        float deltaTime = 0.f;
    };

} // namespace pg
