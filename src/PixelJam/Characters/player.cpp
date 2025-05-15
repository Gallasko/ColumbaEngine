#include "player.h"

namespace pg
{
    void PlayerSystem::movePlayer(float x, float y)
    {
        auto pos = player->get<PositionComponent>();

        auto collisionSys = ecsRef->getSystem<CollisionSystem>();

        constant::Vector2D posVec = {pos->x, pos->y};
        constant::Vector2D size = {pos->width, pos->height};

        // pos->setX(pos->x + x);
        // pos->setY(pos->y + y);

        // Todo fix this
        // This is a randabouty way to do it because attach CollisionComponent is buggy
        auto applX = sweepMove(collisionSys, {pos->x, pos->y}, size, {x, 0.0f}, {0});

        if (applX.entity and applX.entity->has<WallFlag>())
        {
            pos->setX(pos->x + applX.delta.x);
            // pos->setY(pos->y + appl.delta.y);
        }
        else
        {
            pos->setX(pos->x + x);
            // pos->setY(pos->y + y);
        }

        auto applY = sweepMove(collisionSys, {pos->x, pos->y}, size, {0.0f, y}, {0});

        if (applY.entity and applY.entity->has<WallFlag>())
        {
            // pos->setX(pos->x + appl.delta.x);
            pos->setY(pos->y + applY.delta.y);
        }
        else
        {
            // pos->setX(pos->x + x);
            pos->setY(pos->y + y);
        }

        // // 1) sweep X only
        // auto applX = sweepMove(collisionSys, {pos->x, pos->y}, size, {x, 0.0f}, {0});
        // pos->setX(pos->x + applX.delta.x);
        // // pos->setY(pos->y + applX.delta.y);

        // // 2) sweep Y only (using the **new** X position!)
        // auto applY = sweepMove(collisionSys, {pos->x, pos->y}, size, {0.0f, y}, {0});
        // // pos->setX(pos->x + applY.delta.x);
        // pos->setY(pos->y + applY.delta.y);


        // LOG_INFO("Player", "Hit X: " << applX.delta.x << ", hit Y: " << applY.delta.y);

        // // optional: detect if either axis was blocked
        // if (hitX or hitY)
        // {
        //     LOG_INFO("Player", "Hit wall on " << (hitX ? "X" : "") << (hitY ? "Y" : ""));
        // }
    }
} // namespace pg
