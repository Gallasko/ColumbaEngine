#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "gridsystem.h"
#include "camerasystem.h"

using namespace pg;

class GameSystem : public System<InitSys, QueuedListener<OnMouseClick>, Listener<OnMouseRelease>, Listener<OnSDLScanCode>, QueuedListener<OnSDLMouseMotion>>
{
public:
    GameSystem(GridSystem* gridSystem, CameraSystem* cameraSystem)
        : gridSystem(gridSystem), cameraSystem(cameraSystem) {}

    virtual std::string getSystemName() const override { return "Game System"; }

    void init() override
    {
        printf("GameSystem: Grid ready (%dx%d, tile %dpx)\n",
            Grid::WIDTH, Grid::HEIGHT, Grid::TILE_SIZE);

        createCursorEntities();
    }

    virtual void onEvent(const OnSDLScanCode& event) override
    {
        if (event.key == SDL_SCANCODE_R)
        {
            currentDirection = (currentDirection + 1) % 4;
            printf("Direction: %s\n", directionNames[currentDirection]);
            updateGhostTexture();
        }
    }

    virtual void onProcessEvent(const OnMouseClick& event) override
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

    virtual void onProcessEvent(const OnSDLMouseMotion& event) override
    {
        updateCursorPosition();

        if (leftMouseDown)
        {
            placeConveyorAtMouse();
        }
    }

private:
    static constexpr size_t DIRECTION_TILE_INDEX[4] = {LINE_RIGHT_1, LINE_DOWN_1, LINE_LEFT_1, LINE_UP_1};
    static constexpr const char* directionNames[4] = {"Right", "Down", "Left", "Up"};

    void createCursorEntities()
    {
        // Tile cursor — semi-transparent white highlight
        auto cursor = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{255.0f, 255.0f, 255.0f, 40.0f});

        auto cursorPos = cursor.get<PositionComponent>();
        cursorPos->setWidth(static_cast<float>(Grid::TILE_SIZE));
        cursorPos->setHeight(static_cast<float>(Grid::TILE_SIZE));
        cursorPos->setZ(10.0f); // Above everything
        cursorPos->setX(-1000.0f); // Off-screen initially

        cursor.get<Simple2DObject>()->setViewport(GAME_VIEWPORT);
        cursorEntityId = cursor.entity->id;

        // Ghost block — semi-transparent conveyor preview
        size_t tileIndex = DIRECTION_TILE_INDEX[currentDirection];
        size_t frameIndex = tileIndex * 8;
        std::string texName = "Conveyor_Belt." + std::to_string(frameIndex);

        auto ghost = make2DTexture(ecsRef,
            static_cast<float>(Grid::TILE_SIZE),
            static_cast<float>(Grid::TILE_SIZE),
            texName);

        auto ghostPos = ghost.get<PositionComponent>();
        ghostPos->setZ(9.0f); // Below cursor, above game tiles
        ghostPos->setX(-1000.0f); // Off-screen initially

        ghost.get<Texture2DComponent>()->setOpacity(0.4f);
        ghost.get<Texture2DComponent>()->setViewport(GAME_VIEWPORT);
        ghostEntityId = ghost.entity->id;
    }

    void updateCursorPosition()
    {
        float mouseX = cameraSystem->getLastMouseX();
        float mouseY = cameraSystem->getLastMouseY();

        auto worldPos = cameraSystem->screenToWorld(mouseX, mouseY);
        auto& grid = gridSystem->getGrid();
        auto [gridX, gridY] = grid.worldToGrid(worldPos.x, worldPos.y);

        if (grid.isInBounds(gridX, gridY))
        {
            auto [wx, wy] = grid.gridToWorld(gridX, gridY);

            // Update cursor position
            auto cursorEnt = ecsRef->getEntity(cursorEntityId);
            if (cursorEnt)
            {
                auto pos = cursorEnt->get<PositionComponent>();
                pos->setX(wx);
                pos->setY(wy);
            }

            // Update ghost position
            auto ghostEnt = ecsRef->getEntity(ghostEntityId);
            if (ghostEnt)
            {
                auto pos = ghostEnt->get<PositionComponent>();
                pos->setX(wx);
                pos->setY(wy);
            }
        }
        else
        {
            // Move off-screen when out of bounds
            auto cursorEnt = ecsRef->getEntity(cursorEntityId);
            if (cursorEnt)
                cursorEnt->get<PositionComponent>()->setX(-1000.0f);

            auto ghostEnt = ecsRef->getEntity(ghostEntityId);
            if (ghostEnt)
                ghostEnt->get<PositionComponent>()->setX(-1000.0f);
        }
    }

    void updateGhostTexture()
    {
        auto ghostEnt = ecsRef->getEntity(ghostEntityId);
        if (not ghostEnt)
            return;

        size_t tileIndex = DIRECTION_TILE_INDEX[currentDirection];
        size_t frameIndex = tileIndex * 8;
        std::string texName = "Conveyor_Belt." + std::to_string(frameIndex);

        ghostEnt->get<Texture2DComponent>()->setTexture(texName);
    }

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

    uint64_t cursorEntityId = 0;
    uint64_t ghostEntityId = 0;
};
