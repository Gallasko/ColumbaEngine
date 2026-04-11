#include "stdafx.h"

#include "camera3dcontroller.h"

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
    Camera3DController::Camera3DController(MasterRenderer* masterRenderer, const Input* input, Window* window)
        : masterRenderer(masterRenderer), input(input), window(window)
    {
        // Detect WSL/WSLg once at startup. Under WSLg the Windows host owns
        // the real cursor, so SDL_WarpMouseInWindow is a silent no-op and
        // produces no synthetic motion event. The ignoreNextMotion filter
        // must be disabled there to avoid eating real user input.
        onWSL = (std::getenv("WSL_INTEROP") != nullptr)
             or (std::getenv("WSL_DISTRO_NAME") != nullptr);
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

        // Lock the cursor to the window so the user can rotate the camera
        // freely without having to hold a mouse button. LAlt (handled in the
        // TickEvent below) temporarily releases it. We avoid
        // SDL_SetRelativeMouseMode because it is unreliable on WSL; instead
        // Window::setCursorLocked hides the cursor and warps it to the
        // window center, and we re-warp on every mouse motion below.
        if (window)
            window->setCursorLocked(true);

        // Drop the motion event SDL fires in response to the warp above —
        // otherwise its xrel/yrel (from wherever the OS had the cursor to
        // the window center) would rotate the camera to a random start
        // angle on the very first frame. Skipped on WSL, where the warp
        // never actually happens and the flag would eat a real event.
        if (not onWSL)
            ignoreNextMotion = true;
        cursorLocked = true;
    }

    void Camera3DController::onEvent(const OnSDLMouseMotion& event)
    {
        if (not masterRenderer)
            return;

        // Eat exactly one motion event after a warp so the warp delta doesn't
        // cancel the rotation we just applied (or jump the camera on init).
        if (ignoreNextMotion)
        {
            ignoreNextMotion = false;
            return;
        }

        // While the cursor is unlocked (LAlt held) we ignore mouse motion so
        // the user can click around the window without yanking the view.
        if (not cursorLocked)
            return;

        auto& cam = masterRenderer->getCamera();

        cam.yaw   += static_cast<float>(event.xrel) * mouseSens;
        cam.pitch -= static_cast<float>(event.yrel) * mouseSens;

        if (cam.pitch > 89.0f)  cam.pitch = 89.0f;
        if (cam.pitch < -89.0f) cam.pitch = -89.0f;

        // Refresh front/right/up from the new yaw/pitch. We deliberately
        // avoid cam.init() here because init() also rewrites position from
        // a captured parameter, which races with TickEvent's position
        // update and causes visible stutter when moving with WASD while
        // rotating the camera.
        cam.updateCameraVectors();

        // Re-center the cursor so it can never drift to the window edge, and
        // flag the next motion event (the one the warp itself will generate)
        // to be dropped so we don't feed the warp delta back into the camera.
        // On WSL the warp is a no-op, so we skip both the re-warp call and
        // the filter arming: the cursor will drift (contained by F10
        // fullscreen) but xrel/yrel remains clean and every user motion
        // event is consumed exactly once.
        if (window and not onWSL)
        {
            window->setCursorLocked(true);
            ignoreNextMotion = true;
        }
    }

    void Camera3DController::onEvent(const TickEvent& event)
    {
        if (not input or not masterRenderer)
            return;

        auto& cam = masterRenderer->getCamera();

        // --- Cursor lock toggle (hold LAlt to free the cursor) ---
        const bool altHeld    = input->isKeyPressed(SDL_SCANCODE_LALT);
        const bool wantLocked = not altHeld;

        if (wantLocked != cursorLocked)
        {
            if (window)
            {
                if (wantLocked)
                {
                    // Re-center the cursor before relocking so the camera
                    // doesn't jump based on wherever the cursor drifted
                    // while LAlt was held. On WSL the warp is a no-op so
                    // the filter must not be armed (it would eat a real
                    // user motion event after Alt-release).
                    window->setCursorLocked(true);
                    if (not onWSL)
                        ignoreNextMotion = true;
                }
                else
                {
                    // Restore the system cursor in place.
                    window->setCursorLocked(false);
                }
            }
            cursorLocked = wantLocked;
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

        // No cam.updateCameraVectors() / cam.init() call here: the tick only
        // modifies cam.position, which does not feed into front/right/up.
        // Calling init() here used to race with OnSDLMouseMotion's init()
        // and drop either position or yaw/pitch updates, causing visible
        // stutter when moving with WASD while rotating the camera.
    }
}
