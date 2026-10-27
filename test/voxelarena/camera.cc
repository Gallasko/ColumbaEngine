#include "stdafx.h"

#include <gtest/gtest.h>

#include "Core/orbitcamera.h"

using namespace arena;

namespace
{
    BlockTable table()
    {
        BlockTable t(2);
        t[0].solid = false;
        t[0].visible = false;
        return t;
    }

    Tuning tuning()
    {
        Tuning t;
        t.camera.distance = 5.0f;
        t.camera.shoulder = 0.0f;
        t.feel.shakeOffset = 0.0f;
        t.feel.shakeAngle = 0.0f;
        return t;
    }
}

TEST(arena_camera, sits_behind_the_pivot_at_its_distance)
{
    VoxelGrid g(32, 16, 32);
    const Tuning t = tuning();
    OrbitCamera cam;
    cam.yaw = 0.0f;     // looking towards +Z
    cam.pitch = 0.0f;
    cam.distance = 5.0f;

    for (int i = 0; i < 60; ++i)
        cam.update(1.0f / 60.0f, glm::vec3(16, 3, 16), g, table(), t, i / 60.0f);

    EXPECT_NEAR(cam.position.z, 11.0f, 0.01f);
    EXPECT_NEAR(cam.position.y, 3.0f, 0.01f);
    EXPECT_FALSE(cam.blocked);
}

TEST(arena_camera, a_wall_pulls_it_in_at_once_and_it_eases_back_out)
{
    VoxelGrid g(32, 16, 32);
    const Tuning t = tuning();
    OrbitCamera cam;
    cam.yaw = 0.0f;
    cam.pitch = 0.0f;
    cam.distance = 5.0f;

    const glm::vec3 head(16.5f, 3.5f, 16.5f);
    cam.update(1.0f / 60.0f, head, g, table(), t, 0.0f);

    // A wall two blocks behind the head: the camera may not end beyond it, not even for one frame
    g.fill({0, 0, 13}, {31, 15, 13}, 1);
    cam.update(1.0f / 60.0f, head, g, table(), t, 0.0f);

    EXPECT_TRUE(cam.blocked);
    EXPECT_GT(cam.position.z, 14.0f);
    EXPECT_LE(glm::distance(cam.position, head), 2.5f);

    // Wall gone: back out, slowly
    g.fill({0, 0, 13}, {31, 15, 13}, AIR);
    cam.update(1.0f / 60.0f, head, g, table(), t, 0.0f);
    const float afterOneFrame = glm::distance(cam.position, head);
    EXPECT_LT(afterOneFrame, 3.0f);

    for (int i = 0; i < 240; ++i)
        cam.update(1.0f / 60.0f, head, g, table(), t, 0.0f);

    EXPECT_NEAR(glm::distance(cam.position, head), 5.0f, 0.05f);
}

TEST(arena_camera, mouse_turns_and_pitch_stays_in_its_limits)
{
    const Tuning t = tuning();
    OrbitCamera cam;
    cam.yaw = 0.0f;
    cam.pitch = 0.0f;

    cam.look(100.0f, 0.0f, t.camera);
    EXPECT_LT(cam.yaw, 0.0f);   // mouse right turns right: yaw grows to the left

    cam.look(0.0f, 100000.0f, t.camera);
    EXPECT_FLOAT_EQ(cam.pitch, t.camera.maxPitch);
    cam.look(0.0f, -100000.0f, t.camera);
    EXPECT_FLOAT_EQ(cam.pitch, t.camera.minPitch);

    cam.zoom(100.0f, t.camera);
    EXPECT_FLOAT_EQ(cam.distance, t.camera.minDistance);
}

TEST(arena_camera, shake_moves_the_view_then_dies_out)
{
    VoxelGrid g(32, 16, 32);
    Tuning t = tuning();
    t.feel.shakeOffset = 0.3f;
    t.feel.shakeAngle = 2.0f;

    OrbitCamera cam;
    cam.pitch = 0.0f;
    const glm::vec3 head(16.5f, 3.5f, 16.5f);

    for (int i = 0; i < 30; ++i)
        cam.update(1.0f / 60.0f, head, g, table(), t, i / 60.0f);

    const glm::vec3 still = cam.position;
    cam.addTrauma(1.0f);
    cam.update(1.0f / 60.0f, head, g, table(), t, 0.55f);
    EXPECT_GT(glm::distance(cam.position, still), 0.01f);

    for (int i = 0; i < 120; ++i)
        cam.update(1.0f / 60.0f, head, g, table(), t, 1.0f + i / 60.0f);

    EXPECT_EQ(cam.trauma, 0.0f);
    EXPECT_NEAR(glm::distance(cam.position, still), 0.0f, 1e-3f);
}
