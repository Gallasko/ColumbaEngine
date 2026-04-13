#include "stdafx.h"

#include "camera3dcontrollereditor.h"

#include "Renderer/renderer.h"
#include "Renderer/camera.h"
#include "Input/input.h"
#include "Maths/geometry.h"
#include "window.h"

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
#include <cstdlib>

namespace pg
{
    Camera3DControllerEditor::Camera3DControllerEditor(MasterRenderer* masterRenderer, const Input* input, Window* window)
        : masterRenderer(masterRenderer), input(input), window(window)
    {
        onWSL = (std::getenv("WSL_INTEROP") != nullptr)
             or (std::getenv("WSL_DISTRO_NAME") != nullptr);
    }

    void Camera3DControllerEditor::init()
    {
        // Point the camera at the centre of the base platform (8, 0, 8).
        masterRenderer->getCamera().init(
            glm::vec3(8.0f, 6.0f, 18.0f),   // position
            glm::vec3(0.0f, 1.0f, 0.0f),    // world up = +Y
            -90.0f,                          // yaw (looking straight along -Z)
            -31.0f);                         // pitch (down toward platform centre)

        if (window)
        {
            window->setCursorLocked(true);
            if (not onWSL)
            {
                warpPendingX = window->getWidth()  / 2;
                warpPendingY = window->getHeight() / 2;
            }
        }
        cursorLocked = true;
    }

    void Camera3DControllerEditor::onEvent(const OnSDLMouseMotion& event)
    {
        if (not masterRenderer)
            return;

        if (warpPendingX >= 0 and event.x == warpPendingX and event.y == warpPendingY)
        {
            warpPendingX = -1;
            warpPendingY = -1;
            return;
        }

        if (not cursorLocked or editMode)
            return;

        auto& cam = masterRenderer->getCamera();

        cam.yaw   += static_cast<float>(event.xrel) * mouseSens;
        cam.pitch -= static_cast<float>(event.yrel) * mouseSens;

        if (cam.pitch > 89.0f)  cam.pitch = 89.0f;
        if (cam.pitch < -89.0f) cam.pitch = -89.0f;

        cam.updateCameraVectors();

        if (window and not onWSL)
        {
            const int w = window->getWidth();
            const int h = window->getHeight();

            const int marginX = std::max(80, w / 6);
            const int marginY = std::max(80, h / 6);

            const bool nearEdge =
                (event.x < marginX) or (event.x > w - marginX) or
                (event.y < marginY) or (event.y > h - marginY);

            if (nearEdge)
            {
                window->setCursorLocked(true);
                warpPendingX = w / 2;
                warpPendingY = h / 2;
            }
        }
    }

    void Camera3DControllerEditor::onEvent(const OnSDLMouseWheel& event)
    {
        if (not masterRenderer)
            return;

        auto& cam = masterRenderer->getCamera();
        cam.position += cam.front * (static_cast<float>(event.y) * zoomSpeed);
    }

    void Camera3DControllerEditor::onEvent(const TickEvent& event)
    {
        if (not input or not masterRenderer)
            return;

        auto& cam = masterRenderer->getCamera();

        // --- Cursor lock toggle (hold LAlt to free the cursor) ---
        const bool altHeld    = input->isKeyPressed(SDL_SCANCODE_LALT);
        const bool wantLocked = not altHeld and not editMode;

        if (wantLocked != cursorLocked)
        {
            if (window)
            {
                if (wantLocked)
                {
                    window->setCursorLocked(true);
                    if (not onWSL)
                    {
                        warpPendingX = window->getWidth()  / 2;
                        warpPendingY = window->getHeight() / 2;
                    }
                }
                else
                {
                    window->setCursorLocked(false);
                }
            }
            cursorLocked = wantLocked;
        }

        // --- W/S = up/down, A/D = strafe ---
        glm::vec3 dir(0.0f);

        if (input->isKeyPressed(SDL_SCANCODE_W))  dir += cam.worldUp;
        if (input->isKeyPressed(SDL_SCANCODE_S))  dir -= cam.worldUp;
        if (input->isKeyPressed(SDL_SCANCODE_A))  dir -= cam.right;
        if (input->isKeyPressed(SDL_SCANCODE_D))  dir += cam.right;

        if (glm::length(dir) > 1e-6f)
        {
            dir = glm::normalize(dir);
            cam.position += dir * (moveSpeed * event.tick);
        }
    }
}
