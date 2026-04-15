#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "gridsystem.h"
#include "camerasystem.h"
#include "toolbarsystem.h"
#include "buildingregistry.h"
#include "transportsystem.h"

using namespace pg;

class GameSystem : public System<InitSys, QueuedListener<OnMouseClick>, Listener<OnMouseRelease>, Listener<OnSDLScanCode>, QueuedListener<OnSDLMouseMotion>>
{
public:
    GameSystem(GridSystem* gridSystem, CameraSystem* cameraSystem, ToolbarSystem* toolbarSystem, BuildingRegistry* registry, TransportSystem* transportSystem = nullptr)
        : gridSystem(gridSystem), cameraSystem(cameraSystem), toolbarSystem(toolbarSystem), registry(registry), transportSystem(transportSystem) {}

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
            const auto& def = getSelectedDef();
            if (def.hasDirection)
            {
                currentDirection = (currentDirection + 1) % 4;
                printf("Direction: %s\n", directionNames[currentDirection]);
                updateGhostTexture();
            }
        }

        // Debug: T spawns an Iron Ore on the belt under the cursor
        if (event.key == SDL_SCANCODE_T and transportSystem)
        {
            auto [gx, gy] = getMouseGridPos();
            if (transportSystem->tryPlaceItem(gx, gy, 1))
                printf("Debug: Spawned Iron Ore at (%d, %d)\n", gx, gy);
        }
    }

    virtual void onProcessEvent(const OnMouseClick& event) override
    {
        if (event.button == SDL_BUTTON_LEFT)
        {
            // Skip if clicking on toolbar area
            if (isMouseOverToolbar())
                return;

            const auto& def = getSelectedDef();

            if (def.mode == PlacementMode::LineDrag)
            {
                // Start line drag
                auto [gx, gy] = getMouseGridPos();
                if (gridSystem->getGrid().isInBounds(gx, gy))
                {
                    isDragging = true;
                    dragPath.clear();
                    dragPath.push_back({gx, gy});
                    updateDragGhosts();
                }
            }
            else
            {
                leftMouseDown = true;
                placeAtMouse();
            }
        }
        else if (event.button == SDL_BUTTON_RIGHT)
        {
            removeAtMouse();
        }
    }

    virtual void onEvent(const OnMouseRelease& event) override
    {
        if (event.button == SDL_BUTTON_LEFT)
        {
            if (isDragging)
            {
                commitDragPath();
                isDragging = false;
            }
            // Safety: always clear any remaining drag ghosts on release
            clearDragGhosts();
            dragPath.clear();
            leftMouseDown = false;
        }
    }

    virtual void onProcessEvent(const OnSDLMouseMotion& event) override
    {
        updateCursorPosition();

        if (isDragging)
        {
            updateDragPathWithMouse();
        }
        else if (leftMouseDown)
        {
            placeAtMouse();
        }

        // Detect building selection changes to rebuild ghost
        size_t currentSlot = getSelectedSlot();
        if (currentSlot != lastSelectedSlot)
        {
            lastSelectedSlot = currentSlot;
            rebuildGhostForSelectedBuilding();
        }
    }

    size_t getSelectedSlot() const { return toolbarSystem->getSelectedSlot(); }

private:
    // Direction constants
    static constexpr size_t DIRECTION_TILE_INDEX[4] = {LINE_RIGHT_1, LINE_DOWN_1, LINE_LEFT_1, LINE_UP_1};
    static constexpr const char* directionNames[4] = {"Right", "Down", "Left", "Up"};

    // Corner tile mapping: cornerTileMap[enterDir][exitDir]
    // enterDir/exitDir: 0=Right, 1=Down, 2=Left, 3=Up
    //
    // (X,Y) in corner_XX_X_Y = the OPEN/EMPTY corner position:
    //   (0,0) empty top-left    → belt bottom-right → connects Bottom+Right
    //   (1,0) empty top-right   → belt bottom-left  → connects Bottom+Left
    //   (0,1) empty bottom-left → belt top-right    → connects Top+Right
    //   (1,1) empty bottom-right→ belt top-left     → connects Top+Left
    //
    // CW turns:  R→D=1_0, D→L=1_1, L→U=0_1, U→R=0_0
    // CCW turns: R→U=1_1, D→R=0_1, L→D=0_0, U→L=1_0
    static constexpr size_t INVALID_CORNER = SIZE_MAX;
    static constexpr size_t cornerTileMap[4][4] = {
        //                exit: Right          Down             Left             Up
        /* enter Right */ {INVALID_CORNER, CORNER_CW_1_0,  INVALID_CORNER, CORNER_CCW_1_1},
        /* enter Down  */ {CORNER_CCW_0_1, INVALID_CORNER, CORNER_CW_1_1,  INVALID_CORNER},
        /* enter Left  */ {INVALID_CORNER, CORNER_CCW_0_0, INVALID_CORNER, CORNER_CW_0_1},
        /* enter Up    */ {CORNER_CW_0_0,  INVALID_CORNER, CORNER_CCW_1_0, INVALID_CORNER},
    };

    const BuildingDef& getSelectedDef() const
    {
        return registry->get(getSelectedSlot());
    }

    // --- Cursor & Ghost ---

    void createCursorEntities()
    {
        // Tile cursor — semi-transparent white highlight
        auto cursor = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{255.0f, 255.0f, 255.0f, 40.0f});

        auto cursorPos = cursor.get<PositionComponent>();
        cursorPos->setWidth(static_cast<float>(Grid::TILE_SIZE));
        cursorPos->setHeight(static_cast<float>(Grid::TILE_SIZE));
        cursorPos->setZ(10.0f);
        cursorPos->setX(-1000.0f);

        cursor.get<Simple2DObject>()->setViewport(GAME_VIEWPORT);
        cursorEntityId = cursor.entity->id;

        // Ghost block — preview of selected building
        rebuildGhostForSelectedBuilding();
    }

    void rebuildGhostForSelectedBuilding()
    {
        // Destroy old ghost
        if (ghostEntityId != 0)
        {
            auto ent = ecsRef->getEntity(ghostEntityId);
            if (ent)
                ecsRef->removeEntity(ghostEntityId);
            ghostEntityId = 0;
        }

        const auto& def = getSelectedDef();

        if (not def.textureName.empty() and def.gridW == 1 and def.gridH == 1)
        {
            // Textured ghost (conveyors etc.)
            size_t tileIndex = def.hasDirection ? DIRECTION_TILE_INDEX[currentDirection] : 0;
            size_t frameIndex = tileIndex * 8;
            std::string texName = def.textureName + "." + std::to_string(frameIndex);

            auto ghost = make2DTexture(ecsRef,
                static_cast<float>(Grid::TILE_SIZE),
                static_cast<float>(Grid::TILE_SIZE),
                texName);

            ghost.get<PositionComponent>()->setZ(9.0f);
            ghost.get<PositionComponent>()->setX(-1000.0f);
            ghost.get<Texture2DComponent>()->setOpacity(0.4f);
            ghost.get<Texture2DComponent>()->setViewport(GAME_VIEWPORT);
            ghostEntityId = ghost.entity->id;
        }
        else
        {
            // Colored ghost for placeholder / multi-cell buildings
            auto color = def.color;
            color.w = 100.0f; // Semi-transparent alpha

            auto ghost = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, color);
            auto pos = ghost.get<PositionComponent>();
            pos->setZ(9.0f);
            pos->setX(-1000.0f);
            pos->setWidth(static_cast<float>(Grid::TILE_SIZE * def.gridW));
            pos->setHeight(static_cast<float>(Grid::TILE_SIZE * def.gridH));

            ghost.get<Simple2DObject>()->setViewport(GAME_VIEWPORT);
            ghostEntityId = ghost.entity->id;
        }

        // Resize cursor to match building footprint
        auto cursorEnt = ecsRef->getEntity(cursorEntityId);
        if (cursorEnt)
        {
            auto pos = cursorEnt->get<PositionComponent>();
            pos->setWidth(static_cast<float>(Grid::TILE_SIZE * def.gridW));
            pos->setHeight(static_cast<float>(Grid::TILE_SIZE * def.gridH));
        }
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

            // Update ghost position (hidden during line drag)
            if (not isDragging)
            {
                auto ghostEnt = ecsRef->getEntity(ghostEntityId);
                if (ghostEnt)
                {
                    auto pos = ghostEnt->get<PositionComponent>();
                    pos->setX(wx);
                    pos->setY(wy);

                    // Tint ghost red if placement is invalid
                    const auto& def = getSelectedDef();
                    bool canPlace = canPlaceAt(gridX, gridY, def);

                    if (ghostEnt->has<Simple2DObject>())
                    {
                        auto tint = canPlace
                            ? constant::Vector4D{def.color.x, def.color.y, def.color.z, 100.0f}
                            : constant::Vector4D{255.0f, 60.0f, 60.0f, 100.0f};
                        ghostEnt->get<Simple2DObject>()->setColors(tint);
                    }
                    if (ghostEnt->has<Texture2DComponent>())
                    {
                        ghostEnt->get<Texture2DComponent>()->setOpacity(canPlace ? 0.4f : 0.15f);
                    }
                }
            }
        }
        else
        {
            // Move off-screen when out of bounds
            auto cursorEnt = ecsRef->getEntity(cursorEntityId);
            if (cursorEnt)
                cursorEnt->get<PositionComponent>()->setX(-1000.0f);

            if (not isDragging)
            {
                auto ghostEnt = ecsRef->getEntity(ghostEntityId);
                if (ghostEnt)
                    ghostEnt->get<PositionComponent>()->setX(-1000.0f);
            }
        }
    }

    void updateGhostTexture()
    {
        const auto& def = getSelectedDef();
        if (def.textureName.empty() or not def.hasDirection)
            return;

        auto ghostEnt = ecsRef->getEntity(ghostEntityId);
        if (not ghostEnt)
            return;

        size_t tileIndex = DIRECTION_TILE_INDEX[currentDirection];
        size_t frameIndex = tileIndex * 8;
        std::string texName = def.textureName + "." + std::to_string(frameIndex);

        ghostEnt->get<Texture2DComponent>()->setTexture(texName);
    }

    // --- Placement ---

    bool canPlaceAt(int gx, int gy, const BuildingDef& def) const
    {
        auto layer = gridSystem->getBuildingLayer();
        for (int dy = 0; dy < def.gridH; ++dy)
            for (int dx = 0; dx < def.gridW; ++dx)
            {
                int cx = gx + dx, cy = gy + dy;
                if (not gridSystem->getGrid().isInBounds(cx, cy))
                    return false;
                if (gridSystem->getCell(layer, cx, cy).tileId != 0)
                    return false;
            }
        return true;
    }

    std::pair<int, int> getMouseGridPos() const
    {
        float mouseX = cameraSystem->getLastMouseX();
        float mouseY = cameraSystem->getLastMouseY();
        auto worldPos = cameraSystem->screenToWorld(mouseX, mouseY);
        return gridSystem->getGrid().worldToGrid(worldPos.x, worldPos.y);
    }

    void placeAtMouse()
    {
        auto [gridX, gridY] = getMouseGridPos();
        const auto& def = getSelectedDef();

        if (not canPlaceAt(gridX, gridY, def))
            return;

        auto layer = gridSystem->getBuildingLayer();
        size_t tileIndex = def.hasDirection ? DIRECTION_TILE_INDEX[currentDirection] : 0;
        gridSystem->placeBuilding(layer, gridX, gridY, def, currentDirection, tileIndex);

        // Update adjacent belts to reflect the new neighbor
        for (int dy = 0; dy < def.gridH; ++dy)
            for (int dx = 0; dx < def.gridW; ++dx)
                gridSystem->updateNeighborBelts(layer, gridX + dx, gridY + dy);
    }

    void removeAtMouse()
    {
        auto [gridX, gridY] = getMouseGridPos();
        if (not gridSystem->getGrid().isInBounds(gridX, gridY))
            return;

        gridSystem->removeBuilding(gridSystem->getBuildingLayer(), gridX, gridY);
    }

    bool isMouseOverToolbar() const
    {
        float mouseY = cameraSystem->getLastMouseY();
        float screenH = cameraSystem->getScreenHeight();
        return mouseY > screenH - TOOLBAR_HEIGHT;
    }

    static constexpr float TOOLBAR_HEIGHT = 48.0f;

    // --- Line Drag ---

    static uint8_t directionFromTo(int ax, int ay, int bx, int by)
    {
        int dx = bx - ax;
        int dy = by - ay;
        if (dx > 0) return 0; // Right
        if (dy > 0) return 1; // Down
        if (dx < 0) return 2; // Left
        if (dy < 0) return 3; // Up
        return 0;
    }

    size_t resolveTileIndex(uint8_t enterDir, uint8_t exitDir) const
    {
        if (enterDir == exitDir)
            return DIRECTION_TILE_INDEX[exitDir];

        size_t corner = cornerTileMap[enterDir][exitDir];
        if (corner == INVALID_CORNER)
            return DIRECTION_TILE_INDEX[exitDir];

        return corner;
    }

    void updateDragPathWithMouse()
    {
        auto [cx, cy] = getMouseGridPos();
        if (not gridSystem->getGrid().isInBounds(cx, cy))
            return;

        if (dragPath.empty())
            return;

        auto& last = dragPath.back();
        if (cx == last.first and cy == last.second)
            return; // Same cell

        // Check if target cell is already in the path → truncate the loop
        int existingIdx = findInPath(cx, cy);
        if (existingIdx >= 0)
        {
            dragPath.resize(static_cast<size_t>(existingIdx) + 1);
            updateDragGhosts();
            return;
        }

        // Fill gap if mouse skipped cells (fast movement)
        int dx = cx - last.first;
        int dy = cy - last.second;

        // Step one cell at a time, horizontal first then vertical
        int stepX = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
        int stepY = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;

        int curX = last.first;
        int curY = last.second;

        // Horizontal steps
        while (curX != cx)
        {
            curX += stepX;
            int idx = findInPath(curX, curY);
            if (idx >= 0)
            {
                dragPath.resize(static_cast<size_t>(idx) + 1);
                updateDragGhosts();
                return;
            }
            dragPath.push_back({curX, curY});
        }

        // Vertical steps
        while (curY != cy)
        {
            curY += stepY;
            int idx = findInPath(curX, curY);
            if (idx >= 0)
            {
                dragPath.resize(static_cast<size_t>(idx) + 1);
                updateDragGhosts();
                return;
            }
            dragPath.push_back({curX, curY});
        }

        updateDragGhosts();
    }

    int findInPath(int x, int y) const
    {
        for (size_t i = 0; i < dragPath.size(); ++i)
        {
            if (dragPath[i].first == x and dragPath[i].second == y)
                return static_cast<int>(i);
        }
        return -1;
    }

    void updateDragGhosts()
    {
        // Clear old ghosts
        clearDragGhosts();

        // Hide the regular ghost during drag
        auto ghostEnt = ecsRef->getEntity(ghostEntityId);
        if (ghostEnt)
            ghostEnt->get<PositionComponent>()->setX(-1000.0f);

        const auto& def = getSelectedDef();
        auto& grid = gridSystem->getGrid();

        for (size_t i = 0; i < dragPath.size(); ++i)
        {
            auto [gx, gy] = dragPath[i];
            auto [wx, wy] = grid.gridToWorld(gx, gy);

            // Compute enter/exit direction for this cell
            uint8_t enterDir, exitDir;

            if (dragPath.size() == 1)
            {
                enterDir = exitDir = static_cast<uint8_t>(currentDirection);
            }
            else if (i == 0)
            {
                exitDir = directionFromTo(gx, gy, dragPath[1].first, dragPath[1].second);
                enterDir = exitDir;
            }
            else if (i == dragPath.size() - 1)
            {
                enterDir = directionFromTo(dragPath[i - 1].first, dragPath[i - 1].second, gx, gy);
                exitDir = enterDir;
            }
            else
            {
                enterDir = directionFromTo(dragPath[i - 1].first, dragPath[i - 1].second, gx, gy);
                exitDir = directionFromTo(gx, gy, dragPath[i + 1].first, dragPath[i + 1].second);
            }

            size_t tileIndex = resolveTileIndex(enterDir, exitDir);

            // For straight pieces, resolve variant based on neighbor connectivity
            if (enterDir == exitDir)
            {
                auto layer = gridSystem->getBuildingLayer();
                uint8_t backDir = (exitDir + 2) % 4;

                // Internal connectivity: connected if there's an adjacent cell in the path
                bool connectedFront = (i < dragPath.size() - 1);
                bool connectedBack  = (i > 0);

                // External connectivity: check the grid for existing buildings
                if (not connectedFront)
                    connectedFront = gridSystem->isNeighborConnected(layer, gx, gy, exitDir);
                if (not connectedBack)
                    connectedBack = gridSystem->isNeighborConnected(layer, gx, gy, backDir);

                tileIndex = resolveLineTileVariant(exitDir, connectedBack, connectedFront);
            }

            size_t frameIndex = tileIndex * 8;
            std::string texName = def.textureName + "." + std::to_string(frameIndex);

            auto ghost = make2DTexture(ecsRef,
                static_cast<float>(Grid::TILE_SIZE),
                static_cast<float>(Grid::TILE_SIZE),
                texName);

            auto pos = ghost.get<PositionComponent>();
            pos->setX(wx);
            pos->setY(wy);
            pos->setZ(9.0f);

            ghost.get<Texture2DComponent>()->setOpacity(0.4f);
            ghost.get<Texture2DComponent>()->setViewport(GAME_VIEWPORT);

            dragGhostEntityIds.push_back(ghost.entity->id);
        }
    }

    void clearDragGhosts()
    {
        for (auto id : dragGhostEntityIds)
        {
            auto ent = ecsRef->getEntity(id);
            if (ent)
                ecsRef->removeEntity(id);
        }
        dragGhostEntityIds.clear();
    }

    void commitDragPath()
    {
        clearDragGhosts();

        auto layer = gridSystem->getBuildingLayer();
        const auto& def = getSelectedDef();

        // Phase 1: Place all belt cells
        std::vector<std::pair<int, int>> placedCells;

        for (size_t i = 0; i < dragPath.size(); ++i)
        {
            auto [gx, gy] = dragPath[i];

            const auto& existing = gridSystem->getCell(layer, gx, gy);

            // Skip non-conveyor occupied cells
            if (existing.tileId != 0 and existing.tileId != 4)
                continue;

            // Remove existing conveyor to replace with new direction
            if (existing.tileId == 4)
                gridSystem->removeBuilding(layer, gx, gy);

            uint8_t enterDir, exitDir;

            if (dragPath.size() == 1)
            {
                enterDir = exitDir = static_cast<uint8_t>(currentDirection);
            }
            else if (i == 0)
            {
                exitDir = directionFromTo(gx, gy, dragPath[1].first, dragPath[1].second);
                enterDir = exitDir;
            }
            else if (i == dragPath.size() - 1)
            {
                enterDir = directionFromTo(dragPath[i - 1].first, dragPath[i - 1].second, gx, gy);
                exitDir = enterDir;
            }
            else
            {
                enterDir = directionFromTo(dragPath[i - 1].first, dragPath[i - 1].second, gx, gy);
                exitDir = directionFromTo(gx, gy, dragPath[i + 1].first, dragPath[i + 1].second);
            }

            size_t tileIndex = resolveTileIndex(enterDir, exitDir);
            gridSystem->placeBuilding(layer, gx, gy, def, exitDir, tileIndex, enterDir);
            placedCells.push_back({gx, gy});
        }

        // Phase 2: Resolve correct variants for all newly placed straight belts
        for (const auto& [gx, gy] : placedCells)
            gridSystem->resolveAndUpdateBelt(layer, gx, gy);

        // Phase 3: Update existing neighbor belts adjacent to newly placed cells
        for (const auto& [gx, gy] : placedCells)
            gridSystem->updateNeighborBelts(layer, gx, gy);

        dragPath.clear();
    }

    // --- Members ---

    GridSystem* gridSystem = nullptr;
    CameraSystem* cameraSystem = nullptr;
    ToolbarSystem* toolbarSystem = nullptr;
    BuildingRegistry* registry = nullptr;
    TransportSystem* transportSystem = nullptr;

    size_t currentDirection = 0; // 0=Right, 1=Down, 2=Left, 3=Up
    bool leftMouseDown = false;
    size_t lastSelectedSlot = 0;

    uint64_t cursorEntityId = 0;
    uint64_t ghostEntityId = 0;

    // Line-drag state
    bool isDragging = false;
    std::vector<std::pair<int, int>> dragPath;
    std::vector<uint64_t> dragGhostEntityIds;
};
