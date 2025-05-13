#pragma once

#include "ECS/system.h"

#include "2D/simple2dobject.h"

#include "Input/inputcomponent.h"
#include "2D/collisionsystem.h"

#include "Systems/coresystems.h"

#include "Systems/basicsystems.h"

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

    struct CollectibleFlag {};

    struct PlayerMoveUp {};
    struct PlayerMoveDown {};
    struct PlayerMoveLeft {};
    struct PlayerMoveRight {};

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

            std::vector<size_t> collidableLayer = {0, 3};

            ecsRef->attach<CollisionComponent>(playerEnt.entity, 1, 1.0, collidableLayer);

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

                auto bullet = makeSimple2DShape(ecsRef, Shape2D::Square, 10.f, 10.f, {125.f, 125.f, 0.f, 255.f});
                bullet.get<PositionComponent>()->setX(pos->x + 25.f);
                bullet.get<PositionComponent>()->setY(pos->y + 25.f);
                bullet.get<PositionComponent>()->setZ(50);

                std::vector<size_t> collidableLayer = {0, 4};

                ecsRef->attach<CollisionComponent>(bullet.entity, 2, 1.0, collidableLayer);
                ecsRef->attach<AllyBulletFlag>(bullet.entity);
                ecsRef->attach<MoveToComponent>(bullet.entity, constant::Vector2D{event.pos.x, event.pos.y}, 500.f, 1000.0f);
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

        float movespeed = 2.f;
    };

} // namespace pg
