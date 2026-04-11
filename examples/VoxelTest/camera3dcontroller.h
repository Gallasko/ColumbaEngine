#pragma once

#include "ECS/system.h"
#include "Systems/coresystems.h"
#include "Input/inputcomponent.h"   // for OnSDLMouseMotion

namespace pg
{
    class MasterRenderer;
    class Input;
    class Window;

    /**
     * Simple fly-through FPS camera controller.
     *
     *   WASD           — move forward/back/strafe in the view plane
     *   Space / LShift — move up / down along world Y
     *   Mouse          — rotate the view (yaw/pitch); cursor is locked to the
     *                    window by default so no button needs to be held
     *   LAlt (hold)    — release the cursor temporarily
     *
     * The controller writes directly to `MasterRenderer::camera`, which is
     * then read by the engine when building the `view` uniform for viewport 0.
     */
    struct Camera3DController : public System<InitSys, Listener<TickEvent>, Listener<OnSDLMouseMotion>>
    {
        Camera3DController(MasterRenderer* masterRenderer, const Input* input, Window* window);

        std::string getSystemName() const override { return "Camera 3D Controller"; }

        void init() override;

        void onEvent(const TickEvent& event) override;
        void onEvent(const OnSDLMouseMotion& event) override;

        // Tunables
        float moveSpeed  = 0.01f;    // world units per millisecond (TickEvent::tick is in ms)
        float mouseSens  = 0.15f;    // degrees / pixel

    private:
        MasterRenderer* masterRenderer = nullptr;
        const Input*    input          = nullptr;
        Window*         window         = nullptr;

        // Current cursor-lock state. We only rotate the camera while locked,
        // so releasing the cursor with LAlt pauses look input without losing
        // keyboard movement.
        bool cursorLocked = false;

        // When true, the next OnSDLMouseMotion event is dropped. Used to eat
        // the motion event SDL synthesises after SDL_WarpMouseInWindow, which
        // would otherwise cancel the rotation we just applied (or jump the
        // camera on init based on where the OS cursor happened to sit).
        bool ignoreNextMotion = false;
    };
}
