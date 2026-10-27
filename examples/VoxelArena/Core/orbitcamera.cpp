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
        look(dx, dy, t.sensitivity, t.minPitch, t.maxPitch);
    }

    void OrbitCamera::look(float dx, float dy, float sensitivity, float minPitch, float maxPitch)
    {
        // Mouse right turns right: game yaw grows to the left
        yaw -= dx * sensitivity;
        pitch = glm::clamp(pitch + dy * sensitivity, minPitch, maxPitch);

        if (yaw > 180.0f)
            yaw -= 360.0f;
        if (yaw < -180.0f)
            yaw += 360.0f;
    }

    void OrbitCamera::zoom(float steps, const Tuning::Camera& t)
    {
        distance = glm::clamp(distance - steps * t.zoomStep, t.minDistance, t.maxDistance);
    }

    void OrbitCamera::updateFirstPerson(float dt, const glm::vec3& eye, const Tuning& tuning, float time)
    {
        pitch = glm::clamp(pitch, tuning.firstPerson.minPitch, tuning.firstPerson.maxPitch);

        trauma = std::max(0.0f, trauma - tuning.feel.shakeDecay * dt);
        const float shake = trauma * trauma;
        const float ft = time * tuning.feel.shakeFrequency;

        viewYaw = yaw + wobble(ft, 1.3f) * tuning.feel.shakeAngle * shake;
        viewPitch = pitch + wobble(ft, 7.1f) * tuning.feel.shakeAngle * shake;

        // Smaller positional shake: up close it reads much stronger than behind the player
        const glm::vec3 right = localToWorld(viewYaw, glm::vec3(1, 0, 0));
        position = eye + (right * wobble(ft, 3.7f) + glm::vec3(0, wobble(ft, 9.2f), 0)) * (tuning.feel.shakeOffset * 0.3f * shake);

        // Coming back to third person starts from the player, not from where the orbit was left
        placed = false;
        pitchDrop = 0.0f;
        blocked = false;
    }

    void OrbitCamera::update(float dt, const glm::vec3& head, const VoxelGrid& grid, const BlockTable& table, const Tuning& tuning, float time)
    {
        update(dt, head, head, grid, table, tuning, time);
    }

    void OrbitCamera::update(float dt, const glm::vec3& wantedHead, const glm::vec3& body, const VoxelGrid& grid, const BlockTable& table, const Tuning& tuning, float time)
    {
        const auto& t = tuning.camera;

        // The camera's pivot sits above the head: under a low roof it would be inside the roof, every
        // ray from it would hit at once and the camera would snap in. Keep it a radius under the ceiling
        glm::vec3 head = wantedHead;
        {
            const glm::vec3 up = head - body;
            const float len = glm::length(up);

            if (len > 1e-4f)
            {
                const float free = clearance(grid, table, body, up / len, len + t.collisionRadius);

                if (free < len + t.collisionRadius)
                    head = body + up / len * std::max(0.0f, free - t.collisionRadius);
            }
        }

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

        const glm::vec3 camRight = localToWorld(viewYaw, glm::vec3(1, 0, 0));

        // A thick ray from the pivot to a camera at that pitch: the centre and four rays offset by the radius
        // ceiling: set when the nearest hit is the underside of a block
        auto freeAt = [&](float pitchDegrees, bool* ceiling = nullptr) {
            const glm::vec3 back = -viewDirection(viewYaw, pitchDegrees);
            const glm::vec3 camUp = glm::normalize(glm::cross(camRight, back));
            const float maxDist = distance + t.collisionRadius;
            float f = maxDist;
            bool roof = false;

            for (const glm::vec2 o : {glm::vec2(0, 0), glm::vec2(1, 1), glm::vec2(-1, 1), glm::vec2(1, -1), glm::vec2(-1, -1)})
            {
                const glm::vec3 offset = (camRight * o.x + camUp * o.y) * (t.collisionRadius * 0.7f);
                const RayHit hit = raycast(grid, table, pivot + offset, back, maxDist);

                if (hit.hit and hit.distance < f)
                {
                    f = hit.distance;
                    roof = hit.normal.y < 0;
                }
            }

            if (ceiling)
                *ceiling = roof;

            return f;
        };

        // Blocked from above (a roof, a jump under a ceiling): lower the camera rather than pull it in.
        // Find the smallest drop that sees the pivot from the full distance (5 degree steps, then
        // halved three times). It is taken at once (it grows smoothly as the player rises, and a lag
        // would let the roof pull the camera in) and eased back once the ceiling is gone.
        // A wall behind the player is not helped by lowering and pulls the camera in as before.
        {
            const float full = distance + t.collisionRadius - 1e-3f;
            const float lowest = std::max(t.minPitch, -10.0f);   // at most a little under the pivot
            float bestDrop = 0.0f;
            bool ceiling = false;
            float bestFree = freeAt(viewPitch, &ceiling);

            if (bestFree < full and ceiling)
            {
                float blockedDrop = 0.0f;

                for (float drop = 5.0f; viewPitch - drop >= lowest - 4.99f; drop += 5.0f)
                {
                    const float f = freeAt(viewPitch - drop);

                    if (f > bestFree + 0.05f)
                    {
                        bestFree = f;
                        bestDrop = drop;
                    }

                    if (f >= full)
                    {
                        // Refine between the last blocked drop and this one
                        float lo = blockedDrop, hi = drop;

                        for (int i = 0; i < 3; ++i)
                        {
                            const float mid = (lo + hi) * 0.5f;
                            (freeAt(viewPitch - mid) >= full ? hi : lo) = mid;
                        }

                        bestDrop = hi;
                        break;
                    }

                    blockedDrop = drop;
                }
            }

            pitchDrop = bestDrop > pitchDrop ? bestDrop : smoothTowards(pitchDrop, bestDrop, t.pullOutSharpness, dt);
            viewPitch -= pitchDrop;
        }

        const glm::vec3 back = -viewDirection(viewYaw, viewPitch);
        const glm::vec3 camUp = glm::normalize(glm::cross(camRight, back));
        const float free = freeAt(viewPitch);

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
