#pragma once

#include "ECS/entitysystem_fwd.h" // Needed for ResizeEvent
#include "Systems/basicsystems.h"
#include "Renderer/renderer.h"
#include "Renderer/camera.h"
#include "Input/inputcomponent.h"

#include "grid.h"

using namespace pg;

// Free-roam 2D camera using BaseCamera2D directly.
// BaseCamera2D::x/y = top-left of viewport in world space.
// We control x/y/width/height directly — no FollowCamera2D.

class CameraSystem : public System<
    InitSys,
    Listener<TickEvent>,
    Listener<OnSDLMouseWheel>,
    Listener<OnSDLMouseMotion>,
    Listener<OnSDLScanCode>,
    Listener<OnSDLScanCodeReleased>,
    Listener<OnMouseClick>,
    Listener<OnMouseRelease>>
{
public:
    CameraSystem(MasterRenderer* masterRenderer, float screenWidth, float screenHeight)
        : masterRenderer(masterRenderer), baseWidth(screenWidth), baseHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Camera System"; }

    void init() override
    {
        cameraEntity = ecsRef->createEntity();

        // _attach because BaseCamera2D doesn't derive from Ctor
        cam = ecsRef->_attach<BaseCamera2D>(cameraEntity).operator->();

        cam->setWidth(baseWidth);
        cam->setHeight(baseHeight);

        // Center camera on the grid
        float gridCenterX = Grid::WIDTH * Grid::TILE_SIZE * 0.5f;
        float gridCenterY = Grid::HEIGHT * Grid::TILE_SIZE * 0.5f;
        cam->x = gridCenterX - baseWidth * 0.5f;
        cam->y = gridCenterY - baseHeight * 0.5f;
        cam->dirty = true;

        // Register with MasterRenderer → becomes viewport 1
        masterRenderer->queueRegisterCamera(cameraEntity->id);
    }

    virtual void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick / 1000.0f;
    }

    virtual void onEvent(const OnSDLMouseWheel& event) override
    {
        if (event.y == 0)
            return;

        float oldZoom = zoomLevel;
        zoomLevel *= (event.y > 0 ? 1.0f / 0.9f : 1.0f / 1.1f);
        zoomLevel = std::clamp(zoomLevel, minZoom, maxZoom);

        if (std::abs(zoomLevel - oldZoom) < 1e-6f)
            return;

        float oldViewW = baseWidth / oldZoom;
        float oldViewH = baseHeight / oldZoom;
        float newViewW = baseWidth / zoomLevel;
        float newViewH = baseHeight / zoomLevel;

        // Zoom toward mouse: shift so world point under cursor stays fixed
        float mouseNormX = lastMouseX / baseWidth;
        float mouseNormY = lastMouseY / baseHeight;
        float dw = oldViewW - newViewW;
        float dh = oldViewH - newViewH;

        cam->x += dw * mouseNormX;
        cam->y += dh * mouseNormY;
        cam->dirty = true;

        cam->setWidth(newViewW);
        cam->setHeight(newViewH);
    }

    virtual void onEvent(const OnSDLMouseMotion& event) override
    {
        lastMouseX = static_cast<float>(event.x);
        lastMouseY = static_cast<float>(event.y);

        if (rightMouseDown)
        {
            // Scale drag by current zoom (viewport width / screen width)
            float scale = cam->getWidth() / baseWidth;
            cam->x -= event.xrel * scale;
            cam->y -= event.yrel * scale;
            cam->dirty = true;
        }
    }

    virtual void onEvent(const OnMouseClick& event) override
    {
        if (event.button == SDL_BUTTON_RIGHT)
            rightMouseDown = true;
    }

    virtual void onEvent(const OnMouseRelease& event) override
    {
        if (event.button == SDL_BUTTON_RIGHT)
            rightMouseDown = false;
    }

    virtual void onEvent(const OnSDLScanCode& event) override
    {
        switch (event.key)
        {
            case SDL_SCANCODE_W: moveUp = true; break;
            case SDL_SCANCODE_S: moveDown = true; break;
            case SDL_SCANCODE_A: moveLeft = true; break;
            case SDL_SCANCODE_D: moveRight = true; break;
            default: break;
        }
    }

    virtual void onEvent(const OnSDLScanCodeReleased& event) override
    {
        switch (event.key)
        {
            case SDL_SCANCODE_W: moveUp = false; break;
            case SDL_SCANCODE_S: moveDown = false; break;
            case SDL_SCANCODE_A: moveLeft = false; break;
            case SDL_SCANCODE_D: moveRight = false; break;
            default: break;
        }
    }

    void execute() override
    {
        if (deltaTime <= 0.0f)
            return;

        if (moveUp or moveDown or moveLeft or moveRight)
        {
            float speed = panSpeed / zoomLevel * deltaTime;

            if (moveUp)    cam->y -= speed;
            if (moveDown)  cam->y += speed;
            if (moveLeft)  cam->x -= speed;
            if (moveRight) cam->x += speed;

            cam->dirty = true;
        }

        deltaTime = 0.0f;
    }

    // Custom screenToWorld that accounts for zoom
    // (engine's version has width cancel out, ignoring zoom)
    constant::Vector2D screenToWorld(float screenX, float screenY)
    {
        if (not cam)
            return {0.0f, 0.0f};

        float scaleX = cam->getWidth() / baseWidth;
        float scaleY = cam->getHeight() / baseHeight;

        return {cam->x + screenX * scaleX,
                cam->y + screenY * scaleY};
    }

    EntityRef getCameraEntity() const { return cameraEntity; }
    float getLastMouseX() const { return lastMouseX; }
    float getLastMouseY() const { return lastMouseY; }

private:
    MasterRenderer* masterRenderer = nullptr;
    EntityRef cameraEntity;
    BaseCamera2D* cam = nullptr;

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
};
