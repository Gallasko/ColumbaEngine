#include "gamesystem.h"

#include "minersystem.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"

#include <SDL2/SDL.h>

#include <ctime>
#include <random>
#include <cstdio>

void GameSystem::init()
{
    printf("GameSystem: Grid ready (%dx%d, tile %dpx)\n",
        Grid::WIDTH, Grid::HEIGHT, Grid::TILE_SIZE);

    createCursorEntities();

    // Sync manual mining enabled state with initial hotbar selection
    if (manualMining)
        manualMining->setEnabled(not hasBuildingSelected());
}

void GameSystem::closeOtherGroup(UIPanel keep)
{
    if (keep != UIPanel::InventoryGroup)
    {
        // Children first so each panel's close() sees a consistent inventory
        // state. closeInventory() fires InventoryClosedEvent which would
        // re-close them, but doing it explicitly is safer if order changes.
        if (depotUI    and depotUI->isOpen())    depotUI->close();
        if (machineUI  and machineUI->isOpen())  machineUI->close();
        if (storageUI  and storageUI->isOpen())  storageUI->close();
        if (minerUI    and minerUI->isOpen())    minerUI->close();
        if (craftingUI and craftingUI->isOpen()) craftingUI->close();
        if (inventoryUI and inventoryUI->isOpen()) inventoryUI->closeInventory();
    }
    if (keep != UIPanel::Mission)
    {
        if (missionUI and missionUI->isOpen()) missionUI->close();
    }
}

void GameSystem::toggleInventoryFromHud()
{
    if (not inventoryUI)
        return;
    if (inventoryUI->isOpen())
    {
        inventoryUI->closeInventory();
    }
    else
    {
        closeOtherGroup(UIPanel::InventoryGroup);
        inventoryUI->openInventory();
    }
}

void GameSystem::toggleMissionFromHud()
{
    if (not missionUI)
        return;
    if (missionUI->isOpen())
    {
        missionUI->close();
    }
    else
    {
        closeOtherGroup(UIPanel::Mission);
        missionUI->open();
    }
}

void GameSystem::onProcessEvent(const OnSDLScanCode& event)
{
    if (machineDemo and machineDemo->isOpen())
        return;
    if (inventoryUI and inventoryUI->isOpen())
        return;
    if (minerUI and minerUI->isOpen())
        return;
    if (craftingUI and craftingUI->isOpen())
        return;
    if (machineUI and machineUI->isOpen())
        return;
    if (storageUI and storageUI->isOpen())
        return;
    if (depotUI and depotUI->isOpen())
        return;
    if (missionUI and missionUI->isOpen())
        return;

    if (event.key == SDL_SCANCODE_R)
    {
        const auto* def = getSelectedBuildingDef();
        if (def and def->hasDirection)
        {
            currentDirection = (currentDirection + 1) % 4;
            printf("Direction: %s\n", directionNames[currentDirection]);
            updateGhostTexture();
        }
    }

    // Debug: T rerolls the canvas with a fresh seed so we can eyeball
    // different procgen outputs. Player-placed buildings are preserved;
    // only terrain entities are destroyed and regenerated.
    if (event.key == SDL_SCANCODE_T and gridSystem)
    {
        std::random_device rd;
        uint32_t newSeed = rd();
        if (newSeed == 0) newSeed = static_cast<uint32_t>(std::time(nullptr));
        gridSystem->regenerateTerrain(newSeed);
    }
}

void GameSystem::onProcessEvent(const OnMouseClick& event)
{
    bool clickedOnPanel = panelClickedThisFrame;
    panelClickedThisFrame = false;

    if (machineDemo and machineDemo->isOpen())
        return;

    bool anyUIOpen = (inventoryUI and inventoryUI->isOpen())
                  or (minerUI and minerUI->isOpen())
                  or (craftingUI and craftingUI->isOpen())
                  or (machineUI and machineUI->isOpen())
                  or (storageUI and storageUI->isOpen())
                  or (depotUI and depotUI->isOpen())
                  or (missionUI and missionUI->isOpen());

    // Centralized click-outside-to-close for all UIs
    if (anyUIOpen)
    {
        if (event.button == SDL_BUTTON_LEFT)
        {
            bool onAnyPanel = clickedOnPanel
                           or (hotbar and hotbar->isMouseOverHotbar(event.pos.y));

            if (not onAnyPanel)
            {
                // Close every group explicitly. Relying on InventoryClosedEvent
                // to cascade to companion panels left depot/storage/etc. open
                // for one extra frame, which the user perceived as needing a
                // second click.
                closeOtherGroup(UIPanel::None);
            }
        }
        return; // Block all game input while any UI is open
    }

    if (event.button == SDL_BUTTON_LEFT)
    {
        // Skip if clicking on hotbar area
        if (isMouseOverHotbar())
            return;

        // Check if clicking on a miner — open its UI
        if (minerUI)
        {
            auto [gx, gy] = getMouseGridPos();
            auto layer = gridSystem->getBuildingLayer();
            if (gridSystem->getGrid().isInBounds(gx, gy))
            {
                const auto& cell = gridSystem->getCell(layer, gx, gy);
                if (cell.tileName == "Miner")
                {
                    minerUI->open(cell.ownerX, cell.ownerY);
                    return;
                }
            }
        }

        // Check if clicking on a furnace or assembler — open machine UI
        if (machineUI)
        {
            auto [gx, gy] = getMouseGridPos();
            auto layer = gridSystem->getBuildingLayer();
            if (gridSystem->getGrid().isInBounds(gx, gy))
            {
                const auto& cell = gridSystem->getCell(layer, gx, gy);
                if (cell.tileName == "Furnace" or cell.tileName == "Assembler")
                {
                    machineUI->open(cell.ownerX, cell.ownerY, cell.tileName);
                    return;
                }
            }
        }

        // Check if clicking on a storage — open storage UI
        if (storageUI)
        {
            auto [gx, gy] = getMouseGridPos();
            auto layer = gridSystem->getBuildingLayer();
            if (gridSystem->getGrid().isInBounds(gx, gy))
            {
                const auto& cell = gridSystem->getCell(layer, gx, gy);
                if (cell.tileName == "Storage")
                {
                    storageUI->open(gx, gy);
                    return;
                }
            }
        }

        // Check if clicking on a depot
        {
            auto [gx, gy] = getMouseGridPos();
            auto layer = gridSystem->getBuildingLayer();
            if (gridSystem->getGrid().isInBounds(gx, gy))
            {
                const auto& cell = gridSystem->getCell(layer, gx, gy);
                if (cell.tileName == "Depot")
                {
                    int ox = cell.isOwner ? gx : static_cast<int>(cell.ownerX);
                    int oy = cell.isOwner ? gy : static_cast<int>(cell.ownerY);

                    // If mission UI is waiting for depot selection, route there
                    if (missionUI and missionUI->isSelectingDepot())
                    {
                        missionUI->selectDepot(ox, oy);
                        return;
                    }

                    if (depotUI)
                    {
                        depotUI->open(ox, oy);
                        return;
                    }
                }
            }
        }

        const auto* def = getSelectedBuildingDef();

        if (def)
        {
            // Building is selected — place it
            if (def->mode == PlacementMode::LineDrag)
            {
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
        // else: no building selected — ManualMiningSystem handles mining clicks
    }
    else if (event.button == SDL_BUTTON_RIGHT)
    {
        removeAtMouse();
    }
}

void GameSystem::onProcessEvent(const OnMouseRelease& event)
{
    if ((inventoryUI and inventoryUI->isOpen())
     or (minerUI and minerUI->isOpen())
     or (craftingUI and craftingUI->isOpen())
     or (machineUI and machineUI->isOpen())
     or (storageUI and storageUI->isOpen())
     or (depotUI and depotUI->isOpen()))
        return;

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

void GameSystem::onProcessEvent(const OnSDLMouseMotion& event)
{
    if ((inventoryUI and inventoryUI->isOpen())
     or (minerUI and minerUI->isOpen())
     or (craftingUI and craftingUI->isOpen())
     or (machineUI and machineUI->isOpen())
     or (storageUI and storageUI->isOpen())
     or (depotUI and depotUI->isOpen()))
        return;

    updateCursorPosition();

    if (isDragging)
    {
        updateDragPathWithMouse();
    }
    else if (leftMouseDown and hasBuildingSelected())
    {
        placeAtMouse();
    }

    // Detect building selection changes to rebuild ghost
    const auto* currentDef = getSelectedBuildingDef();
    if (currentDef != lastBuildingDef)
    {
        lastBuildingDef = currentDef;
        rebuildGhostForSelectedBuilding();

        // Update mining system: enabled when no building is selected
        if (manualMining)
            manualMining->setEnabled(currentDef == nullptr);
    }
}

// --- Cursor & Ghost ---

void GameSystem::createCursorEntities()
{
    // Tile cursor — semi-transparent white highlight
    auto cursor = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{255.0f, 255.0f, 255.0f, 40.0f});

    auto cursorPos = cursor.get<PositionComponent>();
    cursorPos->setWidth(static_cast<float>(Grid::TILE_SIZE));
    cursorPos->setHeight(static_cast<float>(Grid::TILE_SIZE));
    cursorPos->setZ(10.0f);
    cursorPos->setX(-1000.0f);

    cursor.get<ViewportComponent>()->setViewport(GAME_VIEWPORT);
    cursorEntityId = cursor.entity->id;

    // Ghost block — preview of selected building (if any)
    rebuildGhostForSelectedBuilding();
}

void GameSystem::rebuildGhostForSelectedBuilding()
{
    // Destroy old ghost
    if (ghostEntityId != 0)
    {
        auto ent = ecsRef->getEntity(ghostEntityId);
        if (ent)
            ecsRef->removeEntity(ghostEntityId);
        ghostEntityId = 0;
    }

    const auto* def = getSelectedBuildingDef();

    // No building selected — reset cursor to 1x1 and hide ghost
    if (not def)
    {
        auto cursorEnt = ecsRef->getEntity(cursorEntityId);
        if (cursorEnt)
        {
            auto pos = cursorEnt->get<PositionComponent>();
            pos->setWidth(static_cast<float>(Grid::TILE_SIZE));
            pos->setHeight(static_cast<float>(Grid::TILE_SIZE));
        }
        return;
    }

    if (not def->textureName.empty())
    {
        // Textured ghost — works for both 1x1 (conveyors, inserters) and larger buildings
        float ghostW = static_cast<float>(Grid::TILE_SIZE * def->gridW);
        float ghostH = static_cast<float>(Grid::TILE_SIZE * def->gridH);

        size_t frameIndex = 0;
        if (def->gridW == 1 and def->gridH == 1)
        {
            if (def->name == "Conveyor")
            {
                size_t tileIndex = def->hasDirection ? DIRECTION_TILE_INDEX[currentDirection] : 0;
                frameIndex = tileIndex * 8;
            }
            else // Non-conveyor directional (inserter, etc.)
            {
                static constexpr size_t IDLE_FRAME[4] = {0, 2, 4, 6};
                frameIndex = def->hasDirection ? IDLE_FRAME[currentDirection] : 0;
            }
            // Inserter ghost is oversized to show the arm
            if (def->name == "Inserter")
            {
                ghostW = static_cast<float>(Grid::TILE_SIZE) * 3.0f;
                ghostH = ghostW;
            }
        }

        std::string texName = def->textureName + "." + std::to_string(frameIndex);
        auto ghost = make2DTexture(ecsRef, ghostW, ghostH, texName);

        ghost.get<PositionComponent>()->setZ(9.0f);
        ghost.get<PositionComponent>()->setX(-1000.0f);
        ghost.get<Texture2DComponent>()->setOpacity(0.4f);
        ghost.get<ViewportComponent>()->setViewport(GAME_VIEWPORT);
        ghostEntityId = ghost.entity->id;
    }
    else
    {
        // Colored ghost for buildings with no texture
        auto color = def->color;
        color.w = 100.0f;

        auto ghost = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, color);
        auto pos = ghost.get<PositionComponent>();
        pos->setZ(9.0f);
        pos->setX(-1000.0f);
        pos->setWidth(static_cast<float>(Grid::TILE_SIZE * def->gridW));
        pos->setHeight(static_cast<float>(Grid::TILE_SIZE * def->gridH));

        ghost.get<ViewportComponent>()->setViewport(GAME_VIEWPORT);
        ghostEntityId = ghost.entity->id;
    }

    // Resize cursor to match building footprint
    auto cursorEnt = ecsRef->getEntity(cursorEntityId);
    if (cursorEnt)
    {
        auto pos = cursorEnt->get<PositionComponent>();
        pos->setWidth(static_cast<float>(Grid::TILE_SIZE * def->gridW));
        pos->setHeight(static_cast<float>(Grid::TILE_SIZE * def->gridH));
    }
}

void GameSystem::updateCursorPosition()
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

        // Update ghost position (hidden during line drag, or when no building selected)
        const auto* def = getSelectedBuildingDef();
        if (not isDragging and def)
        {
            auto ghostEnt = ecsRef->getEntity(ghostEntityId);
            if (ghostEnt)
            {
                auto pos = ghostEnt->get<PositionComponent>();
                float ghostOffset = (def->name == "Inserter")
                    ? -static_cast<float>(Grid::TILE_SIZE) : 0.0f;
                pos->setX(wx + ghostOffset);
                pos->setY(wy + ghostOffset);

                // Tint ghost red if placement is invalid
                bool canPlace = canPlaceAt(gridX, gridY, *def);

                if (ghostEnt->has<Simple2DObject>())
                {
                    auto tint = canPlace
                        ? constant::Vector4D{def->color.x, def->color.y, def->color.z, 100.0f}
                        : constant::Vector4D{255.0f, 60.0f, 60.0f, 100.0f};
                    ghostEnt->get<Simple2DObject>()->setColors(tint);
                }
                if (ghostEnt->has<Texture2DComponent>())
                {
                    ghostEnt->get<Texture2DComponent>()->setOpacity(canPlace ? 0.4f : 0.15f);
                }
            }
        }
        else if (not isDragging and ghostEntityId != 0)
        {
            // No building selected, hide ghost
            auto ghostEnt = ecsRef->getEntity(ghostEntityId);
            if (ghostEnt)
                ghostEnt->get<PositionComponent>()->setX(-1000.0f);
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

void GameSystem::updateGhostTexture()
{
    const auto* def = getSelectedBuildingDef();
    if (not def or def->textureName.empty() or not def->hasDirection)
        return;

    auto ghostEnt = ecsRef->getEntity(ghostEntityId);
    if (not ghostEnt)
        return;

    size_t frameIndex;
    if (def->name == "Conveyor")
    {
        size_t tileIndex = DIRECTION_TILE_INDEX[currentDirection];
        frameIndex = tileIndex * 8;
    }
    else // Non-conveyor directional (inserter, etc.)
    {
        static constexpr size_t IDLE_FRAME[4] = {0, 2, 4, 6};
        frameIndex = IDLE_FRAME[currentDirection];
    }
    std::string texName = def->textureName + "." + std::to_string(frameIndex);

    ghostEnt->get<Texture2DComponent>()->setTexture(texName);
}

// --- Placement ---

bool GameSystem::canPlaceAt(int gx, int gy, const BuildingDef& def) const
{
    auto layer = gridSystem->getBuildingLayer();
    for (int dy = 0; dy < def.gridH; ++dy)
        for (int dx = 0; dx < def.gridW; ++dx)
        {
            int cx = gx + dx, cy = gy + dy;
            if (not gridSystem->getGrid().isInBounds(cx, cy))
                return false;
            if (not gridSystem->getCell(layer, cx, cy).tileName.empty())
                return false;
            if (isBlockingTerrain(gridSystem->getTerrainAt(cx, cy)))
                return false;
        }
    return true;
}

std::pair<int, int> GameSystem::getMouseGridPos() const
{
    float mouseX = cameraSystem->getLastMouseX();
    float mouseY = cameraSystem->getLastMouseY();
    auto worldPos = cameraSystem->screenToWorld(mouseX, mouseY);
    return gridSystem->getGrid().worldToGrid(worldPos.x, worldPos.y);
}

void GameSystem::placeAtMouse()
{
    const auto* def = getSelectedBuildingDef();
    if (not def)
        return;

    // Check if hotbar slot has items remaining
    const auto& item = hotbar->getSelectedItem();
    if (item.isEmpty())
        return;

    auto [gridX, gridY] = getMouseGridPos();

    if (not canPlaceAt(gridX, gridY, *def))
        return;

    auto layer = gridSystem->getBuildingLayer();
    size_t tileIndex = def->hasDirection ? DIRECTION_TILE_INDEX[currentDirection] : 0;
    gridSystem->placeBuilding(layer, gridX, gridY, *def, currentDirection, tileIndex);

    // Update adjacent belts to reflect the new neighbor
    for (int dy = 0; dy < def->gridH; ++dy)
        for (int dx = 0; dx < def->gridW; ++dx)
            gridSystem->updateNeighborBelts(layer, gridX + dx, gridY + dy);

    // Consume one building item from hotbar
    hotbar->consumeSelectedItem(1);
}

void GameSystem::removeAtMouse()
{
    auto [gridX, gridY] = getMouseGridPos();
    if (not gridSystem->getGrid().isInBounds(gridX, gridY))
        return;

    auto layer = gridSystem->getBuildingLayer();
    const auto& cell = gridSystem->getCell(layer, gridX, gridY);
    if (cell.tileName.empty())
        return;

    // Find the building item to return to inventory
    const std::string tileName = cell.isOwner ? cell.tileName
        : gridSystem->getCell(layer, cell.ownerX, cell.ownerY).tileName;

    gridSystem->removeBuilding(layer, gridX, gridY);

    // Return the building item to the player
    if (itemRegistry)
    {
        const auto* itemDef = itemRegistry->findByBuildingName(tileName);
        if (itemDef)
            sendEvent(PlayerGainItemEvent{itemDef->id, 1});
    }
}

bool GameSystem::isMouseOverHotbar() const
{
    if (not hotbar)
        return false;
    float mouseY = cameraSystem->getLastMouseY();
    return hotbar->isMouseOverHotbar(mouseY);
}

// --- Line Drag ---

uint8_t GameSystem::directionFromTo(int ax, int ay, int bx, int by)
{
    int dx = bx - ax;
    int dy = by - ay;
    if (dx > 0) return 0; // Right
    if (dy > 0) return 1; // Down
    if (dx < 0) return 2; // Left
    if (dy < 0) return 3; // Up
    return 0;
}

size_t GameSystem::resolveTileIndex(uint8_t enterDir, uint8_t exitDir) const
{
    if (enterDir == exitDir)
        return DIRECTION_TILE_INDEX[exitDir];

    size_t corner = cornerTileMap[enterDir][exitDir];
    if (corner == INVALID_CORNER)
        return DIRECTION_TILE_INDEX[exitDir];

    return corner;
}

void GameSystem::updateDragPathWithMouse()
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

    int stepX = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
    int stepY = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;

    int curX = last.first;
    int curY = last.second;

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

int GameSystem::findInPath(int x, int y) const
{
    for (size_t i = 0; i < dragPath.size(); ++i)
    {
        if (dragPath[i].first == x and dragPath[i].second == y)
            return static_cast<int>(i);
    }
    return -1;
}

void GameSystem::updateDragGhosts()
{
    clearDragGhosts();

    auto ghostEnt = ecsRef->getEntity(ghostEntityId);
    if (ghostEnt)
        ghostEnt->get<PositionComponent>()->setX(-1000.0f);

    const auto* def = getSelectedBuildingDef();
    if (not def)
        return;

    auto& grid = gridSystem->getGrid();

    for (size_t i = 0; i < dragPath.size(); ++i)
    {
        auto [gx, gy] = dragPath[i];
        auto [wx, wy] = grid.gridToWorld(gx, gy);

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

        if (enterDir == exitDir)
        {
            auto layer = gridSystem->getBuildingLayer();
            uint8_t backDir = (exitDir + 2) % 4;

            bool connectedFront = (i < dragPath.size() - 1);
            bool connectedBack  = (i > 0);

            if (not connectedFront)
                connectedFront = gridSystem->isNeighborConnected(layer, gx, gy, exitDir);
            if (not connectedBack)
                connectedBack = gridSystem->isNeighborConnected(layer, gx, gy, backDir);

            tileIndex = resolveLineTileVariant(exitDir, connectedBack, connectedFront);
        }

        size_t frameIndex = tileIndex * 8;
        std::string texName = def->textureName + "." + std::to_string(frameIndex);

        auto ghost = make2DTexture(ecsRef,
            static_cast<float>(Grid::TILE_SIZE),
            static_cast<float>(Grid::TILE_SIZE),
            texName);

        auto pos = ghost.get<PositionComponent>();
        pos->setX(wx);
        pos->setY(wy);
        pos->setZ(9.0f);

        ghost.get<Texture2DComponent>()->setOpacity(0.4f);
        ghost.get<ViewportComponent>()->setViewport(GAME_VIEWPORT);

        dragGhostEntityIds.push_back(ghost.entity->id);
    }
}

void GameSystem::clearDragGhosts()
{
    for (auto id : dragGhostEntityIds)
    {
        auto ent = ecsRef->getEntity(id);
        if (ent)
            ecsRef->removeEntity(id);
    }
    dragGhostEntityIds.clear();
}

void GameSystem::commitDragPath()
{
    clearDragGhosts();

    auto layer = gridSystem->getBuildingLayer();
    const auto* def = getSelectedBuildingDef();
    if (not def)
        return;

    std::vector<std::pair<int, int>> placedCells;

    for (size_t i = 0; i < dragPath.size(); ++i)
    {
        auto [gx, gy] = dragPath[i];

        // Check if we have items remaining
        const auto& item = hotbar->getSelectedItem();
        if (item.isEmpty())
            break;

        const auto& existing = gridSystem->getCell(layer, gx, gy);

        // Skip non-conveyor occupied cells
        if (not existing.tileName.empty() and existing.tileName != "Conveyor")
            continue;

        // Remove existing conveyor to replace with new direction
        if (existing.tileName == "Conveyor")
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
        gridSystem->placeBuilding(layer, gx, gy, *def, exitDir, tileIndex, enterDir);
        placedCells.push_back({gx, gy});

        // Consume one building item per placed cell
        hotbar->consumeSelectedItem(1);
    }

    // Phase 2: Resolve correct variants for all newly placed straight belts
    for (const auto& [gx, gy] : placedCells)
        gridSystem->resolveAndUpdateBelt(layer, gx, gy);

    // Phase 3: Update existing neighbor belts adjacent to newly placed cells
    for (const auto& [gx, gy] : placedCells)
        gridSystem->updateNeighborBelts(layer, gx, gy);

    dragPath.clear();
}
