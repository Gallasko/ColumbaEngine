#include "particles.h"

#include <algorithm>
#include <cmath>

namespace arena
{
    void Particles::burst(const ParticlePreset& p, const glm::vec3& at, const glm::vec3& eventColor)
    {
        std::uniform_real_distribution<float> u(-1.0f, 1.0f);
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);

        for (int i = 0; i < p.count and particles.size() < MAX; ++i)
        {
            // A random direction, pulled upwards by `up`
            glm::vec3 dir(u(rng), u(rng), u(rng));
            dir.y = std::fabs(dir.y) * p.up + dir.y * (1.0f - p.up);
            dir = glm::length(dir) > 1e-4f ? glm::normalize(dir) : glm::vec3(0, 1, 0);

            const glm::vec3 base = p.eventColor ? eventColor : p.color;

            Particle q;
            q.position = at + glm::vec3(u(rng), u(rng), u(rng)) * 0.15f;
            q.velocity = dir * p.speed * (0.5f + 0.5f * unit(rng));
            q.color = base * (0.8f + 0.35f * unit(rng));
            q.size = p.size * (0.7f + 0.6f * unit(rng));
            q.maxLife = q.life = p.life * (0.7f + 0.6f * unit(rng));
            q.gravity = p.gravity;
            particles.push_back(q);
        }
    }

    void Particles::step(float dt, const VoxelGrid& grid, const BlockTable& table)
    {
        for (auto& q : particles)
        {
            q.life -= dt;
            q.velocity.y -= q.gravity * dt;
            q.velocity *= std::exp(-1.5f * dt);   // a little air drag

            glm::vec3 next = q.position + q.velocity * dt;

            // Into a block: bounce off the face it came through, losing most of the speed
            const glm::ivec3 cell(glm::floor(next));

            if (grid.isSolid(table, cell.x, cell.y, cell.z))
            {
                const glm::ivec3 from(glm::floor(q.position));

                for (int k = 0; k < 3; ++k)
                {
                    if (from[k] != cell[k])
                    {
                        q.velocity[k] *= -0.3f;
                        next[k] = q.position[k];
                    }
                }

                q.velocity *= 0.6f;
            }

            q.position = next;
        }

        particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& q) { return q.life <= 0.0f; }), particles.end());
    }
}
