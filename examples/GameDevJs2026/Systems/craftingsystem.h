#pragma once

#include "Systems/basicsystems.h"

#include "gridsystem.h"
#include "transportsystem.h"
#include "reciperegistry.h"
#include "machinekey.h"
#include "saveserialization.h"
#include "worldfacts.h"

using namespace pg;

struct MachineData
{
    int ownerX, ownerY;
    std::string machineName;    // "Furnace", "Assembler"

    Inventory inputSlots;
    Inventory outputSlots;

    const Recipe* currentRecipe = nullptr;
    const Recipe* lockedRecipe  = nullptr; // If set, only this recipe is ever tried
    size_t craftProgress = 0;   // Milliseconds elapsed on current craft

    // Animation state (runtime-only; restored from grid on load)
    uint64_t entityId = 0;
    size_t animFrame = 0;
    bool isCrafting = false;
};

class CraftingSystem : public System<Listener<TickEvent>,
                                     Listener<BuildingPlacedEvent>,
                                     Listener<BuildingRemovedEvent>,
                                     SaveSys>
{
public:
    static constexpr size_t CRAFT_TICK_MS          = 250;
    static constexpr size_t ANIM_FRAME_DURATION_MS = 200;
    static constexpr size_t FURNACE_ANIM_FRAMES    = 3;
    static constexpr size_t ASSEMBLER_ANIM_FRAMES  = 4;

    CraftingSystem(ItemRegistry* itemRegistry, RecipeRegistry* recipeRegistry)
        : itemRegistry(itemRegistry), recipeRegistry(recipeRegistry) {}

    virtual std::string getSystemName() const override { return "Crafting System"; }

    // SaveSys
    virtual void save(Archive& archive) override;
    virtual void load(const UnserializedObject& serializedString) override;

    virtual void onEvent(const TickEvent& event) override
    {
        tickAccumulator += static_cast<size_t>(event.tick);
        animAccumulator += static_cast<size_t>(event.tick);
    }

    virtual void onEvent(const BuildingPlacedEvent& event) override;

    virtual void onEvent(const BuildingRemovedEvent& event) override;

    void execute() override;

    MachineData* getMachine(int x, int y)
    {
        auto it = machines.find(machineKey(x, y));
        return it != machines.end() ? &it->second : nullptr;
    }

private:
    void registerMachine(int x, int y, const std::string& tileName);
    void unregisterMachine(int x, int y);
    void craftTick();

    // Pull items from belts that point INTO the machine
    void pullFromBelts(MachineData& machine, const Grid& grid, size_t buildingLayer);

    ItemRegistry* itemRegistry = nullptr;
    RecipeRegistry* recipeRegistry = nullptr;

    std::unordered_map<uint32_t, MachineData> machines;
    size_t tickAccumulator = 0;
    size_t animAccumulator = 0;

};
