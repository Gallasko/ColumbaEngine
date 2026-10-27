#include "orbitcamera.h"

#include <cmath>

#include "world.h"

namespace arena
{
    namespace
    {
        constexpr float DEG = 3.14159265358979f / 180.0f;

        glm::vec3 viewDirection(float yawDegrees, float pitchDegrees)
        {
            const float y = yawDegrees * DEG, p = pitchDegrees * DEG;
            return glm::vec3(std::sin(y) * std::cos(p), -std::sin(p), std::cos(y) * std::cos(p));
        }

        float smoothTowards(float current, float target, float sharpness, float dt)
        {
            return current + (target - current) * (1.0f - std::exp(-sharpness * dt));
        }

        // Smooth, cheap noise in -1..1: a few incommensurate sines
        float wobble(float t, float seed)
        {
            return 0.5f * std::sin(t * 1.0f + seed) + 0.3f * std::sin(t * 2.37f + seed * 1.7f) + 0.2f * std::sin(t * 4.11f + seed * 2.3f);
        }

        /** Free distance from origin along dir, up to maxDist (minus nothing: the caller keeps its margin). */
        float clearance(const VoxelGrid& grid, const BlockTable& table, const glm::vec3& origin, const glm::vec3& dir, float maxDist)
        {
            const RayHit hit = raycast(grid, table, origin, dir, maxDist);
            return hit.hit ? hit.distance : maxDist;
        }
    }

    glm::vec3 OrbitCamera::forward() const
    {
        return viewDirection(viewYaw, viewPitch);
    }

    void OrbitCamera::look(float dx, float dy, const Tuning::Camera& t)
    {
        // Mouse right turns right: game yaw grows to the left
        yaw -= dx * t.sensitivity;
        pitch = glm::clamp(pitch + dy * t.sensitivity, t.minPitch, t.maxPitch);

        if (yaw > 180.0f)
            yaw -= 360.0f;
        if (yaw < -180.0f)
            yaw += 360.0f;
    }

    void OrbitCamera::zoom(float steps, const Tuning::Camera& t)
    {
        distance = glm::clamp(distance - steps * t.zoomStep, t.minDistance, t.maxDistance);
    }

    void OrbitCamera::update(float dt, const glm::vec3& head, const VoxelGrid& grid, const BlockTable& table, const Tuning& tuning, float time)
    {
        const auto& t = tuning.camera;

        pitch = glm::clamp(pitch, t.minPitch, t.maxPitch);
        distance = glm::clamp(distance, t.minDistance, t.maxDistance);

        // Over the right shoulder, but never through a wall beside the head
        const glm::vec3 right = localToWorld(yaw, glm::vec3(1, 0, 0));
        const float shoulder = std::max(0.0f, clearance(grid, table, head, right, t.shoulder + t.collisionRadius) - t.collisionRadius);
        const glm::vec3 wantedPivot = head + right * shoulder;

        if (not placed)
        {
            pivot = wantedPivot;
            currentDistance = distance;
            placed = true;
        }
        else
        {
            const float k = 1.0f - std::exp(-t.followSharpness * dt);
            pivot += (wantedPivot - pivot) * k;
        }

        // The smoothed pivot lags behind the head: it must not lag into a block
        {
            const glm::vec3 toPivot = pivot - head;
            const float len = glm::length(toPivot);

            if (len > 1e-4f)
            {
                const float free = clearance(grid, table, head, toPivot / len, len + t.collisionRadius);

                if (free < len + t.collisionRadius)
                    pivot = head + toPivot / len * std::max(0.0f, free - t.collisionRadius);
            }
        }

        // Shake
        trauma = std::max(0.0f, trauma - tuning.feel.shakeDecay * dt);
        const float shake = trauma * trauma;
        const float ft = time * tuning.feel.shakeFrequency;

        viewYaw = yaw + wobble(ft, 1.3f) * tuning.feel.shakeAngle * shake;
        viewPitch = pitch + wobble(ft, 7.1f) * tuning.feel.shakeAngle * shake;

        const glm::vec3 back = -viewDirection(viewYaw, viewPitch);
        const glm::vec3 camRight = localToWorld(viewYaw, glm::vec3(1, 0, 0));
        const glm::vec3 camUp = glm::normalize(glm::cross(camRight, back));

        // A thick ray: the centre and four rays offset by the camera's radius
        float free = clearance(grid, table, pivot, back, distance + t.collisionRadius);

        for (const glm::vec2 o : {glm::vec2(1, 1), glm::vec2(-1, 1), glm::vec2(1, -1), glm::vec2(-1, -1)})
        {
            const glm::vec3 offset = (camRight * o.x + camUp * o.y) * (t.collisionRadius * 0.7f);
            free = std::min(free, clearance(grid, table, pivot + offset, back, distance + t.collisionRadius));
        }

        const float limit = std::max(0.0f, free - t.collisionRadius);
        blocked = limit < distance - 1e-3f;

        // In at once (never let a wall come between the camera and the player), out slowly
        if (limit < currentDistance)
            currentDistance = limit;
        else
            currentDistance = smoothTowards(currentDistance, limit, t.pullOutSharpness, dt);

        position = pivot + back * currentDistance;
        position += (camRight * wobble(ft, 3.7f) + camUp * wobble(ft, 9.2f)) * (tuning.feel.shakeOffset * shake);
    }
}
