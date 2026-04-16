#include "craftingsystem.h"

#include "playerinventory.h"

#include <cstdio>

void CraftingSystem::save(Archive& archive)
{
    serialize(archive, "machines", machines);
    printf("CraftingSystem: saved %zu machines\n", machines.size());
}

void CraftingSystem::load(const UnserializedObject& serializedString)
{
    defaultDeserialize(serializedString, "machines", pendingMachines);
    printf("CraftingSystem: loaded %zu machines\n", pendingMachines.size());
}

void CraftingSystem::init()
{
    if (not pendingMachines.empty())
    {
        machines = std::move(pendingMachines);
        pendingMachines.clear();
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
            machine.currentRecipe = recipeRegistry->findMatchingRecipe(
                machine.machineType, machine.inputSlots);

            if (machine.currentRecipe)
            {
                // Consume inputs immediately
                for (const auto& input : machine.currentRecipe->inputs)
                    machine.inputSlots.remove(input.id, input.count);
                machine.craftProgress = 0;
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
                        machine.outputSlots.insert(output.id, output.count, *itemRegistry);
                    machine.currentRecipe = nullptr;
                    machine.craftProgress = 0;
                }
                // else: output full, craft stalls
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
