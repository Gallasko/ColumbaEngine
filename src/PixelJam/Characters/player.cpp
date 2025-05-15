#include "player.h"

namespace pg
{
    void PlayerSystem::movePlayer(float x, float y)
    {
        auto pos = player->get<PositionComponent>();

        auto collisionSys = ecsRef->getSystem<CollisionSystem>();

        constant::Vector2D posVec = {pos->x, pos->y};
        constant::Vector2D size = {pos->width + 2, pos->height + 2};

        bool hitX, hitY;

        // pos->setX(pos->x + x);
        // pos->setY(pos->y + y);

        // 1) sweep X only
        auto applX = sweepMove(collisionSys, {pos->x, pos->y}, size, {x, 0.0f}, {0}, hitX);
        pos->setX(pos->x + applX.x);

        // 2) sweep Y only (using the **new** X position!)
        auto applY = sweepMove(collisionSys, {pos->x, pos->y}, size, {0.0f, y}, {0}, hitY);
        pos->setY(pos->y + applY.y);

        LOG_INFO("Player", "Hit X: " << applX.x << ", hit Y: " << applY.y);

        // optional: detect if either axis was blocked
        if (hitX or hitY)
        {
            LOG_INFO("Player", "Hit wall on " << (hitX ? "X" : "") << (hitY ? "Y" : ""));
        }
    }
} // namespace pg
