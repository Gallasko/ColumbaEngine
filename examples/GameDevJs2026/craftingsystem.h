#pragma once

#include "Systems/basicsystems.h"

#include "gridsystem.h"
#include "transportsystem.h"
#include "reciperegistry.h"

using namespace pg;

struct MachineData
{
    int ownerX, ownerY;
    uint16_t machineType;       // tileId (5=Furnace, 6=Assembler)

    Inventory inputSlots;
    Inventory outputSlots;

    const Recipe* currentRecipe = nullptr;
    size_t craftProgress = 0;   // Milliseconds elapsed on current craft
};

inline uint32_t machineKey(int x, int y)
{
    return (static_cast<uint32_t>(y) << 16) | static_cast<uint32_t>(x);
}

class CraftingSystem : public System<InitSys, Listener<TickEvent>,
                                     Listener<BuildingPlacedEvent>,
                                     Listener<BuildingRemovedEvent>>
{
public:
    static constexpr size_t CRAFT_TICK_MS = 250;

    CraftingSystem(GridSystem* gridSystem, TransportSystem* transportSystem,
                   ItemRegistry* itemRegistry, RecipeRegistry* recipeRegistry)
        : gridSystem(gridSystem), transportSystem(transportSystem),
          itemRegistry(itemRegistry), recipeRegistry(recipeRegistry) {}

    virtual std::string getSystemName() const override { return "Crafting System"; }

    void init() override {}

    virtual void onEvent(const TickEvent& event) override
    {
        tickAccumulator += static_cast<size_t>(event.tick);
    }

    virtual void onEvent(const BuildingPlacedEvent& event) override
    {
        if (event.tileId == 5 or event.tileId == 6)
            registerMachine(event.x, event.y, event.tileId);
    }

    virtual void onEvent(const BuildingRemovedEvent& event) override
    {
        if (event.tileId == 5 or event.tileId == 6)
            unregisterMachine(event.x, event.y);
    }

    void execute() override
    {
        while (tickAccumulator >= CRAFT_TICK_MS)
        {
            tickAccumulator -= CRAFT_TICK_MS;
            craftTick();
        }
    }

    MachineData* getMachine(int x, int y)
    {
        auto it = machines.find(machineKey(x, y));
        return it != machines.end() ? &it->second : nullptr;
    }

private:
    void registerMachine(int x, int y, uint16_t tileId)
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

    void unregisterMachine(int x, int y)
    {
        machines.erase(machineKey(x, y));
    }

    void craftTick()
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

            // Phase 3: push output items to adjacent output belts
            pushToBelts(machine, grid, buildingLayer);
        }
    }

    // Pull items from belts that point INTO the machine
    void pullFromBelts(MachineData& machine, const Grid& grid, size_t buildingLayer)
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

    // Push output items onto belts that source FROM the machine
    void pushToBelts(MachineData& machine, const Grid& grid, size_t buildingLayer)
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

                    // Belt must source FROM the machine:
                    // the belt's enter side should face towards this machine cell
                    uint8_t beltEnterSide = (neighborCell.enterDirection + 2) % 4;
                    int beltSourceX = nx + DIR_DX[beltEnterSide];
                    int beltSourceY = ny + DIR_DY[beltEnterSide];

                    if (beltSourceX != mx or beltSourceY != my)
                        continue;

                    for (auto& slot : machine.outputSlots.slots)
                    {
                        if (slot.isEmpty()) continue;

                        if (transportSystem->tryPlaceItem(nx, ny, slot.id))
                        {
                            slot.count -= 1;
                            if (slot.count == 0) slot.clear();
                            return; // One item per tick per machine output
                        }
                    }
                }
            }
        }
    }

    GridSystem* gridSystem = nullptr;
    TransportSystem* transportSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    RecipeRegistry* recipeRegistry = nullptr;

    std::unordered_map<uint32_t, MachineData> machines;
    size_t tickAccumulator = 0;
};
