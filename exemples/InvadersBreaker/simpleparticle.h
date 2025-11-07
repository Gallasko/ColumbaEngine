#pragma once

#include "components.h"
#include "events.h"

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"

using namespace pg;

class SimpleParticleSystem : public System<InitSys, Listener<TickEvent>, Listener<AlienDestroyedEvent>>
{
public:
    void init() override {
        registerGroup<Particle, PositionComponent, Velocity>();
    }

    void onEvent(const AlienDestroyedEvent& event) override {
        // Spawn particles at alien death position
        spawnExplosion(event.x, event.y);
    }

    void onEvent(const TickEvent& event) override {
        float dt = event.tick;

        std::vector<EntityRef> toDestroy;

        for (auto entity : viewGroup<Particle, PositionComponent, Velocity>()) {
            auto particle = entity->get<Particle>();
            auto pos = entity->get<PositionComponent>();
            auto vel = entity->get<Velocity>();

            particle->elapsed += dt;

            // Simple physics
            pos->setX(pos->x + vel->dx * dt / 1000.0f);
            pos->setY(pos->y + vel->dy * dt / 1000.0f);

            // Gravity
            vel->dy += 200 * dt / 1000.0f;

            // Fade out
            if (auto shape = entity->get<Simple2DObject>())
            {
                float alpha = 1.0f - (particle->elapsed / particle->lifetime);
                auto colors = shape->colors;
                colors.w = (uint8_t)(255 * alpha);
                shape->setColors(colors);
            }

            // Mark for destruction
            if (particle->elapsed >= particle->lifetime) {
                toDestroy.push_back(entity->entity);
            }
        }

        // Clean up dead particles
        for (auto& entity : toDestroy) {
            ecsRef->removeEntity(entity);
        }
    }

private:
    void spawnExplosion(float x, float y) {
        for (int i = 0; i < 6; i++) {
            auto particle = makeSimple2DShape(ecsRef, Shape2D::Square, 4, 4, {255, 200, 100, 255});
            auto pos = particle.get<PositionComponent>();
            pos->setX(x);
            pos->setY(y);

            auto vel = particle.attach<Velocity>();
            float angle = (i / 6.0f) * 6.28f;
            vel->dx = cos(angle) * 150 + (rand() % 100 - 50);
            vel->dy = sin(angle) * 150 - 100;  // Bias upward

            particle.attach<Particle>();
        }
    }
};