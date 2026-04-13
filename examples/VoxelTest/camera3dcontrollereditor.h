#pragma once

#include "ECS/system.h"
#include "Systems/coresystems.h"
#include "Input/inputcomponent.h"   // for OnSDLMouseMotion, OnSDLMouseWheel

namespace pg
{
    class MasterRenderer;
    class Input;
    class Window;

    /**
     * Editor-style camera controller.
     *
     *   W / S          — move up / down along world Y
     *   A / D          — strafe left / right
     *   Scroll wheel   — zoom in / out (move along the view direction)
     *   Mouse          — rotate the view (yaw/pitch); cursor is locked to the
     *                    window by default so no button needs to be held
     *   LAlt (hold)    — release the cursor temporarily
     */
    struct Camera3DControllerEditor : public System<InitSys, Listener<TickEvent>, Listener<OnSDLMouseMotion>, Listener<OnSDLMouseWheel>>
    {
        Camera3DControllerEditor(MasterRenderer* masterRenderer, const Input* input, Window* window);

        std::string getSystemName() const override { return "Camera 3D Controller Editor"; }

        void init() override;

        void onEvent(const TickEvent& event) override;
        void onEvent(const OnSDLMouseMotion& event) override;
        void onEvent(const OnSDLMouseWheel& event) override;

        // Tunables
        float moveSpeed  = 0.01f;    // world units per millisecond (TickEvent::tick is in ms)
        float mouseSens  = 0.15f;    // degrees / pixel
        float zoomSpeed  = 2.0f;     // world units per scroll notch

        // Set by EditorSystem on Tab: when true the controller yields cursor
        // and look control to the editor, but still allows WASD movement.
        bool editMode = false;

    private:
        MasterRenderer* masterRenderer = nullptr;
        const Input*    input          = nullptr;
        Window*         window         = nullptr;

        bool cursorLocked = false;

        int warpPendingX = -1;
        int warpPendingY = -1;

        bool onWSL = false;
    };
}
