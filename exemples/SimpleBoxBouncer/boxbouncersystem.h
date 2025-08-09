// BoxBouncerSystem.h
#pragma once

#include <random>

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"

using namespace pg;

struct BouncingBox
{
    float velocityX;
    float velocityY;
    float speed;

    BouncingBox(float vx = 0.0f, float vy = 0.0f, float spd = 100.0f)
        : velocityX(vx), velocityY(vy), speed(spd) {}
};

class BoxBouncerSystem : public System<InitSys, Listener<TickEvent>>
{
private:
    float screenWidth;
    float screenHeight;
    std::mt19937 rng;
    std::uniform_real_distribution<float> colorDist;

    float deltaTime = 0.0f;

    EntityRef ent;

public:
    BoxBouncerSystem(float width = 820.0f, float height = 640.0f)
        : screenWidth(width), screenHeight(height), rng(std::random_device{}()), colorDist(0.0f, 255.0f) {}

    // Name of the system so it is easier to debug the taskflow
    virtual std::string getSystemName() const override { return "Box Bouncer System"; }
    
    void init() override
    {
        // Create a 2D square
        auto shape = makeSimple2DShape(ecsRef, Shape2D::Square,
            screenWidth / 2, screenHeight / 2,
            {255.0f, 100.0f, 100.0f, 255.0f}); // Red square

        auto pos = shape.get<PositionComponent>();

        // Set initial size
        pos->width = 120.0f;
        pos->height = 70.0f;

        // Add bouncing behavior component
        auto bouncer = shape.attachGeneric<BouncingBox>();
        bouncer->velocityX = 180.0f;  // pixels per second
        bouncer->velocityY = 180.0f;  // pixels per second
        bouncer->speed = 150.0f;

        ent = shape.entity;
    }

    virtual void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick / 1000.0f;
    }

    void execute() override
    {
        if (deltaTime == 0.0f)
            return;

        auto pos = ent->get<PositionComponent>();
        auto shape2D = ent->get<Simple2DObject>();
        auto bouncer = ent->get<BouncingBox>();

        if (not pos or not shape2D or not bouncer)
            return;

        // Update position
        auto x = pos->x + bouncer->velocityX * deltaTime;
        auto y = pos->y + bouncer->velocityY * deltaTime;

        // Check boundaries and bounce
        float width = pos->width;
        float height = pos->height;

        // Left/Right boundaries
        if (x <= 0 or x + width >= screenWidth)
        {
            bouncer->velocityX = -bouncer->velocityX;
            changeColor(shape2D);

            // Keep within bounds
            if (x <= 0)
                x = 0;
            else
                x = screenWidth - width;
        }

        // Top/Bottom boundaries
        if (y <= 0 or y + height >= screenHeight)
        {
            bouncer->velocityY = -bouncer->velocityY;
            changeColor(shape2D);

            // Keep within bounds
            if (y <= 0)
                y = 0;
            else
                y = screenHeight - height;
        }

        pos->setX(x);
        pos->setY(y);

        deltaTime = 0.0f;
    }

    void setScreenSize(float width, float height)
    {
        screenWidth = width;
        screenHeight = height;
    }

private:
    void changeColor(Simple2DObject* simple2D)
    {
        auto r = colorDist(rng), g = colorDist(rng), b = colorDist(rng);

        // Change to random color when bouncing
        simple2D->setColors({r, g, b, 255.0f});
    }
};