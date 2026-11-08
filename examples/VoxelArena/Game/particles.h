#pragma once

#include <random>
#include <vector>

#include "glm/glm.hpp"

#include "Core/effects.h"
#include "Core/voxelgrid.h"

namespace arena
{
    /** Little cubes thrown by hits, breaks and landings. They fall, bounce off the arena and shrink away. */
    class Particles
    {
    public:
        /** scale: the whole burst smaller and slower (the first person's weapon, drawn small right by the eye) */
        void burst(const ParticlePreset& preset, const glm::vec3& at, const glm::vec3& eventColor, float scale = 1.0f);
        void step(float dt, const VoxelGrid& grid, const BlockTable& table);

        struct Particle
        {
            glm::vec3 position;
            glm::vec3 velocity;
            glm::vec3 color;
            float size;
            float life;
            float maxLife;
            float gravity;
            float glow;
        };

        const std::vector<Particle>& all() const { return particles; }

        static constexpr size_t MAX = 1500;

    private:
        std::vector<Particle> particles;
        std::mt19937 rng{777};
    };
}
