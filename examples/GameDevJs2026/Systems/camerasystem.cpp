#include "camerasystem.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>

void CameraSystem::init()
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

void CameraSystem::onEvent(const OnSDLMouseWheel& event)
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

void CameraSystem::onEvent(const OnSDLMouseMotion& event)
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

void CameraSystem::onEvent(const OnMouseClick& event)
{
    if (event.button == SDL_BUTTON_RIGHT)
        rightMouseDown = true;
}

void CameraSystem::onEvent(const OnMouseRelease& event)
{
    if (event.button == SDL_BUTTON_RIGHT)
        rightMouseDown = false;
}

void CameraSystem::onEvent(const OnSDLScanCode& event)
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

void CameraSystem::onEvent(const OnSDLScanCodeReleased& event)
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

void CameraSystem::execute()
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

constant::Vector2D CameraSystem::screenToWorld(float screenX, float screenY)
{
    if (not cam)
        return {0.0f, 0.0f};

    float scaleX = cam->getWidth() / baseWidth;
    float scaleY = cam->getHeight() / baseHeight;

    return {cam->x + screenX * scaleX,
            cam->y + screenY * scaleY};
}
