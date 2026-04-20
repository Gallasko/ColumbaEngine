#include "craftingsystem.h"

#include "playerinventory.h"
#include "worldfacts.h"
#include "2D/texture.h"

#include <cstdio>

void CraftingSystem::save(Archive& archive)
{
    serialize(archive, "machines", machines);
    printf("CraftingSystem: saved %zu machines\n", machines.size());
}

void CraftingSystem::load(const UnserializedObject& serializedString)
{
    defaultDeserialize(serializedString, "machines", machines);
    printf("CraftingSystem: loaded %zu machines\n", machines.size());

    // Restore runtime-only state not persisted to disk
    size_t buildingLayer = gridSystem->getBuildingLayer();
    for (auto& [key, machine] : machines)
    {
        const auto& cell = gridSystem->getGrid().getCell(buildingLayer, machine.ownerX, machine.ownerY);
        machine.entityId = cell.entityId;
        machine.animFrame = 0;
        machine.isCrafting = (machine.currentRecipe != nullptr);

        // Furnace active sprite is 64px tall; restore correct size after load
        if (machine.machineType == 5 and machine.isCrafting)
        {
            auto ent = ecsRef->getEntity(machine.entityId);
            if (ent)
            {
                auto pos = ent->get<PositionComponent>();
                pos->setY(pos->getY() - 16.0f);
                pos->setHeight(64.0f);
            }
        }
    }
}

void CraftingSystem::onEvent(const BuildingPlacedEvent& event)
{
    if (event.tileId == 5 or event.tileId == 6)
        registerMachine(event.x, event.y, event.tileId);
}

void CraftingSystem::onEvent(const BuildingRemovedEvent& event)
{
    if (event.tileId == 5 or event.tileId == 6)
        unregisterMachine(event.x, event.y);
}

void CraftingSystem::execute()
{
    // Advance animation for all active machines
    if (animAccumulator >= ANIM_FRAME_DURATION_MS)
    {
        animAccumulator -= ANIM_FRAME_DURATION_MS;

        for (auto& [key, machine] : machines)
        {
            if (not machine.isCrafting)
                continue;

            size_t numFrames = (machine.machineType == 5)
                ? FURNACE_ANIM_FRAMES
                : ASSEMBLER_ANIM_FRAMES;

            machine.animFrame = (machine.animFrame + 1) % numFrames;

            auto ent = ecsRef->getEntity(machine.entityId);
            if (ent and ent->has<Texture2DComponent>())
            {
                std::string atlas = (machine.machineType == 5)
                    ? "Stone_Furnace_Active"
                    : "Assembler_Machine_1_Running";
                ent->get<Texture2DComponent>()->setTexture(
                    atlas + "." + std::to_string(machine.animFrame));
            }
        }
    }

    while (tickAccumulator >= CRAFT_TICK_MS)
    {
        tickAccumulator -= CRAFT_TICK_MS;
        craftTick();
    }
}

void CraftingSystem::registerMachine(int x, int y, uint16_t tileId)
{
    MachineData data;
    data.ownerX = x;
    data.ownerY = y;
    data.machineType = tileId;

    if (tileId == 5) // Furnace: 1 input, 1 output
    {
        data.inputSlots = Inventory(1);
        data.outputSlots = Inventory(1);
    }
    else if (tileId == 6) // Assembler: 2 inputs, 1 output
    {
        data.inputSlots = Inventory(2);
        data.outputSlots = Inventory(1);
    }

    // Capture entity spawned by GridSystem for texture swapping
    size_t buildingLayer = gridSystem->getBuildingLayer();
    const auto& cell = gridSystem->getGrid().getCell(buildingLayer, x, y);
    data.entityId = cell.entityId;

    machines[machineKey(x, y)] = data;
}

void CraftingSystem::unregisterMachine(int x, int y)
{
    auto it = machines.find(machineKey(x, y));
    if (it != machines.end())
    {
        auto& machine = it->second;

        // Return input slot items to player
        for (const auto& slot : machine.inputSlots.slots)
        {
            if (not slot.isEmpty())
                sendEvent(PlayerGainItemEvent{slot.id, slot.count});
        }

        // Return output slot items to player
        for (const auto& slot : machine.outputSlots.slots)
        {
            if (not slot.isEmpty())
                sendEvent(PlayerGainItemEvent{slot.id, slot.count});
        }

        machines.erase(it);
    }
}

void CraftingSystem::craftTick()
{
    size_t buildingLayer = gridSystem->getBuildingLayer();
    const auto& grid = gridSystem->getGrid();

    for (auto& [key, machine] : machines)
    {
        // Phase 1: pull items from adjacent input belts
        pullFromBelts(machine, grid, buildingLayer);

        // Phase 2: try to start or continue crafting
        if (machine.currentRecipe == nullptr)
        {
            const Recipe* matched = nullptr;
            if (machine.lockedRecipe)
            {
                bool ok = true;
                for (const auto& input : machine.lockedRecipe->inputs)
                    if (not machine.inputSlots.hasAtLeast(input.id, input.count))
                        { ok = false; break; }
                if (ok) matched = machine.lockedRecipe;
            }
            else
            {
                matched = recipeRegistry->findMatchingRecipe(
                    machine.machineType, machine.inputSlots);
            }
            machine.currentRecipe = matched;

            if (machine.currentRecipe)
            {
                // Consume inputs immediately
                for (const auto& input : machine.currentRecipe->inputs)
                    machine.inputSlots.remove(input.id, input.count);
                machine.craftProgress = 0;

                // Start animation
                if (not machine.isCrafting)
                {
                    machine.isCrafting = true;
                    machine.animFrame = 0;

                    auto ent = ecsRef->getEntity(machine.entityId);
                    if (ent and ent->has<Texture2DComponent>())
                    {
                        std::string atlas = (machine.machineType == 5)
                            ? "Stone_Furnace_Active"
                            : "Assembler_Machine_1_Running";
                        ent->get<Texture2DComponent>()->setTexture(atlas + ".0");
                    }
                    // Furnace active sprite is 32x64 (grows upward by 16px)
                    if (machine.machineType == 5 and ent)
                    {
                        auto pos = ent->get<PositionComponent>();
                        pos->setY(pos->getY() - 16.0f);
                        pos->setHeight(64.0f);
                    }
                }
            }
        }

        if (machine.currentRecipe)
        {
            machine.craftProgress += CRAFT_TICK_MS;

            if (machine.craftProgress >= machine.currentRecipe->craftTimeMs)
            {
                // Check if output can accept all products
                bool canOutput = true;
                for (const auto& output : machine.currentRecipe->outputs)
                {
                    if (not machine.outputSlots.canAccept(output.id, *itemRegistry))
                    {
                        canOutput = false;
                        break;
                    }
                }

                if (canOutput)
                {
                    for (const auto& output : machine.currentRecipe->outputs)
                    {
                        machine.outputSlots.insert(output.id, output.count, *itemRegistry);

                        // Emit crafted_ and discovered_ facts so recipe
                        // unlock gates work for machine-produced items too.
                        const auto& def = itemRegistry->get(output.id);
                        std::string itemSnake;
                        for (char c : def.name)
                        {
                            if (c == ' ')
                                itemSnake.push_back('_');
                            else if (c >= 'A' and c <= 'Z')
                                itemSnake.push_back(static_cast<char>(c - 'A' + 'a'));
                            else
                                itemSnake.push_back(c);
                        }
                        ecsRef->sendEvent(pg::IncreaseFact{"crafted_" + itemSnake, static_cast<int>(output.count)});
                        ecsRef->sendEvent(pg::AddFact{"discovered_" + itemSnake, ElementType{true}});
                    }
                    machine.currentRecipe = nullptr;
                    machine.craftProgress = 0;

                    // Stop animation, return to idle texture
                    machine.isCrafting = false;
                    machine.animFrame = 0;

                    auto ent = ecsRef->getEntity(machine.entityId);
                    if (ent and ent->has<Texture2DComponent>())
                    {
                        std::string idle = (machine.machineType == 5)
                            ? "Stone_Furnace.0"
                            : "Assembler_Machine_1.0";
                        ent->get<Texture2DComponent>()->setTexture(idle);
                    }
                    // Restore furnace to idle size (32x48)
                    if (machine.machineType == 5 and ent)
                    {
                        auto pos = ent->get<PositionComponent>();
                        pos->setY(pos->getY() + 16.0f);
                        pos->setHeight(48.0f);
                    }
                }
                // else: output full, craft stalls — isCrafting stays true, animation continues
            }
        }

    }
}

void CraftingSystem::pullFromBelts(MachineData& machine, const Grid& grid, size_t buildingLayer)
{
    const BuildingDef* def = gridSystem->getRegistry()->findByTileId(machine.machineType);
    int w = def ? def->gridW : 1;
    int h = def ? def->gridH : 1;

    for (int dy = 0; dy < h; ++dy)
    {
        for (int dx = 0; dx < w; ++dx)
        {
            int mx = machine.ownerX + dx;
            int my = machine.ownerY + dy;

            for (int dir = 0; dir < 4; ++dir)
            {
                int nx = mx + DIR_DX[dir];
                int ny = my + DIR_DY[dir];

                if (not grid.isInBounds(nx, ny)) continue;

                const auto& neighborCell = grid.getCell(buildingLayer, nx, ny);
                if (neighborCell.tileId != 4) continue;

                // Belt must be pointing INTO this machine cell
                uint8_t beltExitDir = neighborCell.direction;
                int beltTargetX = nx + DIR_DX[beltExitDir];
                int beltTargetY = ny + DIR_DY[beltExitDir];

                if (beltTargetX != mx or beltTargetY != my)
                    continue;

                ItemId item = transportSystem->peekItem(nx, ny);
                if (item == ITEM_NONE) continue;

                if (machine.inputSlots.canAccept(item, *itemRegistry))
                {
                    ItemId taken = transportSystem->tryTakeItem(nx, ny);
                    if (taken != ITEM_NONE)
                        machine.inputSlots.insert(taken, 1, *itemRegistry);
                }
            }
        }
    }
}
