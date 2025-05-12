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
    struct WallFlag {};
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

    struct PlayerSystem : public System<QueuedListener<OnMouseClick>, Listener<ConfiguredKeyEvent<GameKeyConfig>>, Listener<ConfiguredKeyEventReleased<GameKeyConfig>>, InitSys>
    {
        virtual std::string getSystemName() const override { return "Player System"; }

        virtual void init() override
        {
            auto playerEnt = makeSimple2DShape(ecsRef, Shape2D::Square, 50.f, 50.f, {0.f, 255.f, 0.f, 255.f});

            ecsRef->attach<EntityName>(playerEnt.entity, "Player");
            ecsRef->attach<PlayerFlag>(playerEnt.entity);

            std::vector<size_t> collidableLayer = {0, 3};

            ecsRef->attach<CollisionComponent>(playerEnt.entity, 1, 1.0, collidableLayer);

            player = playerEnt.entity;
        }

        virtual void onProcessEvent(const OnMouseClick& event) override
        {
            if (event.button == SDL_BUTTON_LEFT)
            {
                auto pos = player->get<PositionComponent>();

                auto bullet = makeSimple2DShape(ecsRef, Shape2D::Square, 10.f, 10.f, {125.f, 125.f, 0.f, 255.f});
                bullet.get<PositionComponent>()->setX(pos->x + 25.f);
                bullet.get<PositionComponent>()->setY(pos->y + 25.f);

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
                moveLeft();
                break;
            case GameKeyConfig::MoveRight:
                moveRight();
                break;
            case GameKeyConfig::MoveUp:
                moveUp();
                break;
            case GameKeyConfig::MoveDown:
                moveDown();
                break;

            default:
                break;
            }

        }

        virtual void onEvent(const ConfiguredKeyEventReleased<GameKeyConfig>& event) override
        {
            LOG_INFO("Player System", "Received game key release");
        }

        void movePlayer(float x, float y)
        {
            auto pos = player->get<PositionComponent>();
            pos->setX(pos->x + x);
            pos->setY(pos->y + y);
        }

        void moveLeft()
        {
            auto pos = player->get<PositionComponent>();
            pos->setX(pos->x - 1.f);
        }

        void moveRight()
        {
            auto pos = player->get<PositionComponent>();
            pos->setX(pos->x + 1.f);
        }

        void moveUp()
        {
            auto pos = player->get<PositionComponent>();
            pos->setY(pos->y - 1.f);
        }

        void moveDown()
        {
            auto pos = player->get<PositionComponent>();
            pos->setY(pos->y + 1.f);
        }

        EntityRef player;
    };

} // namespace pg
