#pragma once

#include "tuning.h"
#include "voxelgrid.h"

namespace arena
{
    /**
     * Third-person camera orbiting a point above the player's shoulder.
     *
     * The pivot follows the player with smoothing. The camera sits behind the pivot at the
     * zoom distance and is pulled in at once when a wall comes between it and the pivot
     * (five rays, a thick ray), then eases back out once the wall is gone.
     * Shake is "trauma" (0..1) that decays; its square drives a smooth noise offset.
     */
    struct OrbitCamera
    {
        float yaw = 0.0f;        // degrees, game convention: 0 looks towards +Z
        float pitch = 18.0f;     // degrees, + looks down
        float distance = 5.0f;   // wanted distance (zoom)

        float currentDistance = 5.0f;
        glm::vec3 pivot = glm::vec3(0.0f);
        bool placed = false;

        float trauma = 0.0f;

        /** Degrees the camera is lowered under a ceiling, eased in and out. */
        float pitchDrop = 0.0f;

        // Result of update()
        glm::vec3 position = glm::vec3(0.0f);
        float viewYaw = 0.0f;
        float viewPitch = 0.0f;
        bool blocked = false;    // a wall pulls the camera in

        void look(float dxPixels, float dyPixels, const Tuning::Camera& t);
        void look(float dxPixels, float dyPixels, float sensitivity, float minPitch, float maxPitch);
        void zoom(float steps, const Tuning::Camera& t);
        void addTrauma(float amount) { trauma = glm::clamp(trauma + amount, 0.0f, 1.0f); }

        /** head: the player's eyes, the point the camera must always see. */
        void update(float dt, const glm::vec3& head, const VoxelGrid& grid, const BlockTable& table, const Tuning& t, float time);

        /**
         * body: a point inside the player (always free space). The head is kept under any ceiling
         * between the two, so a head bumped into a roof does not put the pivot inside the roof.
         */
        void update(float dt, const glm::vec3& head, const glm::vec3& body, const VoxelGrid& grid, const BlockTable& table, const Tuning& t, float time);

        /** First person: the camera is the eye. No smoothing, no wall pull-in (the eye is inside the body), shake kept. */
        void updateFirstPerson(float dt, const glm::vec3& eye, const Tuning& t, float time);

        /** Forward direction of the view (unit vector). */
        glm::vec3 forward() const;
    };
}
