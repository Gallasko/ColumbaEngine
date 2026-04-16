#pragma once

#include "Systems/basicsystems.h"

#include "gridsystem.h"
#include "transportsystem.h"
#include "inventory.h"
#include "terrain.h"
#include "machinekey.h"
#include "saveserialization.h"

using namespace pg;

struct MinerData
{
    int ownerX, ownerY;
    uint64_t entityId = 0;

    Inventory outputSlots{1};

    size_t mineProgress = 0;    // ms elapsed on current mine cycle
    size_t animFrame = 0;       // current animation frame (0-3)
    size_t animElapsed = 0;     // ms since last frame change
    bool isMining = false;

    // Ore type produced, resolved from the terrain under the miner at placement.
    // ITEM_NONE means the miner is sitting on non-ore terrain and cannot produce.
    ItemId producedItem = ITEM_NONE;
};

class MinerSystem : public System<InitSys, Listener<TickEvent>,
                                   Listener<BuildingPlacedEvent>,
                                   Listener<BuildingRemovedEvent>,
                                   SaveSys>
{
public:
    static constexpr uint16_t MINER_TILE_ID = 7;
    static constexpr size_t MINE_TICK_MS = 250;
    static constexpr size_t MINE_TIME_MS = 2000;
    static constexpr size_t ANIM_FRAME_DURATION_MS = 200;
    static constexpr size_t NUM_ANIM_FRAMES = 4;

    MinerSystem(GridSystem* gridSystem, TransportSystem* transportSystem, ItemRegistry* itemRegistry)
        : gridSystem(gridSystem), transportSystem(transportSystem), itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Miner System"; }

    void init() override;

    // SaveSys
    virtual void save(Archive& archive) override;
    virtual void load(const UnserializedObject& serializedString) override;

    MinerData* getMiner(int x, int y)
    {
        auto it = miners.find(machineKey(x, y));
        return it != miners.end() ? &it->second : nullptr;
    }

    virtual void onEvent(const TickEvent& event) override
    {
        tickAccumulator += static_cast<size_t>(event.tick);
        animAccumulator += static_cast<size_t>(event.tick);
    }

    virtual void onEvent(const BuildingPlacedEvent& event) override;

    virtual void onEvent(const BuildingRemovedEvent& event) override;

    void execute() override;

private:
    // Pick the ore the miner will produce by scanning its 2x3 footprint and taking
    // the majority ore type. Returns ITEM_NONE if no cells under the miner are ore.
    ItemId resolveOreUnder(int ownerX, int ownerY) const;

    void registerMiner(int x, int y);
    void unregisterMiner(int x, int y);
    void mineTick();

    GridSystem* gridSystem = nullptr;
    TransportSystem* transportSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;

    std::unordered_map<uint32_t, MinerData> miners;
    std::unordered_map<uint32_t, MinerData> pendingMiners;
    size_t tickAccumulator = 0;
    size_t animAccumulator = 0;
};
