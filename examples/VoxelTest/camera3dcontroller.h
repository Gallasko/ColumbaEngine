#pragma once

#include "ECS/system.h"
#include "Systems/coresystems.h"
#include "Maths/geometry.h"

namespace pg
{
    class MasterRenderer;
    class Input;

    /**
     * Simple fly-through FPS camera controller.
     *
     *   WASD          — move forward/back/strafe in the view plane
     *   Space / LShift — move up / down along world Y
     *   Right mouse    — hold & drag to rotate the view (yaw/pitch)
     *
     * The controller writes directly to `MasterRenderer::camera`, which is
     * then read by the engine when building the `view` uniform for viewport 0.
     */
    struct Camera3DController : public System<InitSys, Listener<TickEvent>>
    {
        Camera3DController(MasterRenderer* masterRenderer, const Input* input);

        std::string getSystemName() const override { return "Camera 3D Controller"; }

        void init() override;

        void onEvent(const TickEvent& event) override;

        // Tunables
        float moveSpeed  = 0.01f;    // world units per millisecond (TickEvent::tick is in ms)
        float mouseSens  = 0.15f;    // degrees / pixel

    private:
        MasterRenderer* masterRenderer = nullptr;
        const Input*    input          = nullptr;

        // Position-based delta tracking. Using the Input's cumulative mouseDelta
        // is unreliable because updateInput() resets it every main-loop frame,
        // racing with the ECS thread that drives the TickEvent. getMousePos()
        // is not reset, so we compare against our own stored position instead.
        Point2D lastMousePos { 0.0f, 0.0f };
        bool    rightMouseWasHeld = false;
    };
}
