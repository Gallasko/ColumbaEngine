#include "craftingsystem.h"

#include "playerinventory.h"
#include "worldfacts.h"
#include "2D/texture.h"

namespace
{
    // Furnace renders as two entities (base + chimney overflow). Swapping textures
    // for both keeps the split assets in sync with the unified 32x64 sprite.
    void setFurnaceTextures(EntitySystem* ecs, GridSystem* gridSystem,
                            uint64_t baseId, bool active, size_t frame)
    {
        auto baseEnt = ecs->getEntity(baseId);
        if (baseEnt and baseEnt->has<Texture2DComponent>())
        {
            std::string tex = active
                ? "Stone_Furnace_Active_base." + std::to_string(frame)
                : "Stone_Furnace_base.0";
            baseEnt->get<Texture2DComponent>()->setTexture(tex);
        }

        uint64_t ovId = gridSystem ? gridSystem->getOverflowEntityFor(baseId) : 0;
        if (ovId == 0)
            return;
        auto ovEnt = ecs->getEntity(ovId);
        if (ovEnt and ovEnt->has<Texture2DComponent>())
        {
            std::string tex = active
                ? "Stone_Furnace_Active_overflow." + std::to_string(frame)
                : "Stone_Furnace_overflow.0";
            ovEnt->get<Texture2DComponent>()->setTexture(tex);
        }
    }
}

void CraftingSystem::save(Archive& archive)
{
    serialize(archive, "machines", machines);
    LOG_INFO("CraftingSystem", "saved " << machines.size() << " machines");
}

void CraftingSystem::load(const UnserializedObject& serializedString)
{
    defaultDeserialize(serializedString, "machines", machines);
    LOG_INFO("CraftingSystem", "loaded " << machines.size() << " machines");

    // Restore runtime-only state not persisted to disk
    size_t buildingLayer = gridSystem->getBuildingLayer();
    for (auto& [key, machine] : machines)
    {
        const auto& cell = gridSystem->getGrid().getCell(buildingLayer, machine.ownerX, machine.ownerY);
        machine.entityId = cell.entityId;
        machine.animFrame = 0;
        machine.isCrafting = (machine.currentRecipe != nullptr);

        if (machine.machineName == "Furnace" and machine.isCrafting)
            setFurnaceTextures(ecsRef, gridSystem, machine.entityId, true, 0);
    }
}

void CraftingSystem::onEvent(const BuildingPlacedEvent& event)
{
    LOG_INFO("CraftingSystem", "BuildingPlacedEvent " << event.tileName
            << " at (" << event.x << "," << event.y << ")");
    if (event.tileName == "Furnace" or event.tileName == "Assembler")
        registerMachine(event.x, event.y, event.tileName);
}

void CraftingSystem::onEvent(const BuildingRemovedEvent& event)
{
    LOG_INFO("CraftingSystem", "BuildingRemovedEvent " << event.tileName
            << " at (" << event.x << "," << event.y << ")");
    if (event.tileName == "Furnace" or event.tileName == "Assembler")
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

            size_t numFrames = (machine.machineName == "Furnace")
                ? FURNACE_ANIM_FRAMES
                : ASSEMBLER_ANIM_FRAMES;

            machine.animFrame = (machine.animFrame + 1) % numFrames;

            if (machine.machineName == "Furnace")
            {
                setFurnaceTextures(ecsRef, gridSystem, machine.entityId, true, machine.animFrame);
            }
            else
            {
                auto ent = ecsRef->getEntity(machine.entityId);
                if (ent and ent->has<Texture2DComponent>())
                    ent->get<Texture2DComponent>()->setTexture(
                        "Assembler_Machine_1_Running." + std::to_string(machine.animFrame));
            }
        }
    }

    while (tickAccumulator >= CRAFT_TICK_MS)
    {
        tickAccumulator -= CRAFT_TICK_MS;
        craftTick();
    }
}

void CraftingSystem::registerMachine(int x, int y, const std::string& tileName)
{
    MachineData data;
    data.ownerX = x;
    data.ownerY = y;
    data.machineName = tileName;

    if (tileName == "Furnace") // Furnace: 1 input, 1 output
    {
        data.inputSlots = Inventory(1);
        data.outputSlots = Inventory(1);
    }
    else if (tileName == "Assembler") // Assembler: 2 inputs, 1 output
    {
        data.inputSlots = Inventory(2);
        data.outputSlots = Inventory(1);
    }

    // Capture entity spawned by GridSystem for texture swapping
    size_t buildingLayer = gridSystem->getBuildingLayer();
    const auto& cell = gridSystem->getGrid().getCell(buildingLayer, x, y);
    data.entityId = cell.entityId;

    LOG_INFO("CraftingSystem", "registered machine " << tileName << " at (" << x << "," << y
            << ") entityId=" << data.entityId);

    machines[machineKey(x, y)] = data;
}

void CraftingSystem::unregisterMachine(int x, int y)
{
    auto it = machines.find(machineKey(x, y));
    if (it != machines.end())
    {
        auto& machine = it->second;
        LOG_INFO("CraftingSystem", "unregistering machine " << machine.machineName
                << " at (" << x << "," << y << ") — refunding inputs/outputs");

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
    else
    {
        LOG_INFO("CraftingSystem", "unregisterMachine at (" << x << "," << y << ") — not found");
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
            const auto* facts = worldFacts ? &worldFacts->factMap : nullptr;

            const Recipe* matched = nullptr;
            if (machine.lockedRecipe)
            {
                // Check unlock conditions for the locked recipe
                bool unlocked = true;
                if (facts and not machine.lockedRecipe->unlockConditions.empty())
                {
                    for (const auto& cond : machine.lockedRecipe->unlockConditions)
                        if (not cond.check(*facts))
                            { unlocked = false; break; }
                }

                if (unlocked)
                {
                    bool ok = true;
                    for (const auto& input : machine.lockedRecipe->inputs)
                        if (not machine.inputSlots.hasAtLeast(input.id, input.count))
                            { ok = false; break; }
                    if (ok) matched = machine.lockedRecipe;
                }
            }
            else
            {
                matched = recipeRegistry->findMatchingRecipe(
                    machine.machineName, machine.inputSlots, facts);
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

                    if (machine.machineName == "Furnace")
                    {
                        setFurnaceTextures(ecsRef, gridSystem, machine.entityId, true, 0);
                    }
                    else
                    {
                        auto ent = ecsRef->getEntity(machine.entityId);
                        if (ent and ent->has<Texture2DComponent>())
                            ent->get<Texture2DComponent>()->setTexture("Assembler_Machine_1_Running.0");
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

                    if (machine.machineName == "Furnace")
                    {
                        setFurnaceTextures(ecsRef, gridSystem, machine.entityId, false, 0);
                    }
                    else
                    {
                        auto ent = ecsRef->getEntity(machine.entityId);
                        if (ent and ent->has<Texture2DComponent>())
                            ent->get<Texture2DComponent>()->setTexture("Assembler_Machine_1.0");
                    }
                }
                // else: output full, craft stalls — isCrafting stays true, animation continues
            }
        }

    }
}

void CraftingSystem::pullFromBelts(MachineData& machine, const Grid& grid, size_t buildingLayer)
{
    const BuildingDef* def = gridSystem->getRegistry()->findByName(machine.machineName);
    int w = def ? def->getFootprintW() : 1;
    int h = def ? def->getFootprintH() : 1;

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
                if (neighborCell.tileName != "Conveyor") continue;

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
