#pragma once

#include "Systems/basicsystems.h"
#include "2D/texture.h"

#include "gridsystem.h"
#include "transportsystem.h"
#include "minersystem.h"
#include "craftingsystem.h"

using namespace pg;

enum class InserterState : uint8_t
{
    Idle,       // At pickup position, waiting for item to grab
    Swinging,   // Carrying item from pickup to drop position
    Returning   // Empty, swinging back from drop to pickup position
};

struct InserterData
{
    int x, y;
    uint8_t direction;          // 0=R, 1=D, 2=L, 3=U
    uint64_t entityId = 0;     // Arm sprite entity

    InserterState state = InserterState::Idle;
    ItemId heldItem = ITEM_NONE;
    size_t animFrame = 0;       // Current frame in the 5-frame swing (0-4)
    bool initialized = false;   // Deferred init (entity not available at register time)

    uint64_t heldItemEntityId = 0; // Visual entity for item being carried
};

class InserterSystem : public System<Listener<TickEvent>,
                                      Listener<BuildingPlacedEvent>,
                                      Listener<BuildingRemovedEvent>>
{
public:
    static constexpr uint16_t INSERTER_TILE_ID = 8;
    static constexpr size_t ANIM_FRAME_DURATION_MS = 100;
    static constexpr size_t SWING_FRAMES = 5;
    static constexpr size_t TOTAL_SPRITE_FRAMES = 8;

    // Pickup (idle) frame for each direction
    // The 8 sprite frames go clockwise: 0=left, 2=down, 4=right, 6=up
    // Pickup is behind the arm (opposite of direction)
    static constexpr size_t PICKUP_FRAME[4] = {0, 6, 4, 2}; // RIGHT, DOWN, LEFT, UP

    InserterSystem(GridSystem* gridSystem, TransportSystem* transportSystem,
                   MinerSystem* minerSystem, CraftingSystem* craftingSystem,
                   ItemRegistry* itemRegistry)
        : gridSystem(gridSystem), transportSystem(transportSystem),
          minerSystem(minerSystem), craftingSystem(craftingSystem),
          itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Inserter System"; }

    virtual void onEvent(const TickEvent& event) override
    {
        tickAccumulator += static_cast<size_t>(event.tick);
    }

    virtual void onEvent(const BuildingPlacedEvent& event) override
    {
        if (event.tileId == INSERTER_TILE_ID)
            registerInserter(event.x, event.y);
    }

    virtual void onEvent(const BuildingRemovedEvent& event) override
    {
        if (event.tileId == INSERTER_TILE_ID)
            unregisterInserter(event.x, event.y);
    }

    void execute() override
    {
        while (tickAccumulator >= ANIM_FRAME_DURATION_MS)
        {
            tickAccumulator -= ANIM_FRAME_DURATION_MS;
            inserterTick();
        }
    }

private:
    // Compute the actual sprite frame index (0-7) from direction, animation progress, and state
    static size_t getSpriteFrame(uint8_t direction, size_t animFrame, bool returning)
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

    void registerInserter(int x, int y)
    {
        size_t buildingLayer = gridSystem->getBuildingLayer();
        const auto& cell = gridSystem->getGrid().getCell(buildingLayer, x, y);

        InserterData data;
        data.x = x;
        data.y = y;
        data.direction = cell.direction;
        data.entityId = cell.entityId;

        inserters[machineKey(x, y)] = data;
    }

    void unregisterInserter(int x, int y)
    {
        auto it = inserters.find(machineKey(x, y));
        if (it != inserters.end())
        {
            auto& ins = it->second;

            // Return held item to player
            if (ins.heldItem != ITEM_NONE)
                sendEvent(PlayerGainItemEvent{ins.heldItem, 1});

            destroyHeldItemVisual(ins);
            inserters.erase(it);
        }
    }

    void inserterTick()
    {
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
                            pos->setZ(pos->getZ() + 0.5f);

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

    // --- Pickup / Drop ---

    bool tryPickup(InserterData& ins)
    {
        int pickupX = ins.x - DIR_DX[ins.direction];
        int pickupY = ins.y - DIR_DY[ins.direction];

        if (not gridSystem->getGrid().isInBounds(pickupX, pickupY))
            return false;

        size_t buildingLayer = gridSystem->getBuildingLayer();
        const auto& cell = gridSystem->getGrid().getCell(buildingLayer, pickupX, pickupY);

        if (cell.tileId == 0)
            return false;

        // Pick from belt
        if (cell.tileId == 4)
        {
            ItemId item = transportSystem->tryTakeItem(pickupX, pickupY);
            if (item != ITEM_NONE)
            {
                ins.heldItem = item;
                return true;
            }
            return false;
        }

        // Pick from miner output
        if (cell.tileId == MinerSystem::MINER_TILE_ID)
        {
            int ox = cell.isOwner ? pickupX : static_cast<int>(cell.ownerX);
            int oy = cell.isOwner ? pickupY : static_cast<int>(cell.ownerY);
            MinerData* miner = minerSystem->getMiner(ox, oy);
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
        if (cell.tileId == 5 or cell.tileId == 6)
        {
            int ox = cell.isOwner ? pickupX : static_cast<int>(cell.ownerX);
            int oy = cell.isOwner ? pickupY : static_cast<int>(cell.ownerY);
            MachineData* machine = craftingSystem->getMachine(ox, oy);
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

        return false;
    }

    bool tryDrop(InserterData& ins)
    {
        int dropX = ins.x + DIR_DX[ins.direction];
        int dropY = ins.y + DIR_DY[ins.direction];

        if (not gridSystem->getGrid().isInBounds(dropX, dropY))
            return false;

        size_t buildingLayer = gridSystem->getBuildingLayer();
        const auto& cell = gridSystem->getGrid().getCell(buildingLayer, dropX, dropY);

        if (cell.tileId == 0)
            return false;

        // Drop onto belt
        if (cell.tileId == 4)
        {
            if (transportSystem->tryPlaceItem(dropX, dropY, ins.heldItem))
            {
                ins.heldItem = ITEM_NONE;
                return true;
            }
            return false;
        }

        // Drop into machine input
        if (cell.tileId == 5 or cell.tileId == 6)
        {
            int ox = cell.isOwner ? dropX : static_cast<int>(cell.ownerX);
            int oy = cell.isOwner ? dropY : static_cast<int>(cell.ownerY);
            MachineData* machine = craftingSystem->getMachine(ox, oy);
            if (machine and machine->inputSlots.canAccept(ins.heldItem, *itemRegistry))
            {
                machine->inputSlots.insert(ins.heldItem, 1, *itemRegistry);
                ins.heldItem = ITEM_NONE;
                return true;
            }
            return false;
        }

        return false;
    }

    // --- Arm Texture ---

    void updateArmTexture(InserterData& ins)
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

    // --- Held Item Visual ---

    void createHeldItemVisual(InserterData& ins)
    {
        if (ins.heldItem == ITEM_NONE)
            return;

        const auto& itemDef = itemRegistry->get(ins.heldItem);
        float z = gridSystem->getGrid().getLayer(gridSystem->getItemLayer()).zIndex + 0.1f;
        float itemSize = static_cast<float>(Grid::TILE_SIZE) * 0.5f;

        if (not itemDef.textureName.empty())
        {
            auto tex = make2DTexture(ecsRef, itemSize, itemSize, itemDef.textureName);
            auto pos = tex.get<PositionComponent>();
            pos->setZ(z);
            tex.get<Texture2DComponent>()->setViewport(GAME_VIEWPORT);
            ins.heldItemEntityId = tex.entity->id;
        }

        updateHeldItemPosition(ins);
    }

    void updateHeldItemPosition(InserterData& ins)
    {
        if (ins.heldItemEntityId == 0)
            return;

        auto ent = ecsRef->getEntity(ins.heldItemEntityId);
        if (not ent)
            return;

        float t = static_cast<float>(ins.animFrame) / static_cast<float>(SWING_FRAMES - 1);

        int pickupX = ins.x - DIR_DX[ins.direction];
        int pickupY = ins.y - DIR_DY[ins.direction];
        int dropX = ins.x + DIR_DX[ins.direction];
        int dropY = ins.y + DIR_DY[ins.direction];

        auto [pw, ph] = gridSystem->getGrid().gridToWorld(pickupX, pickupY);
        auto [dw, dh] = gridSystem->getGrid().gridToWorld(dropX, dropY);

        float itemSize = static_cast<float>(Grid::TILE_SIZE) * 0.5f;
        float offset = (Grid::TILE_SIZE - itemSize) * 0.5f;

        auto pos = ent->get<PositionComponent>();
        pos->setX(pw + (dw - pw) * t + offset);
        pos->setY(ph + (dh - ph) * t + offset);
    }

    void destroyHeldItemVisual(InserterData& ins)
    {
        if (ins.heldItemEntityId != 0)
        {
            ecsRef->removeEntity(ins.heldItemEntityId);
            ins.heldItemEntityId = 0;
        }
    }

    // --- Members ---

    GridSystem* gridSystem = nullptr;
    TransportSystem* transportSystem = nullptr;
    MinerSystem* minerSystem = nullptr;
    CraftingSystem* craftingSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;

    std::unordered_map<uint32_t, InserterData> inserters;
    size_t tickAccumulator = 0;
};
