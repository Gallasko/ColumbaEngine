#pragma once

#include "components.h"
#include "events.h"

#include "2D/simple2dobject.h"

using namespace pg;

class PowerUpSystem : public System<InitSys, Listener<TickEvent>>
{
private:
    float powerUpChance = 0.15f;  // 15% drop rate

public:
    void init() override {
        registerGroup<PowerUp, PositionComponent, Velocity>();
        registerGroup<Paddle, PositionComponent>();
        registerGroup<Ball, PositionComponent, Velocity>();
    }

    void execute() override {
        checkPowerUpCollection();
        updatePowerUpPhysics();
    }

    void spawnPowerUp(float x, float y) {
        if ((rand() % 100) < (powerUpChance * 100)) {
            auto powerup = makeSimple2DShape(ecsRef, Shape2D::Circle, 24, 24);
            auto pos = powerup->get<PositionComponent>();
            pos->setX(x - 12);
            pos->setY(y);

            auto vel = powerup.attach<Velocity>();
            vel->dy = 120;  // Falls slowly

            // Randomly pick power-up type
            int type = rand() % 3;
            auto pu = powerup.attach<PowerUp>();

            switch(type) {
                case 0:  // Multi-ball - THE CHAOS MAKER
                    pu->type = PowerUp::MULTI_BALL;
                    powerup->get<Simple2DObject>()->setColors({255, 100, 255, 255}); // Purple
                    break;
                case 1:  // Wide paddle
                    pu->type = PowerUp::WIDE_PADDLE;
                    powerup->get<Simple2DObject>()->setColors({100, 100, 255, 255}); // Blue
                    break;
                case 2:  // Fast ball (risk/reward)
                    pu->type = PowerUp::FAST_BALL;
                    powerup->get<Simple2DObject>()->setColors({255, 100, 100, 255}); // Red
                    break;
            }

            // Pulsing effect
            powerup.attach<PulseEffect>();
        }
    }

    void applyMultiBall() {
        // Find the main ball
        Ball* mainBall = nullptr;
        PositionComponent* mainPos = nullptr;
        Velocity* mainVel = nullptr;

        for (auto entity : viewGroup<Ball, PositionComponent, Velocity>()) {
            mainBall = entity->get<Ball>();
            mainPos = entity->get<PositionComponent>();
            mainVel = entity->get<Velocity>();
            if (mainBall->launched) break;  // Find an active ball
        }

        if (!mainBall || !mainBall->launched) return;

        // Spawn 2 extra balls at slight angles
        for (int i = 0; i < 2; i++) {
            auto ball = makeSimple2DShape(ecsRef, Shape2D::Square, 16, 16, {255, 150, 150, 255});
            auto pos = ball.get<PositionComponent>();
            pos->setX(mainPos->x);
            pos->setY(mainPos->y);

            auto vel = ball.attach<Velocity>();
            float angle = (i == 0) ? -30 : 30;  // Degrees
            float radians = angle * 3.14159f / 180.0f;

            // Rotate main velocity
            vel->dx = mainVel->dx * cos(radians) - mainVel->dy * sin(radians);
            vel->dy = mainVel->dx * sin(radians) + mainVel->dy * cos(radians);

            auto newBall = ball.attach<Ball>();
            newBall->launched = true;  // Already in flight
            newBall->isExtra = true;   // Mark as extra ball for cleanup
        }

        // Screen shake for impact
        ecsRef->sendEvent(ScreenShakeEvent{0.4f});
    }
};
