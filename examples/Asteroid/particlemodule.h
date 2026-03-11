#pragma once

#include "Compiler/native_module.h"
#include "ECS/entitysystem.h"
#include "2D/simple2dobject.h"
#include "particle_system.h"
#include <cmath>
#include <cstdlib>

namespace pg
{
    /**
     * Particle Module for Asteroid game
     * Provides function to spawn particle effects
     */
    class ParticleModule : public NativeModule
    {
    public:
        ParticleModule(EntitySystem* ecsRef)
        {
            // Capture ecsRef for use in native functions
            auto ecsRefCopy = ecsRef;

            // Add spawnParticles function
            // Usage: spawnParticles(x, y, count, colorHex, size)
            // Example: spawnParticles(100, 200, 8, 0x333333, 3)
            addNativeFunction("spawnParticles", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount != 5)
                {
                    throw std::runtime_error("spawnParticles expects 5 arguments (x, y, count, colorHex, size)");
                }

                // Validate all arguments are numbers
                if (!IS_DOUBLE(args[0]) || !IS_DOUBLE(args[1]) || !IS_INT(args[2]) || !IS_INT(args[3]) || !IS_DOUBLE(args[4]))
                {
                    throw std::runtime_error("spawnParticles expects (float x, float y, int count, int colorHex, float size)");
                }

                float posX = AS_DOUBLE(args[0]);
                float posY = AS_DOUBLE(args[1]);
                int particleCount = static_cast<int>(AS_INT(args[2]));
                int64_t color = AS_INT(args[3]);
                float particleSize = AS_DOUBLE(args[4]);

                // Extract RGB from packed integer (0xRRGGBB format)
                uint8_t r = (color >> 16) & 0xFF;
                uint8_t g = (color >> 8) & 0xFF;
                uint8_t b = color & 0xFF;

                // Spawn particles in random directions
                for (int i = 0; i < particleCount; i++)
                {
                    auto particle = makeSimple2DShape(ecsRefCopy, Shape2D::Square, particleSize, particleSize, {r, g, b, 255});

                    auto pos = particle.get<PositionComponent>();
                    pos->setX(posX);
                    pos->setY(posY);

                    auto vel = particle.attach<ParticleVelocity>();

                    // Random angle (0 to 2π)
                    float angle = ((float)rand() / RAND_MAX) * 6.28318f;
                    // Random speed between 50 and 150 pixels/second
                    float speed = 50.0f + ((float)rand() / RAND_MAX) * 100.0f;

                    vel->dx = cos(angle) * speed;
                    vel->dy = sin(angle) * speed;

                    auto particleComp = particle.attach<Particle>();
                    // Random lifetime between 300ms and 700ms
                    particleComp->lifetime = 300.0f + ((float)rand() / RAND_MAX) * 400.0f;
                }

                return makeIntValue(particleCount);
            });
        }
    };
}