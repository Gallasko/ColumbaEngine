#pragma once

#include "ECS/system.h"

#include "2D/simple2dobject.h"

#include "Systems/coresystems.h"

#include "../config.h"

namespace pg
{

    struct PlayerSystem : public System<StoragePolicy, Listener<ConfiguredKeyEvent<GameKeyConfig>>, Listener<ConfiguredKeyEventReleased<GameKeyConfig>>, InitSys>
    {
        virtual std::string getSystemName() const override { return "Player System"; }

        virtual void init() override
        {
            auto playerEnt = makeSimple2DShape(ecsRef, Shape2D::Square, 50.f, 50.f, {0.f, 255.f, 0.f, 255.f});

            ecsRef->attach<EntityName>(playerEnt.entity, "Player");

            player = playerEnt.entity;
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
