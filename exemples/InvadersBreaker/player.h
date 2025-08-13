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

class GameStateSystem : public System<InitSys>
{
private:
    bool initialized = false;

public:
    std::string getSystemName() const override
    {
        return "Game Init System";
    }

    void init() override
    {
        spawnPaddle();
        // Grab ECS reference if available in init
        // ecs = getECS(); // or however you access it
    }

    void execute() override
    {
    }

private:
    void spawnPaddle()
    {
        // Position at bottom center of screen
        float startX = 410.0f - 75.0f; // Center of 820px screen
        float startY = 580.0f;         // Near bottom of 640px screen

        auto paddle = makeSimple2DShape(ecsRef, Shape2D::Square, 150, 40);

        auto pos = paddle.get<PositionComponent>();

        pos->setX(startX);
        pos->setY(startY);

        paddle.attach<Velocity>();
        paddle.attach<Paddle>();
    }
};