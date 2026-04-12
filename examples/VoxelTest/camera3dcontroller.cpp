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
        // window center, and the motion handler re-warps it whenever it
        // drifts close to the window edge.
        if (window)
        {
            window->setCursorLocked(true);
            // Remember the warp target so the synth motion event SDL fires
            // in response to the warp above can be filtered out by absolute
            // position rather than by "next event arrives". On WSL the warp
            // is a no-op so no synth event will arrive — leave warpPending
            // at -1 there or we would eat a real user motion.
            if (not onWSL)
            {
                warpPendingX = window->getWidth()  / 2;
                warpPendingY = window->getHeight() / 2;
            }
        }
        cursorLocked = true;
    }

    void Camera3DController::onEvent(const OnSDLMouseMotion& event)
    {
        if (not masterRenderer)
            return;

        // Filter the synth motion event SDL queues in response to a warp.
        // Match it by absolute position rather than "next event arrives":
        // SDL queues the synth event behind any user motion events that
        // were already pending, so a positional eat-the-next filter would
        // drop a real user motion and let the warp synth through with its
        // wrong-direction delta. That misalignment was the cause of the
        // look stutter when moving with WASD + mouse simultaneously.
        if (warpPendingX >= 0 and event.x == warpPendingX and event.y == warpPendingY)
        {
            warpPendingX = -1;
            warpPendingY = -1;
            return;
        }

        // While the cursor is unlocked (LAlt held or edit mode active) we
        // ignore mouse motion so the user can interact with the UI / scene
        // without yanking the view.
        if (not cursorLocked or editMode)
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

        // Re-center the cursor only when it drifts close to the window edge.
        // The cursor is hidden, so its exact location does not matter as
        // long as it stays inside the window — and warping on every motion
        // event multiplied the rate at which warp synth events were
        // generated, which (combined with the SDL queue ordering above)
        // showed up as visible look stutter under sustained mouse motion.
        // On WSL the warp is a no-op, so skip it entirely: the cursor will
        // drift (contained by F10 fullscreen) but xrel/yrel stay clean.
        if (window and not onWSL)
        {
            const int w = window->getWidth();
            const int h = window->getHeight();

            // Margin scales with window size with a sane minimum so this
            // works on tiny test windows and large fullscreen alike.
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

    void Camera3DController::onEvent(const TickEvent& event)
    {
        if (not input or not masterRenderer)
            return;

        auto& cam = masterRenderer->getCamera();

        // --- Cursor lock toggle (hold LAlt to free the cursor) ---
        const bool altHeld    = input->isKeyPressed(SDL_SCANCODE_LALT);
        // In edit mode the cursor is always free; LAlt is also honoured.
        const bool wantLocked = not altHeld and not editMode;

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
                    {
                        warpPendingX = window->getWidth()  / 2;
                        warpPendingY = window->getHeight() / 2;
                    }
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
