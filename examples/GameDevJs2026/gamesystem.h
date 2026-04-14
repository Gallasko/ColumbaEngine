#pragma once

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"

using namespace pg;

class GameSystem : public System<InitSys, Listener<TickEvent>>
{
public:
    GameSystem(float width = 820.0f, float height = 640.0f)
        : screenWidth(width), screenHeight(height) {}

    virtual std::string getSystemName() const override { return "Game System"; }

    void init() override
    {
        // Create a simple square to confirm rendering works
        auto shape = makeSimple2DShape(ecsRef, Shape2D::Square,
            screenWidth / 2 - 50, screenHeight / 2 - 50,
            {100.0f, 180.0f, 255.0f, 255.0f});

        auto pos = shape.get<PositionComponent>();
        pos->width = 100.0f;
        pos->height = 100.0f;
    }

    virtual void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick / 1000.0f;
    }

    void execute() override
    {
        if (deltaTime == 0.0f)
            return;

        // Idle game logic goes here

        deltaTime = 0.0f;
    }

private:
    float screenWidth;
    float screenHeight;
    float deltaTime = 0.0f;
};
