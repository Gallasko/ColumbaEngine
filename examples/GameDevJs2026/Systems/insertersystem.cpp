#include "insertersystem.h"
#include "saveserialization.h"

#include "2D/texture.h"
#include "playerinventory.h"

#include <cmath>

void InserterSystem::save(Archive& archive)
{
    serialize(archive, "inserters", inserters);
    LOG_INFO("InserterSystem", "saved " << inserters.size() << " inserters");
}

void InserterSystem::load(const UnserializedObject& serializedString)
{
    defaultDeserialize(serializedString, "inserters", inserters);
    LOG_INFO("InserterSystem", "loaded " << inserters.size() << " inserters");

    auto* gridSystem = ecsRef->getSystem<GridSystem>();
    size_t buildingLayer = gridSystem->getBuildingLayer();

    for (auto& [key, ins] : inserters)
    {
        const auto& cell = gridSystem->getGrid().getCell(buildingLayer, ins.x, ins.y);
        ins.entityId = cell.entityId;

        // Do the lazy init immediately (resize arm to 48x48, set texture)
        auto ent = ecsRef->getEntity(ins.entityId);
        if (ent)
        {
            auto pos = ent->get<PositionComponent>();
            float armSize = static_cast<float>(Grid::TILE_SIZE) * 3.0f;
            float cellOffset = static_cast<float>(Grid::TILE_SIZE);
            auto [wx, wy] = gridSystem->getGrid().gridToWorld(ins.x, ins.y);
            pos->setX(wx - cellOffset);
            pos->setY(wy - cellOffset);
            pos->setWidth(armSize);
            pos->setHeight(armSize);
            pos->setZ(pos->getZ() + 1.5f);

            ins.initialized = true;
            updateArmTexture(ins);
        }

        if (ins.heldItem != ITEM_NONE)
            createHeldItemVisual(ins);

        LOG_INFO("InserterSystem", "restored inserter at (" << ins.x << ", " << ins.y
                << "), dir=" << ins.direction << ", state=" << static_cast<unsigned>(ins.state)
                << ", heldItem=" << ins.heldItem);
    }
}

void InserterSystem::onEvent(const BuildingPlacedEvent& event)
{
    LOG_INFO("InserterSystem", "BuildingPlacedEvent " << event.tileName
            << " at (" << event.x << "," << event.y << ")");
    if (event.tileName == "Inserter")
        registerInserter(event.x, event.y);
}

void InserterSystem::onEvent(const BuildingRemovedEvent& event)
{
    LOG_INFO("InserterSystem", "BuildingRemovedEvent " << event.tileName
            << " at (" << event.x << "," << event.y << ")");
    if (event.tileName == "Inserter")
        unregisterInserter(event.x, event.y);
}

void InserterSystem::execute()
{
    while (tickAccumulator >= ANIM_FRAME_DURATION_MS)
    {
        tickAccumulator -= ANIM_FRAME_DURATION_MS;
        inserterTick();
    }
}

size_t InserterSystem::getSpriteFrame(uint8_t direction, size_t animFrame, bool returning)
{
    size_t base = PICKUP_FRAME[direction];
    if (returning)
    {
        // Reverse: from drop position back to pickup
        // Drop frame is (base + 4) % 8, step backward
        return (base + SWING_FRAMES - 1 - animFrame) % TOTAL_SPRITE_FRAMES;
    }
    else
    {
        // Forward: from pickup to drop
        return (base + animFrame) % TOTAL_SPRITE_FRAMES;
    }
}

void InserterSystem::registerInserter(int x, int y)
{
    auto* gridSystem = ecsRef->getSystem<GridSystem>();
    size_t buildingLayer = gridSystem->getBuildingLayer();
    const auto& cell = gridSystem->getGrid().getCell(buildingLayer, x, y);

    InserterData data;
    data.x = x;
    data.y = y;
    data.direction = cell.direction;
    data.entityId = cell.entityId;

    LOG_INFO("InserterSystem", "registered inserter at (" << x << "," << y
            << ") direction=" << static_cast<unsigned>(data.direction)
            << " entityId=" << data.entityId);

    inserters[machineKey(x, y)] = data;
}

void InserterSystem::unregisterInserter(int x, int y)
{
    auto it = inserters.find(machineKey(x, y));
    if (it != inserters.end())
    {
        auto& ins = it->second;
        LOG_INFO("InserterSystem", "unregistering inserter at (" << x << "," << y
                << ") heldItem=" << ins.heldItem);

        // Return held item to player
        if (ins.heldItem != ITEM_NONE)
            sendEvent(PlayerGainItemEvent{ins.heldItem, 1});

        destroyHeldItemVisual(ins);
        inserters.erase(it);
    }
    else
    {
        LOG_INFO("InserterSystem", "unregisterInserter at (" << x << "," << y << ") — not found");
    }
}

void InserterSystem::inserterTick()
{
    auto* gridSystem = ecsRef->getSystem<GridSystem>();
    for (auto& [key, ins] : inserters)
    {
        switch (ins.state)
        {
            case InserterState::Idle:
            {
                if (not ins.initialized)
                {
                    auto ent = ecsRef->getEntity(ins.entityId);
                    if (ent)
                    {
                        // Resize from 16x16 to 48x48 (3 tiles), centered on cell
                        auto pos = ent->get<PositionComponent>();
                        float armSize = static_cast<float>(Grid::TILE_SIZE) * 3.0f;
                        float cellOffset = static_cast<float>(Grid::TILE_SIZE);
                        auto [wx, wy] = gridSystem->getGrid().gridToWorld(ins.x, ins.y);
                        pos->setX(wx - cellOffset);
                        pos->setY(wy - cellOffset);
                        pos->setWidth(armSize);
                        pos->setHeight(armSize);
                        pos->setZ(pos->getZ() + 1.5f); // Above items layer (z=3.0)

                        // Set correct idle texture for this direction
                        ent->get<Texture2DComponent>()->setTexture(
                            "Robotic_Arms_1." + std::to_string(PICKUP_FRAME[ins.direction]));

                        ins.initialized = true;
                    }
                    break; // Skip pickup until initialized
                }

                if (tryPickup(ins))
                {
                    ins.state = InserterState::Swinging;
                    ins.animFrame = 0;
                    updateArmTexture(ins);
                    createHeldItemVisual(ins);
                }
                break;
            }

            case InserterState::Swinging:
            {
                if (ins.animFrame < SWING_FRAMES - 1)
                {
                    ins.animFrame++;
                    updateArmTexture(ins);
                    updateHeldItemPosition(ins);
                }
                else
                {
                    // At drop position — try to drop
                    if (tryDrop(ins))
                    {
                        destroyHeldItemVisual(ins);
                        ins.state = InserterState::Returning;
                        ins.animFrame = 0;
                        updateArmTexture(ins);
                    }
                    // else: stall at drop position until target accepts
                }
                break;
            }

            case InserterState::Returning:
            {
                if (ins.animFrame < SWING_FRAMES - 1)
                {
                    ins.animFrame++;
                    updateArmTexture(ins);
                }
                else
                {
                    ins.state = InserterState::Idle;
                    ins.animFrame = 0;
                    updateArmTexture(ins);
                }
                break;
            }
        }
    }
}

bool InserterSystem::tryPickup(InserterData& ins)
{
    int pickupX = ins.x - DIR_DX[ins.direction];
    int pickupY = ins.y - DIR_DY[ins.direction];

    auto* gridSystem = ecsRef->getSystem<GridSystem>();
    if (not gridSystem->getGrid().isInBounds(pickupX, pickupY))
        return false;

    size_t buildingLayer = gridSystem->getBuildingLayer();
    const auto& cell = gridSystem->getGrid().getCell(buildingLayer, pickupX, pickupY);

    if (cell.tileName.empty())
        return false;

    // Pick from belt
    if (cell.tileName == "Conveyor")
    {
        ItemId item = ecsRef->getSystem<TransportSystem>()->tryTakeItem(pickupX, pickupY);
        if (item != ITEM_NONE)
        {
            ins.heldItem = item;
            return true;
        }
        return false;
    }

    // Pick from miner output
    if (cell.tileName == "Miner")
    {
        int ox = cell.isOwner ? pickupX : static_cast<int>(cell.ownerX);
        int oy = cell.isOwner ? pickupY : static_cast<int>(cell.ownerY);
        MinerData* miner = ecsRef->getSystem<MinerSystem>()->getMiner(ox, oy);
        if (miner)
        {
            for (auto& slot : miner->outputSlots.slots)
            {
                if (not slot.isEmpty())
                {
                    ins.heldItem = slot.id;
                    slot.count -= 1;
                    if (slot.count == 0) slot.clear();
                    return true;
                }
            }
        }
        return false;
    }

    // Pick from machine (furnace/assembler) output
    if (cell.tileName == "Furnace" or cell.tileName == "Assembler")
    {
        int ox = cell.isOwner ? pickupX : static_cast<int>(cell.ownerX);
        int oy = cell.isOwner ? pickupY : static_cast<int>(cell.ownerY);
        MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(ox, oy);
        if (machine)
        {
            for (auto& slot : machine->outputSlots.slots)
            {
                if (not slot.isEmpty())
                {
                    ins.heldItem = slot.id;
                    slot.count -= 1;
                    if (slot.count == 0) slot.clear();
                    return true;
                }
            }
        }
        return false;
    }

    // Pick from storage
    if (cell.tileName == "Storage")
    {
        StorageData* storage = ecsRef->getSystem<StorageSystem>()->getStorage(pickupX, pickupY);
        if (storage)
        {
            for (auto& slot : storage->inventory.slots)
            {
                if (not slot.isEmpty())
                {
                    ins.heldItem = slot.id;
                    slot.count -= 1;
                    if (slot.count == 0) slot.clear();
                    return true;
                }
            }
        }
        return false;
    }

    // Pick from depot
    if (cell.tileName == "Depot")
    {
        int ox = cell.isOwner ? pickupX : static_cast<int>(cell.ownerX);
        int oy = cell.isOwner ? pickupY : static_cast<int>(cell.ownerY);
        DepotData* depot = ecsRef->getSystem<DepotSystem>()->getDepot(ox, oy);
        if (depot)
        {
            for (auto& slot : depot->inventory.slots)
            {
                if (not slot.isEmpty())
                {
                    ins.heldItem = slot.id;
                    slot.count -= 1;
                    if (slot.count == 0) slot.clear();
                    return true;
                }
            }
        }
        return false;
    }

    return false;
}

bool InserterSystem::tryDrop(InserterData& ins)
{
    int dropX = ins.x + DIR_DX[ins.direction];
    int dropY = ins.y + DIR_DY[ins.direction];

    auto* gridSystem = ecsRef->getSystem<GridSystem>();
    if (not gridSystem->getGrid().isInBounds(dropX, dropY))
        return false;

    size_t buildingLayer = gridSystem->getBuildingLayer();
    const auto& cell = gridSystem->getGrid().getCell(buildingLayer, dropX, dropY);

    if (cell.tileName.empty())
        return false;

    // Drop onto belt
    if (cell.tileName == "Conveyor")
    {
        if (ecsRef->getSystem<TransportSystem>()->tryPlaceItem(dropX, dropY, ins.heldItem))
        {
            ins.heldItem = ITEM_NONE;
            return true;
        }
        return false;
    }

    // Drop into machine input
    if (cell.tileName == "Furnace" or cell.tileName == "Assembler")
    {
        int ox = cell.isOwner ? dropX : static_cast<int>(cell.ownerX);
        int oy = cell.isOwner ? dropY : static_cast<int>(cell.ownerY);
        MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(ox, oy);
        if (machine and machine->inputSlots.canAccept(ins.heldItem, *itemRegistry))
        {
            machine->inputSlots.insert(ins.heldItem, 1, *itemRegistry);
            ins.heldItem = ITEM_NONE;
            return true;
        }
        return false;
    }

    // Drop into storage
    if (cell.tileName == "Storage")
    {
        StorageData* storage = ecsRef->getSystem<StorageSystem>()->getStorage(dropX, dropY);
        if (storage and storage->inventory.canAccept(ins.heldItem, *itemRegistry))
        {
            storage->inventory.insert(ins.heldItem, 1, *itemRegistry);
            ins.heldItem = ITEM_NONE;
            return true;
        }
        return false;
    }

    // Drop into depot
    if (cell.tileName == "Depot")
    {
        int ox = cell.isOwner ? dropX : static_cast<int>(cell.ownerX);
        int oy = cell.isOwner ? dropY : static_cast<int>(cell.ownerY);
        DepotData* depot = ecsRef->getSystem<DepotSystem>()->getDepot(ox, oy);
        if (depot and depot->inventory.canAccept(ins.heldItem, *itemRegistry))
        {
            depot->inventory.insert(ins.heldItem, 1, *itemRegistry);
            ins.heldItem = ITEM_NONE;
            return true;
        }
        return false;
    }

    return false;
}

void InserterSystem::updateArmTexture(InserterData& ins)
{
    bool returning = (ins.state == InserterState::Returning);
    size_t spriteFrame = getSpriteFrame(ins.direction, ins.animFrame, returning);

    auto ent = ecsRef->getEntity(ins.entityId);
    if (ent and ent->has<Texture2DComponent>())
    {
        ent->get<Texture2DComponent>()->setTexture(
            "Robotic_Arms_1." + std::to_string(spriteFrame));
    }
}

void InserterSystem::createHeldItemVisual(InserterData& ins)
{
    if (ins.heldItem == ITEM_NONE)
        return;

    const auto& itemDef = itemRegistry->get(ins.heldItem);
    auto* gridSystem = ecsRef->getSystem<GridSystem>();
    float z = gridSystem->getGrid().getLayer(gridSystem->getItemLayer()).zIndex + 0.1f;
    float itemSize = static_cast<float>(Grid::TILE_SIZE) * 0.5f;

    if (not itemDef.textureName.empty())
    {
        auto tex = make2DTexture(ecsRef, itemSize, itemSize, itemDef.textureName);
        auto pos = tex.get<PositionComponent>();
        pos->setZ(z);
        tex.get<ViewportComponent>()->setViewport(GAME_VIEWPORT);
        ins.heldItemEntityId = tex.entity->id;
    }

    updateHeldItemPosition(ins);
}

void InserterSystem::updateHeldItemPosition(InserterData& ins)
{
    if (ins.heldItemEntityId == 0)
        return;

    auto ent = ecsRef->getEntity(ins.heldItemEntityId);
    if (not ent)
        return;

    float t = static_cast<float>(ins.animFrame) / static_cast<float>(SWING_FRAMES - 1);
    float halfTile = static_cast<float>(Grid::TILE_SIZE) * 0.5f;

    // Arc center = inserter cell center
    auto* gridSystem = ecsRef->getSystem<GridSystem>();
    auto [iwx, iwy] = gridSystem->getGrid().gridToWorld(ins.x, ins.y);
    float cx = iwx + halfTile;
    float cy = iwy + halfTile;

    // Pickup cell center
    int pickupX = ins.x - DIR_DX[ins.direction];
    int pickupY = ins.y - DIR_DY[ins.direction];
    auto [pw, ph] = gridSystem->getGrid().gridToWorld(pickupX, pickupY);
    float pcx = pw + halfTile;
    float pcy = ph + halfTile;

    // Start angle from center to pickup position
    float startAngle = std::atan2(pcy - cy, pcx - cx);

    // Sweep π radians (semicircle) clockwise matching arm animation
    float sweep = static_cast<float>(M_PI);

    float angle = startAngle + t * sweep;
    float radius = static_cast<float>(Grid::TILE_SIZE);

    float itemSize = halfTile; // 0.5 * TILE_SIZE
    float itemOffset = itemSize * 0.5f;

    auto pos = ent->get<PositionComponent>();
    pos->setX(cx + radius * std::cos(angle) - itemOffset);
    pos->setY(cy + radius * std::sin(angle) - itemOffset);
}

void InserterSystem::destroyHeldItemVisual(InserterData& ins)
{
    if (ins.heldItemEntityId != 0)
    {
        ecsRef->removeEntity(ins.heldItemEntityId);
        ins.heldItemEntityId = 0;
    }
}
