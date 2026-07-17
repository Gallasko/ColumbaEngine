#pragma once

#include "ECS/entitysystem_fwd.h" // Needed for ResizeEvent
#include "Systems/basicsystems.h"
#include "Renderer/renderer.h"
#include "Renderer/camera.h"
#include "Input/inputcomponent.h"

#include "grid.h"

class InventoryUISystem;

// Free-roam 2D camera using BaseCamera2D directly.
// BaseCamera2D::x/y = top-left of viewport in world space.
// We control x/y/width/height directly — no FollowCamera2D.

class CameraSystem : public pg::System<
    pg::InitSys,
    pg::Listener<pg::TickEvent>,
    pg::Listener<pg::ResizeEvent>,
    pg::Listener<pg::OnSDLMouseWheel>,
    pg::Listener<pg::OnSDLMouseMotion>,
    pg::Listener<pg::OnSDLScanCode>,
    pg::Listener<pg::OnSDLScanCodeReleased>,
    pg::Listener<pg::OnMouseClick>,
    pg::Listener<pg::OnMouseRelease>>
{
public:
    CameraSystem(pg::MasterRenderer* masterRenderer, float screenWidth, float screenHeight)
        : masterRenderer(masterRenderer), baseWidth(screenWidth), baseHeight(screenHeight) {}

    void setUiCameraEntity(pg::EntityRef entity) { uiCameraEntity = entity; }

    virtual std::string getSystemName() const override { return "Camera System"; }

    void init() override;

    virtual void onEvent(const pg::TickEvent& event) override
    {
        deltaTime += event.tick / 1000.0f;
    }

    virtual void onEvent(const pg::ResizeEvent& event) override;
    virtual void onEvent(const pg::OnSDLMouseWheel& event) override;
    virtual void onEvent(const pg::OnSDLMouseMotion& event) override;
    virtual void onEvent(const pg::OnMouseClick& event) override;
    virtual void onEvent(const pg::OnMouseRelease& event) override;
    virtual void onEvent(const pg::OnSDLScanCode& event) override;
    virtual void onEvent(const pg::OnSDLScanCodeReleased& event) override;

    void execute() override;

    // Custom screenToWorld that accounts for zoom
    // (engine's version has width cancel out, ignoring zoom)
    pg::constant::Vector2D screenToWorld(float screenX, float screenY);

    pg::EntityRef getCameraEntity() const { return cameraEntity; }
    float getLastMouseX() const { return lastMouseX; }
    float getLastMouseY() const { return lastMouseY; }
    float getScreenWidth() const { return baseWidth; }
    float getScreenHeight() const { return baseHeight; }

private:
    pg::MasterRenderer* masterRenderer = nullptr;
    pg::EntityRef cameraEntity;
    pg::BaseCamera2D* cam = nullptr;

    float baseWidth;
    float baseHeight;
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

    pg::EntityRef uiCameraEntity;
};
