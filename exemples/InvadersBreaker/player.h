#pragma once

#include <random>

#include "components.h"

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"

using namespace pg;

class PaddleControlSystem : public System<InitSys, Listener<TickEvent>, Listener<OnSDLScanCode>, Listener<OnSDLScanCodeReleased>>
{
private:
    // Register the group we'll query
    float deltaTime = 0.0f;

    std::vector<bool> keyPressed = {false, false};

public:
    std::string getSystemName() const override { return "Paddle Control System"; }

    void init() override
    {
        // Register our view group - THIS IS CRITICAL
        registerGroup<PositionComponent, Velocity, Paddle>();
    }

    void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick;
    }

    void onEvent(const OnSDLScanCode& event) override
    {
        if (event.key == SDL_SCANCODE_A)
            keyPressed[0] = true;

        if (event.key == SDL_SCANCODE_D)
            keyPressed[1] = true;
    }

    void onEvent(const OnSDLScanCodeReleased& event) override
    {
        if (event.key == SDL_SCANCODE_A)
            keyPressed[0] = false;

        if (event.key == SDL_SCANCODE_D)
            keyPressed[1] = false;
    }

    void execute() override
    {
        if (deltaTime == 0.0f)
            return;

        // Get delta time
        float dt = deltaTime / 1000;
        deltaTime = 0.0f;

        // Iterate through our registered group
        for (auto entity : viewGroup<PositionComponent, Velocity, Paddle>())
        {
            auto pos = entity->get<PositionComponent>();
            auto vel = entity->get<Velocity>();
            auto paddle = entity->get<Paddle>();

            // Reset velocity
            vel->dx = 0;

            if (keyPressed[0])
                vel->dx = -paddle->speed;

            if (keyPressed[1])
                vel->dx = paddle->speed;

            // Apply movement
            auto x = pos->x + vel->dx * dt;

            // Screen boundaries (assuming 800x600)
            const float SCREEN_WIDTH = 820.0f;
            const float MARGIN = 10.0f;

            if (x < MARGIN)
            {
                x = MARGIN;
                vel->dx = 0;
            }
            if (x + pos->width > SCREEN_WIDTH - MARGIN)
            {
                x = SCREEN_WIDTH - MARGIN - pos->width;
                vel->dx = 0;
            }

            pos->setX(x);
        }
    }
};

class BallPhysicsSystem : public System<InitSys, Listener<TickEvent>, Listener<OnSDLScanCode>>
{
private:
    float deltaTime = 0.0f;
    const float SCREEN_WIDTH = 820.0f;
    const float SCREEN_HEIGHT = 640.0f;

public:
    std::string getSystemName() const override { return "Ball Physics System"; }

    void init() override
    {
        registerGroup<PositionComponent, Velocity, Ball>();
        registerGroup<PositionComponent, Paddle>();  // Need paddle position for following
    }

    void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick;
    }

    void onEvent(const OnSDLScanCode& event) override
    {
        if (event.key == SDL_SCANCODE_SPACE)
        {
            launchBall();
        }
    }

    void execute() override
    {
        if (deltaTime == 0.0f)
            return;

        float dt = deltaTime / 1000.0f;
        deltaTime = 0.0f;

        // Process each ball
        for (auto ballEntity : viewGroup<PositionComponent, Velocity, Ball>())
        {
            auto ballPos = ballEntity->get<PositionComponent>();
            auto ballVel = ballEntity->get<Velocity>();
            auto ball = ballEntity->get<Ball>();

            if (not ball->launched)
            {
                // Follow paddle when not launched
                followPaddle(ballPos, ballVel);
            }
            else
            {
                // Apply physics
                updatePhysics(ballPos, ballVel, ball, dt);
            }
        }
    }

private:
    void followPaddle(PositionComponent* ballPos, Velocity* ballVel)
    {
        // Find paddle and stick ball to it
        for (auto paddleEntity : viewGroup<PositionComponent, Paddle>())
        {
            auto paddlePos = paddleEntity->get<PositionComponent>();

            // Center ball on paddle, slightly above
            ballPos->setX(paddlePos->x + paddlePos->width / 2.0f - ballPos->width / 2.0f);
            ballPos->setY(paddlePos->y - 25.0f);

            // Zero velocity while attached
            ballVel->dx = 0;
            ballVel->dy = 0;

            break; // Only one paddle
        }
    }

    void launchBall()
    {
        for (auto ballEntity : viewGroup<PositionComponent, Velocity, Ball>())
        {
            auto ball = ballEntity->get<Ball>();
            auto vel = ballEntity->get<Velocity>();

            if (not ball->launched)
            {
                ball->launched = true;

                // Random slight angle for variety
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<> dis(-100.0, 100.0);

                vel->dx = dis(gen);
                vel->dy = -ball->speed;

                printf("Ball launched! vx=%.1f, vy=%.1f\n", vel->dx, vel->dy);
                break; // Only launch one ball at a time
            }
        }
    }

    void updatePhysics(PositionComponent* pos, Velocity* vel, Ball* ball, float dt)
    {
        // Update position
        float newX = pos->x + vel->dx * dt;
        float newY = pos->y + vel->dy * dt;

        // Left/Right wall collision
        if (newX <= 0)
        {
            newX = 0;
            vel->dx = abs(vel->dx);  // Bounce right
            // TODO: Add sound/particle effect here
        }
        else if (newX + pos->width >= SCREEN_WIDTH)
        {
            newX = SCREEN_WIDTH - pos->width;
            vel->dx = -abs(vel->dx); // Bounce left
        }

        // Top wall collision
        if (newY <= 0)
        {
            newY = 0;
            vel->dy = abs(vel->dy);  // Bounce down
        }

        // Bottom = death (for now just reset)
        if (newY >= SCREEN_HEIGHT)
        {
            // Reset ball
            ball->launched = false;
            printf("Ball lost! Resetting...\n");
            // TODO: Trigger life loss event
        }

        // Apply new position
        pos->setX(newX);
        pos->setY(newY);
    }
};

// Simple collision detection for paddle bouncing
class EntityCollisionSystem : public System<InitSys, Listener<TickEvent>>
{
private:
    float deltaTime = 0.0f;

public:
    std::string getSystemName() const override
    {
        return "Collision System";
    }

    void init() override
    {
        registerGroup<PositionComponent, Velocity, Ball>();
        registerGroup<PositionComponent, Paddle>();
    }

    void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick;
    }

    void execute() override
    {
        if (deltaTime == 0.0f)
            return;

        deltaTime = 0.0f;  // Reset after use

        // Check ball vs paddle collision
        for (auto ballEntity : viewGroup<PositionComponent, Velocity, Ball>())
        {
            auto ballPos = ballEntity->get<PositionComponent>();
            auto ballVel = ballEntity->get<Velocity>();
            auto ball = ballEntity->get<Ball>();

            // Skip if ball not launched
            if (not ball->launched)
                continue;

            for (auto paddleEntity : viewGroup<PositionComponent, Paddle>())
            {
                auto paddlePos = paddleEntity->get<PositionComponent>();

                // Simple AABB collision
                if (checkCollision(ballPos, paddlePos))
                {
                    handlePaddleBounce(ballPos, ballVel, paddlePos);
                }
            }
        }
    }

private:
    bool checkCollision(PositionComponent* a, PositionComponent* b)
    {
        return a->x < b->x + b->width   and
                a->x + a->width > b->x  and
                a->y < b->y + b->height and
                a->y + a->height > b->y;
    }

    void handlePaddleBounce(PositionComponent* ballPos, Velocity* ballVel,
                            PositionComponent* paddlePos)
    {
        // Only bounce if ball is moving downward
        if (ballVel->dy > 0)
        {
            // Calculate hit position (-1 to 1)
            float paddleCenter = paddlePos->x + paddlePos->width / 2.0f;
            float ballCenter = ballPos->x + ballPos->width / 2.0f;
            float hitPos = (ballCenter - paddleCenter) / (paddlePos->width / 2.0f);

            // Clamp to reasonable range
            hitPos = std::max(-0.8f, std::min(0.8f, hitPos));

            // Angle the bounce based on hit position
            ballVel->dx = hitPos * 250.0f;  // Stronger angle = more horizontal speed
            ballVel->dy = -abs(ballVel->dy); // Always bounce up

            // Prevent ball from getting stuck in paddle
            ballPos->setY(paddlePos->y - ballPos->height - 1);

            printf("Paddle bounce! Hit position: %.2f\n", hitPos);
        }
    }
};
