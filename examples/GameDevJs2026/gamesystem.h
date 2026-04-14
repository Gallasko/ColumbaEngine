#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "gridsystem.h"
#include "camerasystem.h"

using namespace pg;

class GameSystem : public System<InitSys, Listener<OnMouseClick>, Listener<OnMouseRelease>, Listener<OnSDLScanCode>, Listener<OnSDLMouseMotion>>
{
public:
    GameSystem(GridSystem* gridSystem, CameraSystem* cameraSystem)
        : gridSystem(gridSystem), cameraSystem(cameraSystem) {}

    virtual std::string getSystemName() const override { return "Game System"; }

    void init() override
    {
        printf("GameSystem: Grid ready (%dx%d, tile %dpx)\n",
            Grid::WIDTH, Grid::HEIGHT, Grid::TILE_SIZE);
    }

    virtual void onEvent(const OnSDLScanCode& event) override
    {
        if (event.key == SDL_SCANCODE_R)
        {
            currentDirection = (currentDirection + 1) % 4;
            printf("Direction: %s\n", directionNames[currentDirection]);
        }
    }

    virtual void onEvent(const OnMouseClick& event) override
    {
        if (event.button == SDL_BUTTON_LEFT)
        {
            leftMouseDown = true;
            placeConveyorAtMouse();
        }
        else if (event.button == SDL_BUTTON_RIGHT)
        {
            removeConveyorAtMouse();
        }
    }

    virtual void onEvent(const OnMouseRelease& event) override
    {
        if (event.button == SDL_BUTTON_LEFT)
        {
            leftMouseDown = false;
        }
    }

    virtual void onEvent(const OnSDLMouseMotion& event) override
    {
        if (leftMouseDown)
        {
            placeConveyorAtMouse();
        }
    }

private:
    static constexpr size_t DIRECTION_TILE_INDEX[4] = {LINE_RIGHT_1, LINE_DOWN_1, LINE_LEFT_1, LINE_UP_1};
    static constexpr const char* directionNames[4] = {"Right", "Down", "Left", "Up"};

    void placeConveyorAtMouse()
    {
        float mouseX = cameraSystem->getLastMouseX();
        float mouseY = cameraSystem->getLastMouseY();

        auto worldPos = cameraSystem->screenToWorld(mouseX, mouseY);
        auto& grid = gridSystem->getGrid();
        auto [gridX, gridY] = grid.worldToGrid(worldPos.x, worldPos.y);

        if (not grid.isInBounds(gridX, gridY))
            return;

        auto buildingLayer = gridSystem->getBuildingLayer();
        auto& cell = gridSystem->getCell(buildingLayer, gridX, gridY);

        if (cell.tileId == 0)
        {
            size_t tileIndex = DIRECTION_TILE_INDEX[currentDirection];
            gridSystem->setCell(buildingLayer, gridX, gridY, 4, tileIndex);
        }
    }

    void removeConveyorAtMouse()
    {
        float mouseX = cameraSystem->getLastMouseX();
        float mouseY = cameraSystem->getLastMouseY();

        auto worldPos = cameraSystem->screenToWorld(mouseX, mouseY);
        auto& grid = gridSystem->getGrid();
        auto [gridX, gridY] = grid.worldToGrid(worldPos.x, worldPos.y);

        if (not grid.isInBounds(gridX, gridY))
            return;

        auto buildingLayer = gridSystem->getBuildingLayer();
        auto& cell = gridSystem->getCell(buildingLayer, gridX, gridY);

        if (cell.tileId != 0)
        {
            gridSystem->clearCell(buildingLayer, gridX, gridY);
        }
    }

    GridSystem* gridSystem = nullptr;
    CameraSystem* cameraSystem = nullptr;

    size_t currentDirection = 0; // 0=Right, 1=Down, 2=Left, 3=Up
    bool leftMouseDown = false;
};
