#pragma once

#include "components.h"
#include "events.h"

#include "Systems/basicsystems.h"

using namespace pg;

class ScreenShakeSystem : public System<InitSys, QueuedListener<TickEvent>, Listener<ScreenShakeEvent>, Listener<AlienDestroyedEvent>, Listener<PlayerHitEvent>>
{
private:
    EntityRef camera;
    float offsetX = 0, offsetY = 0;

public:
    void init() override {
        // Create a camera entity to hold shake state
        camera = ecsRef->createEntity("Camera");
        camera.attach<ScreenShake>();
    }

    void onEvent(const AlienDestroyedEvent& event) override {
        addTrauma(0.3f);
    }

    void onEvent(const PlayerHitEvent& event) override {
        addTrauma(0.5f);
    }

    void onEvent(const ScreenShakeEvent& event) override
    {
        addTrauma(event.trauma);
    }

    void onProcessEvent(const TickEvent& event) override {
        float dt = event.tick / 1000.0f;

        auto shake = camera->get<ScreenShake>();
        if (shake->trauma > 0) {
            shake->trauma = std::max(0.0f, shake->trauma - dt);

            float shakeAmount = shake->trauma * shake->trauma; // Quadratic falloff
            offsetX = ((rand() % 200) - 100) / 100.0f * shake->maxOffset * shakeAmount;
            offsetY = ((rand() % 200) - 100) / 100.0f * shake->maxOffset * shakeAmount;

            // Apply to all visible entities - crude but works
            applyShakeToAll(offsetX, offsetY);
        }
    }

private:
    void addTrauma(float amount) {
        auto shake = camera->get<ScreenShake>();
        shake->trauma = std::min(1.0f, shake->trauma + amount);
    }

    void applyShakeToAll(float x, float y) {
        // This is hacky but for a jam it works
        static float lastOffsetX = 0, lastOffsetY = 0;

        // Revert last offset and apply new one
        for (auto entity : viewGroup<PositionComponent>()) {
            auto pos = entity->get<PositionComponent>();
            pos->setX(pos->x - lastOffsetX + x);
            pos->setY(pos->y - lastOffsetY + y);
        }

        lastOffsetX = x;
        lastOffsetY = y;
    }
};