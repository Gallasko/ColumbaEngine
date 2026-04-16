#pragma once

#include "Systems/basicsystems.h"
#include "2D/texture.h"

#include "gridsystem.h"
#include "transportsystem.h"
#include "inventory.h"

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
};

class MinerSystem : public System<Listener<TickEvent>,
                                   Listener<BuildingPlacedEvent>,
                                   Listener<BuildingRemovedEvent>>
{
public:
    static constexpr uint16_t MINER_TILE_ID = 7;
    static constexpr size_t MINE_TICK_MS = 250;
    static constexpr size_t MINE_TIME_MS = 2000;
    static constexpr size_t ANIM_FRAME_DURATION_MS = 200;
    static constexpr size_t NUM_ANIM_FRAMES = 4;
    static constexpr ItemId PRODUCED_ITEM = 1; // Iron Ore

    MinerSystem(GridSystem* gridSystem, TransportSystem* transportSystem, ItemRegistry* itemRegistry)
        : gridSystem(gridSystem), transportSystem(transportSystem), itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Miner System"; }

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

    virtual void onEvent(const BuildingPlacedEvent& event) override
    {
        if (event.tileId == MINER_TILE_ID)
            registerMiner(event.x, event.y);
    }

    virtual void onEvent(const BuildingRemovedEvent& event) override
    {
        if (event.tileId == MINER_TILE_ID)
            unregisterMiner(event.x, event.y);
    }

    void execute() override
    {
        // Advance animation for all mining miners
        if (animAccumulator >= ANIM_FRAME_DURATION_MS)
        {
            animAccumulator -= ANIM_FRAME_DURATION_MS;

            for (auto& [key, miner] : miners)
            {
                if (not miner.isMining)
                    continue;

                miner.animFrame = (miner.animFrame + 1) % NUM_ANIM_FRAMES;

                auto ent = ecsRef->getEntity(miner.entityId);
                if (ent and ent->has<Texture2DComponent>())
                {
                    ent->get<Texture2DComponent>()->setTexture(
                        "Miner_Machine_1_Mining." + std::to_string(miner.animFrame));
                }
            }
        }

        // Mine tick
        while (tickAccumulator >= MINE_TICK_MS)
        {
            tickAccumulator -= MINE_TICK_MS;
            mineTick();
        }
    }

private:
    void registerMiner(int x, int y)
    {
        size_t buildingLayer = gridSystem->getBuildingLayer();
        const auto& cell = gridSystem->getGrid().getCell(buildingLayer, x, y);

        MinerData data;
        data.ownerX = x;
        data.ownerY = y;
        data.entityId = cell.entityId;
        data.outputSlots = Inventory(1);

        miners[machineKey(x, y)] = data;
    }

    void unregisterMiner(int x, int y)
    {
        auto it = miners.find(machineKey(x, y));
        if (it != miners.end())
        {
            auto& miner = it->second;

            // Return output slot items to player
            for (const auto& slot : miner.outputSlots.slots)
            {
                if (not slot.isEmpty())
                    sendEvent(PlayerGainItemEvent{slot.id, slot.count});
            }

            miners.erase(it);
        }
    }

    void mineTick()
    {
        size_t buildingLayer = gridSystem->getBuildingLayer();
        const auto& grid = gridSystem->getGrid();

        for (auto& [key, miner] : miners)
        {
            bool canMine = miner.outputSlots.canAccept(PRODUCED_ITEM, *itemRegistry);

            if (canMine)
            {
                if (not miner.isMining)
                {
                    miner.isMining = true;
                    miner.animFrame = 0;
                    miner.mineProgress = 0;
                }

                miner.mineProgress += MINE_TICK_MS;

                if (miner.mineProgress >= MINE_TIME_MS)
                {
                    miner.outputSlots.insert(PRODUCED_ITEM, 1, *itemRegistry);
                    miner.mineProgress = 0;
                }
            }
            else
            {
                if (miner.isMining)
                {
                    miner.isMining = false;
                    // Switch back to idle texture
                    auto ent = ecsRef->getEntity(miner.entityId);
                    if (ent and ent->has<Texture2DComponent>())
                        ent->get<Texture2DComponent>()->setTexture("Miner_Machine_1.0");
                }
            }

        }
    }

    GridSystem* gridSystem = nullptr;
    TransportSystem* transportSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;

    std::unordered_map<uint32_t, MinerData> miners;
    size_t tickAccumulator = 0;
    size_t animAccumulator = 0;
};
