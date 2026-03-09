#pragma once

#include "ECS/entitysystem.h"
#include "ECS/standardsystem.h"
#include "Systems/coresystems.h"
#include "2D/simple2dobject.h"

using namespace pg;

// Particle component to track lifetime
struct Particle : public Component
{
    DEFAULT_COMPONENT_MEMBERS(Particle)

    float lifetime = 500.0f;  // milliseconds
    float elapsed = 0.0f;
};

// Velocity component for particle movement
struct ParticleVelocity : public Component
{
    DEFAULT_COMPONENT_MEMBERS(ParticleVelocity)

    float dx = 0.0f;
    float dy = 0.0f;
};

// Particle system that updates and manages particles
class ParticleSystem : public System<InitSys, DeltaTime>
{
public:
    void init() override
    {
        registerGroup<Particle, PositionComponent, ParticleVelocity>();
    }

    void onExecute(float deltaTime) override
    {
        std::vector<EntityRef> toDestroy;

        for (auto entity : viewGroup<Particle, PositionComponent, ParticleVelocity>())
        {
            auto particle = entity->get<Particle>();
            auto pos = entity->get<PositionComponent>();
            auto vel = entity->get<ParticleVelocity>();

            particle->elapsed += deltaTime * 1000.0f;  // deltaTime is in seconds

            // Update position
            pos->setX(pos->x + vel->dx * deltaTime);
            pos->setY(pos->y + vel->dy * deltaTime);

            // Fade out over lifetime
            if (auto shape = entity->get<Simple2DObject>())
            {
                float alpha = 1.0f - (particle->elapsed / particle->lifetime);
                alpha = std::max(0.0f, std::min(1.0f, alpha));  // Clamp to [0, 1]

                auto colors = shape->colors;
                colors.w = (uint8_t)(255 * alpha);
                shape->setColors(colors);
            }

            // Mark for destruction when lifetime expires
            if (particle->elapsed >= particle->lifetime)
            {
                toDestroy.push_back(entity->entity);
            }
        }

        // Clean up dead particles
        for (auto& entity : toDestroy)
        {
            ecsRef->removeEntity(entity);
        }
    }
};