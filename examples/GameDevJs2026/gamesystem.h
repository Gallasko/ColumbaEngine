#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "gridsystem.h"
#include "camerasystem.h"

using namespace pg;

class GameSystem : public System<InitSys, Listener<OnMouseClick>>
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

    virtual void onEvent(const OnMouseClick& event) override
    {
        if (event.button != SDL_BUTTON_LEFT)
            return;

        // Use tracked mouse position from camera system (event.pos may be unreliable for global listeners)
        float mouseX = cameraSystem->getLastMouseX();
        float mouseY = cameraSystem->getLastMouseY();

        auto worldPos = cameraSystem->screenToWorld(mouseX, mouseY);
        auto& grid = gridSystem->getGrid();
        auto [gridX, gridY] = grid.worldToGrid(worldPos.x, worldPos.y);

        if (not grid.isInBounds(gridX, gridY))
        {
            printf("Click out of bounds: world(%.1f, %.1f) -> grid(%d, %d)\n",
                worldPos.x, worldPos.y, gridX, gridY);
            return;
        }

        // Toggle a building tile on click
        auto buildingLayer = gridSystem->getBuildingLayer();
        auto& cell = gridSystem->getCell(buildingLayer, gridX, gridY);

        if (cell.tileId == 0)
        {
            gridSystem->setCell(buildingLayer, gridX, gridY, 4); // Conveyor
            printf("Placed conveyor at grid(%d, %d)\n", gridX, gridY);
        }
        else
        {
            gridSystem->clearCell(buildingLayer, gridX, gridY);
            printf("Removed tile at grid(%d, %d)\n", gridX, gridY);
        }
    }

private:
    GridSystem* gridSystem = nullptr;
    CameraSystem* cameraSystem = nullptr;
};
