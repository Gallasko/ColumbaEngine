#include "minersystem.h"
#include "saveserialization.h"

#include "2D/texture.h"
#include "playerinventory.h"

void MinerSystem::save(Archive& archive)
{
    serialize(archive, "miners", miners);
    LOG_INFO("MinerSystem", "saved " << miners.size() << " miners");
}

void MinerSystem::load(const UnserializedObject& serializedString)
{
    defaultDeserialize(serializedString, "miners", miners);
    LOG_INFO("MinerSystem", "loaded " << miners.size() << " miners");

    auto* gridSystem = ecsRef->getSystem<GridSystem>();
    size_t buildingLayer = gridSystem->getBuildingLayer();

    for (auto& [key, miner] : miners)
    {
        const auto& cell = gridSystem->getGrid().getCell(buildingLayer, miner.ownerX, miner.ownerY);
        miner.entityId = cell.entityId;
        miner.producedItem = resolveOreUnder(miner.ownerX, miner.ownerY);
        miner.animFrame = 0;
        miner.animElapsed = 0;

        LOG_INFO("MinerSystem", "restored miner at (" << miner.ownerX << ", " << miner.ownerY
                << "), producedItem=" << miner.producedItem << ", isMining=" << miner.isMining);
    }
}

void MinerSystem::onEvent(const BuildingPlacedEvent& event)
{
    LOG_INFO("MinerSystem", "BuildingPlacedEvent " << event.tileName << " at ("
            << event.x << "," << event.y << ")");
    if (event.tileName == "Miner")
        registerMiner(event.x, event.y);
}

void MinerSystem::onEvent(const BuildingRemovedEvent& event)
{
    LOG_INFO("MinerSystem", "BuildingRemovedEvent " << event.tileName << " at ("
            << event.x << "," << event.y << ")");
    if (event.tileName == "Miner")
        unregisterMiner(event.x, event.y);
}

void MinerSystem::execute()
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

ItemId MinerSystem::resolveOreUnder(int ownerX, int ownerY) const
{
    // Miner footprint is 2 wide x 3 tall (see buildingregistry.h miner def).
    constexpr int W = 2;
    constexpr int H = 3;

    // Tally each ore terrain type and pick the majority. Mapping from
    // TerrainType to ItemId is centralised in terrain.h::oreToItem().
    TerrainType candidates[4] = {
        TerrainType::OreIron,
        TerrainType::OreCopper,
        TerrainType::OreCoal,
        TerrainType::OreStone
    };
    int counts[4] = {0, 0, 0, 0};

    auto* gridSystem = ecsRef->getSystem<GridSystem>();
    for (int dy = 0; dy < H; ++dy)
    {
        for (int dx = 0; dx < W; ++dx)
        {
            TerrainType t = gridSystem->getTerrainAt(ownerX + dx, ownerY + dy);
            for (int i = 0; i < 4; ++i)
            {
                if (t == candidates[i])
                {
                    counts[i]++;
                    break;
                }
            }
        }
    }

    int best = -1;
    int bestIdx = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (counts[i] > best)
        {
            best = counts[i];
            bestIdx = i;
        }
    }

    if (best <= 0) return ITEM_NONE;

    return oreToItem(candidates[bestIdx]);
}

void MinerSystem::registerMiner(int x, int y)
{
    auto* gridSystem = ecsRef->getSystem<GridSystem>();
    size_t buildingLayer = gridSystem->getBuildingLayer();
    const auto& cell = gridSystem->getGrid().getCell(buildingLayer, x, y);

    MinerData data;
    data.ownerX = x;
    data.ownerY = y;
    data.entityId = cell.entityId;
    data.outputSlots = Inventory(1);
    data.producedItem = resolveOreUnder(x, y);

    if (data.producedItem == ITEM_NONE)
        LOG_INFO("MinerSystem", "miner at (" << x << ", " << y << "): placed off ore, idle.");
    else
        LOG_INFO("MinerSystem", "miner at (" << x << ", " << y << "): producing item " << data.producedItem << ".");

    miners[machineKey(x, y)] = data;
}

void MinerSystem::unregisterMiner(int x, int y)
{
    auto it = miners.find(machineKey(x, y));
    if (it != miners.end())
    {
        auto& miner = it->second;
        LOG_INFO("MinerSystem", "unregistering miner at (" << x << "," << y
                << "), returning output slots to player");

        // Return output slot items to player
        for (const auto& slot : miner.outputSlots.slots)
        {
            if (not slot.isEmpty())
                sendEvent(PlayerGainItemEvent{slot.id, slot.count});
        }

        miners.erase(it);
    }
    else
    {
        LOG_INFO("MinerSystem", "unregisterMiner at (" << x << "," << y << ") — not found");
    }
}

void MinerSystem::mineTick()
{
    for (auto& [key, miner] : miners)
    {
        // Miners placed on non-ore terrain never produce.
        if (miner.producedItem == ITEM_NONE)
        {
            if (miner.isMining)
            {
                miner.isMining = false;
                auto ent = ecsRef->getEntity(miner.entityId);
                if (ent and ent->has<Texture2DComponent>())
                    ent->get<Texture2DComponent>()->setTexture("Miner_Machine_1.0");
            }
            continue;
        }

        bool canMine = miner.outputSlots.canAccept(miner.producedItem, *itemRegistry);

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
                miner.outputSlots.insert(miner.producedItem, 1, *itemRegistry);
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
