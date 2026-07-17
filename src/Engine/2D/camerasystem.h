#pragma once

#include "ECS/entitysystem_fwd.h" // Needed for ResizeEvent
#include "Systems/basicsystems.h"
#include "Renderer/renderer.h"
#include "Renderer/camera.h"
#include "Input/inputcomponent.h"

#include <functional>

namespace pg
{
    // Free-roam 2D camera using BaseCamera2D directly.
    // BaseCamera2D::x/y = top-left of viewport in world space.
    // We control x/y/width/height directly — no FollowCamera2D.
    class CameraSystem : public System<
        InitSys,
        Listener<TickEvent>,
        Listener<ResizeEvent>,
        Listener<OnSDLMouseWheel>,
        Listener<OnSDLMouseMotion>,
        Listener<OnSDLScanCode>,
        Listener<OnSDLScanCodeReleased>,
        Listener<OnMouseClick>,
        Listener<OnMouseRelease>>
    {
    public:
        CameraSystem(MasterRenderer* masterRenderer, float screenWidth, float screenHeight,
                     float initialCenterX = 0.0f, float initialCenterY = 0.0f)
            : masterRenderer(masterRenderer), baseWidth(screenWidth), baseHeight(screenHeight),
              initialCenterX(initialCenterX), initialCenterY(initialCenterY) {}

        void setUiCameraEntity(EntityRef entity) { uiCameraEntity = entity; }

        // Called on every mouse-wheel event before applying zoom; return true to
        // suppress zooming (e.g. while a modal panel wants the wheel instead).
        std::function<bool()> shouldSuppressZoom;

        virtual std::string getSystemName() const override { return "Camera System"; }

        void init() override;

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick / 1000.0f;
        }

        virtual void onEvent(const ResizeEvent& event) override;
        virtual void onEvent(const OnSDLMouseWheel& event) override;
        virtual void onEvent(const OnSDLMouseMotion& event) override;
        virtual void onEvent(const OnMouseClick& event) override;
        virtual void onEvent(const OnMouseRelease& event) override;
        virtual void onEvent(const OnSDLScanCode& event) override;
        virtual void onEvent(const OnSDLScanCodeReleased& event) override;

        void execute() override;

        // Custom screenToWorld that accounts for zoom
        // (BaseCamera2D::screenToWorld ignores viewport width, i.e. zoom)
        constant::Vector2D screenToWorld(float screenX, float screenY);

        EntityRef getCameraEntity() const { return cameraEntity; }
        float getLastMouseX() const { return lastMouseX; }
        float getLastMouseY() const { return lastMouseY; }
        float getScreenWidth() const { return baseWidth; }
        float getScreenHeight() const { return baseHeight; }

    private:
        MasterRenderer* masterRenderer = nullptr;
        EntityRef cameraEntity;
        BaseCamera2D* cam = nullptr;

        float baseWidth;
        float baseHeight;
        float initialCenterX;
        float initialCenterY;
        float zoomLevel = 1.0f;
        float minZoom = 0.5f;
        float maxZoom = 8.0f;
        float panSpeed = 300.0f;

        float deltaTime = 0.0f;
        float lastMouseX = 0.0f;
        float lastMouseY = 0.0f;

        bool rightMouseDown = false;
        bool moveUp = false;
        bool moveDown = false;
        bool moveLeft = false;
        bool moveRight = false;

        EntityRef uiCameraEntity;
    };
}
