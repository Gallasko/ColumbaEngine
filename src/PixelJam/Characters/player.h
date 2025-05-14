#pragma once

#include "ECS/system.h"

#include "2D/simple2dobject.h"

#include "Input/inputcomponent.h"
#include "2D/collisionsystem.h"

#include "Systems/coresystems.h"

#include "Systems/basicsystems.h"

#include "Weapons/weapon.h"

#include "2D/camera2d.h"

#include "../config.h"

namespace pg
{
    struct WallFlag : public Ctor
    {
        WallFlag() {}
        WallFlag(const WallFlag& rhs) : ecsRef(rhs.ecsRef), entityId(rhs.entityId) {}

        WallFlag& operator=(const WallFlag& rhs)
        {
            ecsRef = rhs.ecsRef;
            entityId = rhs.entityId;

            return *this;
        }

        virtual void onCreation(EntityRef entity)
        {
            ecsRef = entity->world();
            entityId = entity->id;
        }

        EntitySystem* ecsRef;
        _unique_id entityId;
    };

    struct PlayerFlag {};
    struct AllyBulletFlag : public Ctor
    {
        AllyBulletFlag(float dmg = 1) : damage(dmg) {}
        AllyBulletFlag(const AllyBulletFlag& rhs) : damage(rhs.damage), ecsRef(rhs.ecsRef), entityId(rhs.entityId) {}

        AllyBulletFlag& operator=(const AllyBulletFlag& rhs)
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

    enum class CollectibleType { Gold, Weapon };
    struct CollectibleFlag
    {
        CollectibleFlag() : type(CollectibleType::Gold) {}
        CollectibleFlag(Weapon weapon) : type(CollectibleType::Weapon), weapon(weapon) {}
        CollectibleFlag(const CollectibleFlag& rhs) : type(rhs.type), weapon(rhs.weapon) {}

        CollectibleFlag& operator=(const CollectibleFlag& rhs)
        {
            type = rhs.type;
            weapon = rhs.weapon;

            return *this;
        }

        CollectibleType type;
        Weapon weapon;
    };

    struct PlayerMoveUp {};
    struct PlayerMoveDown {};
    struct PlayerMoveLeft {};
    struct PlayerMoveRight {};

    // Todo bug bullet can stay stuck in a wall if fired from within the wall

    struct PlayerSystem : public System<QueuedListener<OnMouseClick>, Listener<ConfiguredKeyEvent<GameKeyConfig>>, Listener<ConfiguredKeyEventReleased<GameKeyConfig>>, InitSys,
        Listener<PlayerMoveUp>, Listener<PlayerMoveDown>, Listener<PlayerMoveLeft>, Listener<PlayerMoveRight>>
    {
        virtual std::string getSystemName() const override { return "Player System"; }

        virtual void init() override
        {
            auto playerEnt = makeSimple2DShape(ecsRef, Shape2D::Square, 50.f, 50.f, {0.f, 255.f, 0.f, 255.f});

            playerEnt.get<PositionComponent>()->setZ(10);

            ecsRef->attach<EntityName>(playerEnt.entity, "Player");
            ecsRef->attach<PlayerFlag>(playerEnt.entity);
            ecsRef->attach<FollowCamera2D>(playerEnt.entity);

            playerEnt.get<Simple2DObject>()->setViewport(1);

            std::vector<size_t> collidableLayer = {0, 3, 5, 6};

            ecsRef->attach<CollisionComponent>(playerEnt.entity, 1, 1.0, collidableLayer);

            Weapon baseWeapon;

            ecsRef->attach<WeaponComponent>(playerEnt.entity, baseWeapon);

            player = playerEnt.entity;

            auto entity2 = ecsRef->createEntity();
            auto entity3 = ecsRef->createEntity();
            auto entity4 = ecsRef->createEntity();
            auto entity5 = ecsRef->createEntity();

            upTimer = ecsRef->attach<Timer>(entity2);
            leftTimer = ecsRef->attach<Timer>(entity3);
            bottomTimer = ecsRef->attach<Timer>(entity4);
            rightTimer = ecsRef->attach<Timer>(entity5);

            upTimer->interval = 10;
            leftTimer->interval = 10;
            bottomTimer->interval = 10;
            rightTimer->interval = 10;

            upTimer->callback = makeCallable<PlayerMoveUp>();
            leftTimer->callback = makeCallable<PlayerMoveLeft>();
            bottomTimer->callback = makeCallable<PlayerMoveDown>();
            rightTimer->callback = makeCallable<PlayerMoveRight>();

            upTimer->running = false;
            leftTimer->running = false;
            bottomTimer->running = false;
            rightTimer->running = false;
        }

        virtual void onProcessEvent(const OnMouseClick& event) override
        {
            if (event.button == SDL_BUTTON_LEFT)
            {
                auto pos = player->get<PositionComponent>();
                auto weaponEnt = player->get<WeaponComponent>();

                if (not pos or not weaponEnt)
                    return;

                auto window = ecsRef->getEntity("__MainWindow");

                if (not window)
                    return;

                // Todo I should only need to get the main camera and use mousePosToWorldPos of the main camera instead !
                auto windowWidth = window->get<PositionComponent>()->width;
                auto windowHeight = window->get<PositionComponent>()->height;

                auto normalizedX = 2 * (event.pos.x / windowWidth) - 1.0;
                auto normalizedY = 2 * (event.pos.y / windowHeight) - 1.0;

                const auto& weapon = weaponEnt->weapon;

                for (const auto& dir : weapon.fireDirections({normalizedX, normalizedY}))
                {
                    auto bullet = makeSimple2DShape(ecsRef, Shape2D::Square, weapon.projectileSize, weapon.projectileSize, {125.f, 125.f, 0.f, 255.f});

                    bullet.get<Simple2DObject>()->setViewport(1);

                    bullet.get<PositionComponent>()->setX(pos->x + 25.f);
                    bullet.get<PositionComponent>()->setY(pos->y + 25.f);
                    bullet.get<PositionComponent>()->setZ(50);

                    std::vector<size_t> collidableLayer = {0, 4};

                    ecsRef->attach<CollisionComponent>(bullet.entity, 2, 1.0, collidableLayer);
                    ecsRef->attach<AllyBulletFlag>(bullet.entity);
                    ecsRef->attach<MoveDirComponent>(bullet.entity, dir, weapon.projectileSpeed, weapon.projectileLifeTime, true);
                }
            }
        }

        virtual void onEvent(const ConfiguredKeyEvent<GameKeyConfig>& event) override
        {
            LOG_INFO("Player System", "Received game key input");

            switch (event.value)
            {
            case GameKeyConfig::MoveLeft:
                if (not leftTimer->running)
                    leftTimer->start();
                break;
            case GameKeyConfig::MoveRight:
                if (not rightTimer->running)
                    rightTimer->start();
                break;
            case GameKeyConfig::MoveUp:
                if (not upTimer->running)
                    upTimer->start();
                break;
            case GameKeyConfig::MoveDown:
                if (not bottomTimer->running)
                    bottomTimer->start();
                break;

            case GameKeyConfig::Interact:
                tryCollect();
                break;

            default:
                break;
            }
        }

        virtual void onEvent(const ConfiguredKeyEventReleased<GameKeyConfig>& event) override
        {
            LOG_INFO("Player System", "Received game key release");

            switch (event.value)
            {
            case GameKeyConfig::MoveLeft:
                leftTimer->stop();
                break;
            case GameKeyConfig::MoveRight:
                rightTimer->stop();
                break;
            case GameKeyConfig::MoveUp:
                upTimer->stop();
                break;
            case GameKeyConfig::MoveDown:
                bottomTimer->stop();
                break;

            default:
                break;
            }
        }

        void tryCollect()
        {
            constexpr float collectRadius = 40.f;
            constexpr float collectRadius2 = collectRadius * collectRadius;

            // get player pos
            auto pos = player->get<PositionComponent>();
            constant::Vector2D playerCenter{ pos->x + pos->width / 2.0f, pos->y + pos->height / 2.0f };

            // search all collectibles

            bool found = false;
            CollectibleFlag *collectible = nullptr;
            _unique_id cId;

            float minDist = std::numeric_limits<float>::max();

            // Find nearest collectible
            for (const auto& obj : viewGroup<CollectibleFlag, PositionComponent>())
            {
                auto cf = obj->get<CollectibleFlag>();
                auto cpos = obj->get<PositionComponent>();

                constant::Vector2D colCenter{ cpos->x + 0.5f * 25.f, cpos->y + 0.5f * 25.f };

                float dx = colCenter.x - playerCenter.x;
                float dy = colCenter.y - playerCenter.y;

                float dist2 = dx*dx + dy*dy;

                if (dist2 <= collectRadius2)
                {
                    if (dist2 < minDist)
                    {
                        minDist = dist2;
                        collectible = cf;
                        found = true;
                        cId = obj->entity.id;
                    }
                }
            }

            if (found)
            {
                // it's in range!
                switch (collectible->type)
                {
                    case CollectibleType::Gold:
                        LOG_ERROR("Player System", "Todo !");
                        break;
                    case CollectibleType::Weapon:
                    // // give the weapon to player
                    // if (player->has<WeaponComponent>()) {
                    //     // swap or store old in inventory…
                    // }
                        ecsRef->attach<WeaponComponent>(player.entity, collectible->weapon);
                    break;
                }

                ecsRef->removeEntity(cId);  // remove collectible
            }

        }

        virtual void onEvent(const PlayerMoveUp&) override
        {
            movePlayer(0.f, -movespeed);
        }

        virtual void onEvent(const PlayerMoveDown&) override
        {
            movePlayer(0.f, movespeed);
        }

        virtual void onEvent(const PlayerMoveLeft&) override
        {
            movePlayer(-movespeed, 0.f);
        }

        virtual void onEvent(const PlayerMoveRight&) override
        {
            movePlayer(movespeed, 0.f);
        }

        void movePlayer(float x, float y)
        {
            auto pos = player->get<PositionComponent>();
            pos->setX(pos->x + x);
            pos->setY(pos->y + y);
        }

        EntityRef player;

        CompRef<Timer> upTimer;
        CompRef<Timer> leftTimer;
        CompRef<Timer> bottomTimer;
        CompRef<Timer> rightTimer;

        float movespeed = 10.f;
    };

} // namespace pg
