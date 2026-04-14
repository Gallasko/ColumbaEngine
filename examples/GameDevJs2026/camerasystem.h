#pragma once

#include "ECS/entitysystem_fwd.h" // Needed for ResizeEvent before camera2d.h
#include "Systems/basicsystems.h"
#include "2D/camera2d.h"
#include "Input/inputcomponent.h"

#include "grid.h"

using namespace pg;

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
    CameraSystem(float screenWidth, float screenHeight)
        : baseWidth(screenWidth), baseHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Camera System"; }

    void init() override
    {
        cameraEntity = ecsRef->createEntity();

        auto pos = ecsRef->attach<PositionComponent>(cameraEntity);
        // Center camera on the grid (FollowCamera2D targets this position)
        float gridCenterX = Grid::WIDTH * Grid::TILE_SIZE * 0.5f;
        float gridCenterY = Grid::HEIGHT * Grid::TILE_SIZE * 0.5f;
        pos->setX(gridCenterX);
        pos->setY(gridCenterY);

        auto followCam = ecsRef->attach<FollowCamera2D>(cameraEntity);
        followCam->setSmoothFactor(1.0f);
        followCam->setViewportWidth(baseWidth);
        followCam->setViewportHeight(baseHeight);
        followCam->useWindowViewport = false;
    }

    virtual void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick / 1000.0f;
    }

    virtual void onEvent(const OnSDLMouseWheel& event) override
    {
        if (event.y == 0)
            return;

        auto cam = ecsRef->getComponent<BaseCamera2D>(cameraEntity.id);
        if (not cam)
            return;

        float oldWidth = cam->getWidth();
        float oldHeight = cam->getHeight();

        // Zoom in or out
        zoomLevel *= (event.y > 0 ? 1.0f / 0.9f : 1.0f / 1.1f);
        zoomLevel = std::clamp(zoomLevel, minZoom, maxZoom);

        float newWidth = baseWidth / zoomLevel;
        float newHeight = baseHeight / zoomLevel;

        // Zoom toward mouse position
        float mouseNormX = lastMouseX / baseWidth;
        float mouseNormY = lastMouseY / baseHeight;

        cam->x += (oldWidth - newWidth) * mouseNormX;
        cam->y += (oldHeight - newHeight) * mouseNormY;
        cam->dirty = true;

        // Sync FollowCamera2D viewport
        auto followCam = ecsRef->getComponent<FollowCamera2D>(cameraEntity.id);
        if (followCam)
        {
            followCam->setViewportWidth(newWidth);
            followCam->setViewportHeight(newHeight);
        }

        syncPositionToCamera(cam);
    }

    virtual void onEvent(const OnSDLMouseMotion& event) override
    {
        lastMouseX = static_cast<float>(event.x);
        lastMouseY = static_cast<float>(event.y);

        if (rightMouseDown)
        {
            auto cam = ecsRef->getComponent<BaseCamera2D>(cameraEntity.id);
            if (not cam)
                return;

            float scaleX = cam->getWidth() / baseWidth;
            float scaleY = cam->getHeight() / baseHeight;

            cam->x -= event.xrel * scaleX;
            cam->y -= event.yrel * scaleY;
            cam->dirty = true;

            syncPositionToCamera(cam);
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
            auto cam = ecsRef->getComponent<BaseCamera2D>(cameraEntity.id);
            if (not cam)
            {
                deltaTime = 0.0f;
                return;
            }

            float speed = panSpeed / zoomLevel * deltaTime;

            if (moveUp)    cam->y -= speed;
            if (moveDown)  cam->y += speed;
            if (moveLeft)  cam->x -= speed;
            if (moveRight) cam->x += speed;
            cam->dirty = true;

            syncPositionToCamera(cam);
        }

        deltaTime = 0.0f;
    }

    // Convert screen mouse position to world coordinates
    constant::Vector2D screenToWorld(float screenX, float screenY)
    {
        auto cam = ecsRef->getComponent<BaseCamera2D>(cameraEntity.id);
        if (not cam)
            return {0.0f, 0.0f};

        return cam->screenToWorld(screenX, screenY);
    }

    EntityRef getCameraEntity() const { return cameraEntity; }
    float getLastMouseX() const { return lastMouseX; }
    float getLastMouseY() const { return lastMouseY; }

private:
    void syncPositionToCamera(BaseCamera2D* cam)
    {
        auto pos = ecsRef->getComponent<PositionComponent>(cameraEntity.id);
        if (pos)
        {
            pos->setX(cam->x + cam->getWidth() * 0.5f);
            pos->setY(cam->y + cam->getHeight() * 0.5f);
        }
    }

    EntityRef cameraEntity;

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
