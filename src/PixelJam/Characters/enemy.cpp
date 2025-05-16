#include "enemy.h"

namespace pg
{
    constexpr float EPSILON = 1e-5f;

    void EnemyAISystem::chaseAndOrbit(PositionComponent* pos, AIStateComponent* ai, float dt)
    {
        ai->elapsedTime += dt;
        auto playerPos = findPlayerPosition();

        constant::Vector2D toPlayer { playerPos.x - pos->x, playerPos.y - pos->y };

        float dist = toPlayer.length();

        toPlayer.normalize();

        // Decide behavior
        float diff = dist - ai->idealDistance;

        constant::Vector2D moveDir;
        if (std::fabs(diff) < ai->orbitThreshold)
        {
            // Orbit: perpendicular
            if (ai->orbitDirection == -1.0f)
                moveDir = { -toPlayer.y,  toPlayer.x };
            else
                moveDir = {  toPlayer.y, -toPlayer.x };
        }
        else if (diff > 0)
        {
            // Too far: move in
            moveDir = toPlayer;
        } else
        {
            // Too close: kite out
            moveDir = { -toPlayer.x, -toPlayer.y };
        }

        auto collisionSys = ecsRef->getSystem<CollisionSystem>();

        // Apply movement
        // 2) Feelers for wall avoidance
        //    Cast three short sweeps and accumulate an avoidance vector.
        constant::Vector2D avoid{0, 0};

        float feelerLen = ai->feelersLength;      // e.g. 20–30 pixels
        float inv = 1.0f / sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
        
        // base heading
        constant::Vector2D dirNorm{ moveDir.x * inv, moveDir.y * inv };

        // helper to rotate the heading by angle θ (in radians)
        auto rotated = [&](constant::Vector2D d, float theta) {
            return constant::Vector2D {
                d.x * cos(theta) - d.y * sin(theta),
                d.x * sin(theta) + d.y * cos(theta)
            };
        };

        // sample angles: center, left, right
        std::array<float, 3> angles = { 0.f, +ai->feelerAngle, -ai->feelerAngle };
        for (float ang : angles) {
            constant::Vector2D fdir = rotated(dirNorm, ang);
            auto sweep = sweepMove(
                collisionSys,
                {pos->x, pos->y},
                {pos->width, pos->height},
                { fdir.x * feelerLen, fdir.y * feelerLen },
                { 0 }
            );

            if (sweep.hit) {
                // steer away: push in the opposite direction of that feeler
                avoid.x += -fdir.x * (feelerLen - sweep.delta.length());
                avoid.y += -fdir.y * (feelerLen - sweep.delta.length());
            }
        }

        // blend avoidance into moveDir
        const float avoidWeight = ai->avoidanceStrength; // e.g. 0.5
        constant::Vector2D finalDir = {
            moveDir.x + avoid.x * avoidWeight,
            moveDir.y + avoid.y * avoidWeight
        };
        // re‑normalize
        float lenFD = sqrt(finalDir.x * finalDir.x + finalDir.y * finalDir.y);
        if (lenFD > EPSILON) {
            finalDir.x /= lenFD;
            finalDir.y /= lenFD;
        }

        // 3) Apply collision‑aware movement (X then Y)
        float speedStep = ai->chaseSpeed;
        auto applX = sweepMove(
            collisionSys,
            {pos->x, pos->y},
            {pos->width, pos->height},
            {finalDir.x * speedStep, 0.f},
            {0}
        );

        auto applY = sweepMove(
            collisionSys,
            {pos->x, pos->y},
            {pos->width, pos->height},
            {0.f, finalDir.y * speedStep},
            {0}
        );

        bool blockedX = applX.hit && fabs(applX.delta.x) < EPSILON;
        bool blockedY = applY.hit && fabs(applY.delta.y) < EPSILON;

        if (blockedX && blockedY)
        {
            // Fully blocked: flip orbit and slide along one axis
            ai->orbitDirection *= -1.0f;

            // Try sliding along whichever axis had *less* penetration
            if (fabs(applX.delta.x) > fabs(applY.delta.y))
            {
                // slide horizontal
                pos->setX(pos->x + applX.delta.x);
            } 
            else 
            {
                // slide vertical
                pos->setY(pos->y + applY.delta.y);
            }
        } 
        else 
        {
            // Normal partial movement
            pos->setX(pos->x + (applX.hit ? applX.delta.x : (finalDir.x * speedStep)));
            pos->setY(pos->y + (applY.hit ? applY.delta.y : (finalDir.y * speedStep)));
        }

        auto checkDist = std::max(ai->attackDistance, ai->idealDistance);

        // Switch to attack if within range
        // Todo replace 45 by the actual size of the enemy (+ a small margin)
        if (dist - 45 <= checkDist and ai->elapsedTime > ai->cooldownTime)
        {
            ai->elapsedTime = 0.0f;
            ai->state = AIState::ShotWideUp;
        }
    }
} // namespace pg
