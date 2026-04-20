#include "machinedemosystem.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>
#include <cmath>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// Available demos
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<DemoScenario> buildDemos()
{
    std::vector<DemoScenario> demos;
    demos.push_back(createInserterDemo());
    demos.push_back(createConveyorDemo());
    demos.push_back(createMinerDemo());
    demos.push_back(createFurnaceDemo());
    return demos;
}

static const std::vector<DemoScenario>& getDemos()
{
    static auto demos = buildDemos();
    return demos;
}

// ─────────────────────────────────────────────────────────────────────────────
// Event handlers
// ─────────────────────────────────────────────────────────────────────────────

void MachineDemoSystem::onEvent(const TickEvent& event)
{
    if (open)
    {
        tickAccumulator += static_cast<size_t>(event.tick);
    }
}

void MachineDemoSystem::onEvent(const OnSDLScanCode& event)
{
    if (open && event.key == SDL_SCANCODE_ESCAPE)
        closeDemo();
}

void MachineDemoSystem::onProcessEvent(const OnMouseClick& event)
{
    if (open)
    {
        if (isClickOnCloseBtn(event.pos.x, event.pos.y))
            closeDemo();
        // Consume all clicks while open
    }
}

void MachineDemoSystem::execute()
{
    if (pendingClose)
    {
        pendingClose = false;
        open = false;
        destroyPanel();
        printf("MachineDemoSystem: closed demo\n");
    }

    if (pendingOpen)
    {
        pendingOpen = false;
        for (const auto& demo : getDemos())
        {
            if (demo.tileId == pendingTileId)
            {
                currentScenario = demo;
                createPanel();
                initSimulation(currentScenario);
                open = true;
                tickAccumulator = 0;
                spawnTickCounter = 0;
                printf("MachineDemoSystem: opened demo for tileId %u\n", pendingTileId);
                break;
            }
        }
    }

    if (not open)
        return;

    while (tickAccumulator >= TICK_INTERVAL_MS)
    {
        tickAccumulator -= TICK_INTERVAL_MS;
        simulationTick();
    }

    updateRendering();
}

// ─────────────────────────────────────────────────────────────────────────────
// Open / Close
// ─────────────────────────────────────────────────────────────────────────────

bool MachineDemoSystem::hasDemoForTile(uint16_t tileId) const
{
    for (const auto& d : getDemos())
    {
        if (d.tileId == tileId)
            return true;
    }
    return false;
}

void MachineDemoSystem::openDemo(uint16_t tileId)
{
    pendingOpen = true;
    pendingTileId = tileId;
}

void MachineDemoSystem::closeDemo()
{
    pendingClose = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Panel creation / destruction
// ─────────────────────────────────────────────────────────────────────────────

float MachineDemoSystem::panelW() const
{
    return currentScenario.gridW * DEMO_TILE_SIZE + PADDING * 2;
}

float MachineDemoSystem::panelH() const
{
    return currentScenario.gridH * DEMO_TILE_SIZE + PADDING * 2 + TITLE_HEIGHT + DESC_HEIGHT;
}

float MachineDemoSystem::panelX() const
{
    return (screenW - panelW()) * 0.5f;
}

float MachineDemoSystem::panelY() const
{
    return (screenH - panelH()) * 0.5f;
}

float MachineDemoSystem::gridOriginX() const
{
    return panelX() + PADDING;
}

float MachineDemoSystem::gridOriginY() const
{
    return panelY() + TITLE_HEIGHT + DESC_HEIGHT;
}

void MachineDemoSystem::createPanel()
{
    if (panelCreated)
        destroyPanel();

    float pw = panelW();
    float ph = panelH();
    float px = panelX();
    float py = panelY();

    // Backdrop
    auto bd = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        {20.0f, 20.0f, 30.0f, 230.0f});
    auto bdPos = bd.get<PositionComponent>();
    bdPos->setX(px);
    bdPos->setY(py);
    bdPos->setZ(195.0f);
    bdPos->setWidth(pw);
    bdPos->setHeight(ph);
    bdPos->setVisibility(true);
    bd.get<Simple2DObject>()->setViewport(UI_VP);
    backdropId = bd.entity->id;

    // Title
    auto title = makeTTFText(ecsRef,
        px + PADDING, py + PADDING, 198.0f,
        FONT_PATH, currentScenario.title, TITLE_SCALE,
        {255.0f, 210.0f, 80.0f, 255.0f});
    title.get<TTFText>()->setViewport(UI_VP);
    titleId = title.entity->id;

    // Description
    auto desc = makeTTFText(ecsRef,
        px + PADDING, py + PADDING + 22.0f, 198.0f,
        FONT_PATH, currentScenario.description, DESC_SCALE,
        {210.0f, 210.0f, 210.0f, 255.0f});
    desc.get<TTFText>()->setViewport(UI_VP);
    descId = desc.entity->id;

    // Close button background
    auto closeBtn = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        {180.0f, 60.0f, 60.0f, 220.0f});
    auto cbPos = closeBtn.get<PositionComponent>();
    cbPos->setX(px + pw - CLOSE_BTN_SIZE - 4.0f);
    cbPos->setY(py + 4.0f);
    cbPos->setZ(199.0f);
    cbPos->setWidth(CLOSE_BTN_SIZE);
    cbPos->setHeight(CLOSE_BTN_SIZE);
    cbPos->setVisibility(true);
    closeBtn.get<Simple2DObject>()->setViewport(UI_VP);
    closeBtnId = closeBtn.entity->id;

    // Close button text "X"
    auto closeTxt = makeTTFText(ecsRef,
        px + pw - CLOSE_BTN_SIZE + 2.0f, py + 6.0f, 199.0f,
        FONT_PATH, "X", 0.35f,
        {255.0f, 255.0f, 255.0f, 255.0f});
    closeTxt.get<TTFText>()->setViewport(UI_VP);
    closeBtnTextId = closeTxt.entity->id;

    panelCreated = true;
}

void MachineDemoSystem::destroyPanel()
{
    if (!panelCreated)
        return;

    // Remove panel entities
    auto removeEntity = [this](uint64_t& id) {
        if (id != 0) { ecsRef->removeEntity(id); id = 0; }
    };

    removeEntity(backdropId);
    removeEntity(titleId);
    removeEntity(descId);
    removeEntity(closeBtnId);
    removeEntity(closeBtnTextId);

    // Remove simulation entities
    for (size_t i = 0; i < MAX_ITEMS; ++i)
    {
        if (items[i].entityId != 0)
        {
            ecsRef->removeEntity(items[i].entityId);
            items[i].entityId = 0;
        }
        items[i].active = false;
    }

    for (size_t i = 0; i < inserterCount; ++i)
    {
        if (inserters[i].entityId != 0)
        {
            ecsRef->removeEntity(inserters[i].entityId);
            inserters[i].entityId = 0;
        }
    }
    inserterCount = 0;

    for (size_t i = 0; i < beltCount; ++i)
    {
        if (belts[i].entityId != 0)
        {
            ecsRef->removeEntity(belts[i].entityId);
            belts[i].entityId = 0;
        }
    }
    beltCount = 0;

    for (size_t i = 0; i < machineCount; ++i)
    {
        if (machines[i].entityId != 0)
        {
            ecsRef->removeEntity(machines[i].entityId);
            machines[i].entityId = 0;
        }
    }
    machineCount = 0;

    panelCreated = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Simulation init
// ─────────────────────────────────────────────────────────────────────────────

void MachineDemoSystem::initSimulation(const DemoScenario& scenario)
{
    inserterCount = 0;
    beltCount = 0;
    beltAnimFrame = 0;
    machineCount = 0;
    for (size_t i = 0; i < MAX_ITEMS; ++i)
        items[i].active = false;

    float ox = gridOriginX();
    float oy = gridOriginY();

    for (const auto& tile : scenario.tiles)
    {
        float worldX = ox + tile.x * DEMO_TILE_SIZE;
        float worldY = oy + tile.y * DEMO_TILE_SIZE;

        switch (tile.tileId)
        {
            case 4: // Belt
            {
                if (beltCount >= MAX_BELTS) break;
                auto& belt = belts[beltCount];
                belt.gridX = tile.x;
                belt.gridY = tile.y;
                belt.direction = tile.direction;
                belt.carriedItemIndex = -1;

                // Belt frame: tileIndex * 8 + animFrame(0)
                static constexpr size_t DIR_TO_TILE[4] = {LINE_RIGHT_1, LINE_DOWN_1, LINE_LEFT_1, LINE_UP_1};
                size_t beltFrame = DIR_TO_TILE[tile.direction] * 8;
                auto tex = make2DTexture(ecsRef, DEMO_TILE_SIZE, DEMO_TILE_SIZE,
                    "Conveyor_Belt." + std::to_string(beltFrame));
                auto pos = tex.get<PositionComponent>();
                pos->setX(worldX);
                pos->setY(worldY);
                pos->setZ(196.0f);
                pos->setVisibility(true);
                tex.get<Texture2DComponent>()->setViewport(UI_VP);
                belt.entityId = tex.entity->id;
                beltCount++;
                break;
            }

            case 8: // Inserter
            {
                if (inserterCount >= MAX_INSERTERS) break;
                auto& ins = inserters[inserterCount];
                ins.gridX = tile.x;
                ins.gridY = tile.y;
                ins.direction = tile.direction;
                ins.state = DemoInserterState::Idle;
                ins.animFrame = 0;
                ins.heldItemIndex = -1;

                // Create inserter arm sprite - 3x scale centered
                float armSize = DEMO_TILE_SIZE * 3.0f;
                float offset = DEMO_TILE_SIZE;
                std::string armTex = "Robotic_Arms_1." + std::to_string(INSERTER_PICKUP_FRAME[tile.direction]);
                auto tex = make2DTexture(ecsRef, armSize, armSize, armTex);
                auto pos = tex.get<PositionComponent>();
                pos->setX(worldX - offset);
                pos->setY(worldY - offset);
                pos->setZ(197.0f);
                pos->setVisibility(true);
                tex.get<Texture2DComponent>()->setViewport(UI_VP);
                ins.entityId = tex.entity->id;
                inserterCount++;
                break;
            }

            case 5: // Furnace
            case 6: // Assembler
            {
                if (machineCount >= MAX_MACHINES) break;
                auto& mach = machines[machineCount];
                mach.gridX = tile.x;
                mach.gridY = tile.y;
                mach.tileId = tile.tileId;
                mach.inputItemIndex = -1;
                mach.outputItemIndex = -1;
                mach.processTimer = 0;

                std::string texName = (tile.tileId == 5) ? "Stone_Furnace.0" : "Assembler_Machine_1.0";
                float w = DEMO_TILE_SIZE * 2.0f;
                float h = DEMO_TILE_SIZE * 3.0f;

                auto tex = make2DTexture(ecsRef, w, h, texName);
                auto pos = tex.get<PositionComponent>();
                pos->setX(worldX);
                pos->setY(worldY);
                pos->setZ(197.0f);
                pos->setVisibility(true);
                tex.get<Texture2DComponent>()->setViewport(UI_VP);
                mach.entityId = tex.entity->id;
                machineCount++;
                break;
            }

            case 7: // Miner
            {
                if (machineCount >= MAX_MACHINES) break;
                auto& mach = machines[machineCount];
                mach.gridX = tile.x;
                mach.gridY = tile.y;
                mach.tileId = tile.tileId;
                mach.inputItemIndex = -1;
                mach.outputItemIndex = -1;
                mach.processTimer = 0;

                float w = DEMO_TILE_SIZE * 2.0f;
                float h = DEMO_TILE_SIZE * 3.0f;

                auto tex = make2DTexture(ecsRef, w, h, "Miner_Machine_1.0");
                auto pos = tex.get<PositionComponent>();
                pos->setX(worldX);
                pos->setY(worldY);
                pos->setZ(197.0f);
                pos->setVisibility(true);
                tex.get<Texture2DComponent>()->setViewport(UI_VP);
                mach.entityId = tex.entity->id;
                machineCount++;
                break;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Simulation tick
// ─────────────────────────────────────────────────────────────────────────────

void MachineDemoSystem::simulationTick()
{
    spawnTickCounter++;
    tickSpawns();
    tickBelts();
    tickInserters();
    tickMachines();

    // Advance belt animation frame (synced with simulation tick)
    beltAnimFrame = (beltAnimFrame + 1) % 8;
    static constexpr size_t DIR_TO_TILE[4] = {LINE_RIGHT_1, LINE_DOWN_1, LINE_LEFT_1, LINE_UP_1};
    for (size_t i = 0; i < beltCount; ++i)
    {
        size_t frame = DIR_TO_TILE[belts[i].direction] * 8 + beltAnimFrame;
        auto ent = ecsRef->getEntity(belts[i].entityId);
        if (ent && ent->has<Texture2DComponent>())
            ent->get<Texture2DComponent>()->setTexture("Conveyor_Belt." + std::to_string(frame));
    }
}

void MachineDemoSystem::tickSpawns()
{
    for (const auto& spawn : currentScenario.itemSpawns)
    {
        if (spawn.spawnIntervalTicks <= 0)
            continue;
        if ((spawnTickCounter % spawn.spawnIntervalTicks) != 0)
            continue;

        // For miner demo: spawn directly on belt at spawn position
        int beltIdx = findBeltAt(spawn.spawnX, spawn.spawnY);
        if (beltIdx >= 0 && belts[beltIdx].carriedItemIndex < 0)
        {
            float ox = gridOriginX();
            float oy = gridOriginY();
            float ix = ox + spawn.spawnX * DEMO_TILE_SIZE + DEMO_TILE_SIZE * 0.25f;
            float iy = oy + spawn.spawnY * DEMO_TILE_SIZE + DEMO_TILE_SIZE * 0.25f;
            int itemIdx = allocateItem(spawn.itemId, ix, iy);
            if (itemIdx >= 0)
                belts[beltIdx].carriedItemIndex = itemIdx;
        }
    }
}

void MachineDemoSystem::tickBelts()
{
    // Iterate in reverse so downstream belts process first —
    // prevents items from cascading through all belts in one tick.
    for (size_t i = beltCount; i-- > 0;)
    {
        auto& belt = belts[i];
        if (belt.carriedItemIndex < 0)
            continue;

        // Move item to next belt position
        int nx = belt.gridX + DIR_DX[belt.direction];
        int ny = belt.gridY + DIR_DY[belt.direction];

        int nextBeltIdx = findBeltAt(nx, ny);
        if (nextBeltIdx >= 0 && belts[nextBeltIdx].carriedItemIndex < 0)
        {
            // Transfer to next belt
            belts[nextBeltIdx].carriedItemIndex = belt.carriedItemIndex;
            belt.carriedItemIndex = -1;

            // Update item position
            auto& item = items[belts[nextBeltIdx].carriedItemIndex];
            float ox = gridOriginX();
            float oy = gridOriginY();
            item.x = ox + nx * DEMO_TILE_SIZE + DEMO_TILE_SIZE * 0.25f;
            item.y = oy + ny * DEMO_TILE_SIZE + DEMO_TILE_SIZE * 0.25f;
        }
        else if (nextBeltIdx < 0)
        {
            // Item falls off end of belt — despawn
            freeItem(belt.carriedItemIndex);
            belt.carriedItemIndex = -1;
        }
        // else: next belt is full, stall
    }
}

void MachineDemoSystem::tickInserters()
{
    for (size_t i = 0; i < inserterCount; ++i)
    {
        auto& ins = inserters[i];

        switch (ins.state)
        {
            case DemoInserterState::Idle:
            {
                // Try to pick up from behind
                int pickupX = ins.gridX - DIR_DX[ins.direction];
                int pickupY = ins.gridY - DIR_DY[ins.direction];

                // Check belt at pickup position
                int beltIdx = findBeltAt(pickupX, pickupY);
                if (beltIdx >= 0 && belts[beltIdx].carriedItemIndex >= 0)
                {
                    ins.heldItemIndex = belts[beltIdx].carriedItemIndex;
                    belts[beltIdx].carriedItemIndex = -1;
                    ins.state = DemoInserterState::Swinging;
                    ins.animFrame = 0;
                    updateInserterSprite(ins);
                }
                // Check machine output at pickup
                else
                {
                    int machIdx = findMachineAt(pickupX, pickupY);
                    if (machIdx >= 0 && machines[machIdx].outputItemIndex >= 0)
                    {
                        ins.heldItemIndex = machines[machIdx].outputItemIndex;
                        machines[machIdx].outputItemIndex = -1;
                        ins.state = DemoInserterState::Swinging;
                        ins.animFrame = 0;
                        updateInserterSprite(ins);
                    }
                }
                break;
            }

            case DemoInserterState::Swinging:
            {
                if (ins.animFrame < INSERTER_SWING_FRAMES - 1)
                {
                    ins.animFrame++;
                    updateInserterSprite(ins);

                    // Update held item position along arc
                    if (ins.heldItemIndex >= 0)
                    {
                        float t = static_cast<float>(ins.animFrame) / static_cast<float>(INSERTER_SWING_FRAMES - 1);
                        float ox = gridOriginX();
                        float oy = gridOriginY();
                        float cx = ox + ins.gridX * DEMO_TILE_SIZE + DEMO_TILE_SIZE * 0.5f;
                        float cy = oy + ins.gridY * DEMO_TILE_SIZE + DEMO_TILE_SIZE * 0.5f;

                        int pickupX = ins.gridX - DIR_DX[ins.direction];
                        int pickupY = ins.gridY - DIR_DY[ins.direction];
                        float pcx = ox + pickupX * DEMO_TILE_SIZE + DEMO_TILE_SIZE * 0.5f;
                        float pcy = oy + pickupY * DEMO_TILE_SIZE + DEMO_TILE_SIZE * 0.5f;

                        float startAngle = std::atan2(pcy - cy, pcx - cx);
                        float angle = startAngle + t * static_cast<float>(M_PI);
                        float radius = DEMO_TILE_SIZE;
                        float itemHalf = DEMO_TILE_SIZE * 0.25f;

                        items[ins.heldItemIndex].x = cx + radius * std::cos(angle) - itemHalf;
                        items[ins.heldItemIndex].y = cy + radius * std::sin(angle) - itemHalf;
                    }
                }
                else
                {
                    // At drop position — try to drop
                    int dropX = ins.gridX + DIR_DX[ins.direction];
                    int dropY = ins.gridY + DIR_DY[ins.direction];

                    bool dropped = false;

                    // Try drop on belt
                    int beltIdx = findBeltAt(dropX, dropY);
                    if (beltIdx >= 0 && belts[beltIdx].carriedItemIndex < 0)
                    {
                        belts[beltIdx].carriedItemIndex = ins.heldItemIndex;
                        float ox = gridOriginX();
                        float oy = gridOriginY();
                        items[ins.heldItemIndex].x = ox + dropX * DEMO_TILE_SIZE + DEMO_TILE_SIZE * 0.25f;
                        items[ins.heldItemIndex].y = oy + dropY * DEMO_TILE_SIZE + DEMO_TILE_SIZE * 0.25f;
                        ins.heldItemIndex = -1;
                        dropped = true;
                    }

                    // Try drop in machine
                    if (!dropped)
                    {
                        int machIdx = findMachineAt(dropX, dropY);
                        if (machIdx >= 0 && machines[machIdx].inputItemIndex < 0)
                        {
                            machines[machIdx].inputItemIndex = ins.heldItemIndex;
                            // Hide item (it's inside the machine now)
                            items[ins.heldItemIndex].x = -100.0f;
                            items[ins.heldItemIndex].y = -100.0f;
                            ins.heldItemIndex = -1;
                            dropped = true;
                        }
                    }

                    if (dropped)
                    {
                        ins.state = DemoInserterState::Returning;
                        ins.animFrame = 0;
                        updateInserterSprite(ins);
                    }
                    // else stall
                }
                break;
            }

            case DemoInserterState::Returning:
            {
                if (ins.animFrame < INSERTER_SWING_FRAMES - 1)
                {
                    ins.animFrame++;
                    updateInserterSprite(ins);
                }
                else
                {
                    ins.state = DemoInserterState::Idle;
                    ins.animFrame = 0;
                    updateInserterSprite(ins);
                }
                break;
            }
        }
    }
}

void MachineDemoSystem::tickMachines()
{
    for (size_t i = 0; i < machineCount; ++i)
    {
        auto& mach = machines[i];

        // Miner: periodically produce output
        if (mach.tileId == 7)
        {
            // Miner output is handled via spawn system
            continue;
        }

        // Furnace/Assembler: process input into output
        if (mach.inputItemIndex >= 0 && mach.outputItemIndex < 0)
        {
            mach.processTimer++;
            if (mach.processTimer >= 10) // 10 ticks to smelt
            {
                mach.processTimer = 0;
                // Transform input into output (iron ore -> iron plate for demo)
                uint16_t outputItem = 5; // Iron Plate
                freeItem(mach.inputItemIndex);
                mach.inputItemIndex = -1;

                float ox = gridOriginX();
                float oy = gridOriginY();
                float mx = ox + mach.gridX * DEMO_TILE_SIZE + DEMO_TILE_SIZE;
                float my = oy + mach.gridY * DEMO_TILE_SIZE + DEMO_TILE_SIZE;
                mach.outputItemIndex = allocateItem(outputItem, mx, my);
            }
        }
    }
}

void MachineDemoSystem::resetSimulation()
{
    // Free all items and reset state
    for (size_t i = 0; i < MAX_ITEMS; ++i)
    {
        if (items[i].active)
            freeItem(static_cast<int>(i));
    }

    for (size_t i = 0; i < beltCount; ++i)
        belts[i].carriedItemIndex = -1;

    for (size_t i = 0; i < inserterCount; ++i)
    {
        inserters[i].state = DemoInserterState::Idle;
        inserters[i].animFrame = 0;
        inserters[i].heldItemIndex = -1;
        updateInserterSprite(inserters[i]);
    }

    for (size_t i = 0; i < machineCount; ++i)
    {
        machines[i].inputItemIndex = -1;
        machines[i].outputItemIndex = -1;
        machines[i].processTimer = 0;
    }

    spawnTickCounter = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering updates
// ─────────────────────────────────────────────────────────────────────────────

void MachineDemoSystem::updateRendering()
{
    for (size_t i = 0; i < MAX_ITEMS; ++i)
    {
        if (!items[i].active)
            continue;
        updateItemPosition(items[i]);
    }
}

void MachineDemoSystem::updateInserterSprite(DemoSimInserter& ins)
{
    bool returning = (ins.state == DemoInserterState::Returning);
    size_t frame = getInserterSpriteFrame(ins.direction, ins.animFrame, returning);

    auto ent = ecsRef->getEntity(ins.entityId);
    if (ent && ent->has<Texture2DComponent>())
    {
        ent->get<Texture2DComponent>()->setTexture(
            "Robotic_Arms_1." + std::to_string(frame));
    }
}

void MachineDemoSystem::updateItemPosition(DemoSimItem& item)
{
    if (item.entityId == 0)
        return;
    auto ent = ecsRef->getEntity(item.entityId);
    if (!ent)
        return;
    auto pos = ent->get<PositionComponent>();
    pos->setX(item.x);
    pos->setY(item.y);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

int MachineDemoSystem::findBeltAt(int x, int y) const
{
    for (size_t i = 0; i < beltCount; ++i)
    {
        if (belts[i].gridX == x && belts[i].gridY == y)
            return static_cast<int>(i);
    }
    return -1;
}

int MachineDemoSystem::findMachineAt(int x, int y) const
{
    for (size_t i = 0; i < machineCount; ++i)
    {
        auto& m = machines[i];
        int w = (m.tileId == 5 || m.tileId == 6 || m.tileId == 7) ? 2 : 1;
        int h = (m.tileId == 5 || m.tileId == 6 || m.tileId == 7) ? 3 : 1;
        if (x >= m.gridX && x < m.gridX + w && y >= m.gridY && y < m.gridY + h)
            return static_cast<int>(i);
    }
    return -1;
}

int MachineDemoSystem::allocateItem(uint16_t itemId, float x, float y)
{
    for (size_t i = 0; i < MAX_ITEMS; ++i)
    {
        if (!items[i].active)
        {
            items[i].active = true;
            items[i].itemId = itemId;
            items[i].x = x;
            items[i].y = y;

            // Create visual entity
            const auto& itemDef = itemRegistry->get(itemId);
            float itemSize = DEMO_TILE_SIZE * 0.5f;
            std::string texName = itemDef.textureName.empty() ? "Items" : itemDef.textureName;

            auto tex = make2DTexture(ecsRef, itemSize, itemSize, texName);
            auto pos = tex.get<PositionComponent>();
            pos->setX(x);
            pos->setY(y);
            pos->setZ(198.0f);
            pos->setVisibility(true);
            tex.get<Texture2DComponent>()->setViewport(UI_VP);
            items[i].entityId = tex.entity->id;

            return static_cast<int>(i);
        }
    }
    return -1;
}

void MachineDemoSystem::freeItem(int index)
{
    if (index < 0 || index >= static_cast<int>(MAX_ITEMS))
        return;
    if (!items[index].active)
        return;

    if (items[index].entityId != 0)
    {
        ecsRef->removeEntity(items[index].entityId);
        items[index].entityId = 0;
    }
    items[index].active = false;
}

bool MachineDemoSystem::isClickOnPanel(float x, float y) const
{
    return x >= panelX() && x <= panelX() + panelW() &&
           y >= panelY() && y <= panelY() + panelH();
}

bool MachineDemoSystem::isClickOnCloseBtn(float x, float y) const
{
    float bx = panelX() + panelW() - CLOSE_BTN_SIZE - 4.0f;
    float by = panelY() + 4.0f;
    return x >= bx && x <= bx + CLOSE_BTN_SIZE &&
           y >= by && y <= by + CLOSE_BTN_SIZE;
}


size_t MachineDemoSystem::getInserterSpriteFrame(uint8_t direction, size_t animFrame, bool returning)
{
    size_t base = INSERTER_PICKUP_FRAME[direction];
    if (returning)
        return (base + INSERTER_SWING_FRAMES - 1 - animFrame) % INSERTER_TOTAL_FRAMES;
    else
        return (base + animFrame) % INSERTER_TOTAL_FRAMES;
}
