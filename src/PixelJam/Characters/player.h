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

    struct PlayerSystem : public System<QueuedListener<OnMouseClick>, Listener<ConfiguredKeyEvent<GameKeyConfig>>, Listener<ConfiguredKeyEventReleased<GameKeyConfig>>, InitSys>
    {
        virtual std::string getSystemName() const override { return "Player System"; }

        virtual void init() override
        {
            auto playerEnt = makeSimple2DShape(ecsRef, Shape2D::Square, 50.f, 50.f, {0.f, 255.f, 0.f, 255.f});

            ecsRef->attach<EntityName>(playerEnt.entity, "Player");

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

                ecsRef->attach<CollisionComponent>(bullet.entity);
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
