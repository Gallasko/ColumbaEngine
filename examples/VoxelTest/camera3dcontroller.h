#pragma once

#include "ECS/system.h"
#include "Systems/coresystems.h"

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
        float moveSpeed  = 6.0f;   // world units / second
        float mouseSens  = 0.12f;  // degrees / pixel

    private:
        MasterRenderer* masterRenderer = nullptr;
        const Input*    input          = nullptr;
    };
}
