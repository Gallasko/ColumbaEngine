#include "stdafx.h"

#include "camera3dcontroller.h"

#include "Renderer/renderer.h"
#include "Renderer/camera.h"
#include "Input/input.h"
#include "Maths/geometry.h"

#include "logger.h"

#ifdef __EMSCRIPTEN__
    #include <SDL2/SDL.h>
#else
    #ifdef __linux__
        #include <SDL2/SDL.h>
    #elif _WIN32
        #include <SDL.h>
    #endif
#endif

#include <algorithm>
#include <cmath>

namespace pg
{
    Camera3DController::Camera3DController(MasterRenderer* masterRenderer, const Input* input)
        : masterRenderer(masterRenderer), input(input)
    {
    }

    void Camera3DController::init()
    {
        // Seed the camera somewhere pulled back and slightly above the
        // ground plane so the whole test scene is visible on first frame.
        masterRenderer->getCamera().init(
            glm::vec3(8.0f, 6.0f, 18.0f),   // position
            glm::vec3(0.0f, 1.0f, 0.0f),    // world up = +Y
            -110.0f,                         // yaw (looking toward -Z and slightly -X)
            -20.0f);                         // pitch (slightly downward)
    }

    void Camera3DController::onEvent(const TickEvent& event)
    {
        if (not input or not masterRenderer)
            return;

        auto& cam = masterRenderer->getCamera();

        // --- Mouse look (right mouse button held) ---
        if (input->isButtonPressed(SDL_BUTTON_RIGHT))
        {
            const Point2D& d = input->getMouseDelta();

            cam.yaw   += d.x * mouseSens;
            cam.pitch -= d.y * mouseSens;

            if (cam.pitch > 89.0f)  cam.pitch = 89.0f;
            if (cam.pitch < -89.0f) cam.pitch = -89.0f;
        }

        // --- WASD + Space/LShift movement ---
        glm::vec3 dir(0.0f);

        if (input->isKeyPressed(SDL_SCANCODE_W))      dir += cam.front;
        if (input->isKeyPressed(SDL_SCANCODE_S))      dir -= cam.front;
        if (input->isKeyPressed(SDL_SCANCODE_A))      dir -= cam.right;
        if (input->isKeyPressed(SDL_SCANCODE_D))      dir += cam.right;
        if (input->isKeyPressed(SDL_SCANCODE_SPACE))  dir += cam.worldUp;
        if (input->isKeyPressed(SDL_SCANCODE_LSHIFT)) dir -= cam.worldUp;

        if (glm::length(dir) > 1e-6f)
        {
            dir = glm::normalize(dir);
            cam.position += dir * (moveSpeed * event.tick);
        }

        // Camera::updateCameraVectors() is private, so to refresh
        // front/right/up after our yaw/pitch/position edits we re-run
        // init() which internally calls updateCameraVectors().
        cam.init(cam.position, cam.worldUp, cam.yaw, cam.pitch);
    }
}
