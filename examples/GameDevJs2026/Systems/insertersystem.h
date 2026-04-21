#pragma once

#include "Systems/basicsystems.h"

#include "gridsystem.h"
#include "transportsystem.h"
#include "minersystem.h"
#include "craftingsystem.h"
#include "storagesystem.h"
#include "depotsystem.h"
#include "machinekey.h"
#include "saveserialization.h"

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
                                      Listener<BuildingRemovedEvent>,
                                      SaveSys>
{
public:
    static constexpr size_t ANIM_FRAME_DURATION_MS = 100;
    static constexpr size_t SWING_FRAMES = 5;
    static constexpr size_t TOTAL_SPRITE_FRAMES = 8;

    // Pickup (idle) frame for each direction
    // The 8 sprite frames go clockwise: 0=left, 2=down, 4=right, 6=up
    // Pickup is behind the arm (opposite of direction)
    static constexpr size_t PICKUP_FRAME[4] = {0, 2, 4, 6}; // RIGHT, DOWN, LEFT, UP

    InserterSystem(GridSystem* gridSystem, TransportSystem* transportSystem,
                   MinerSystem* minerSystem, CraftingSystem* craftingSystem,
                   StorageSystem* storageSystem, DepotSystem* depotSystem,
                   ItemRegistry* itemRegistry)
        : gridSystem(gridSystem), transportSystem(transportSystem),
          minerSystem(minerSystem), craftingSystem(craftingSystem),
          storageSystem(storageSystem), depotSystem(depotSystem),
          itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Inserter System"; }

    // SaveSys
    virtual void save(Archive& archive) override;
    virtual void load(const UnserializedObject& serializedString) override;

    virtual void onEvent(const TickEvent& event) override
    {
        tickAccumulator += static_cast<size_t>(event.tick);
    }

    virtual void onEvent(const BuildingPlacedEvent& event) override;
    virtual void onEvent(const BuildingRemovedEvent& event) override;

    void execute() override;

private:
    // Compute the actual sprite frame index (0-7) from direction, animation progress, and state
    static size_t getSpriteFrame(uint8_t direction, size_t animFrame, bool returning);

    void registerInserter(int x, int y);
    void unregisterInserter(int x, int y);
    void inserterTick();

    // --- Pickup / Drop ---

    bool tryPickup(InserterData& ins);
    bool tryDrop(InserterData& ins);

    // --- Arm Texture ---

    void updateArmTexture(InserterData& ins);

    // --- Held Item Visual ---

    void createHeldItemVisual(InserterData& ins);
    void updateHeldItemPosition(InserterData& ins);
    void destroyHeldItemVisual(InserterData& ins);

    // --- Members ---

    GridSystem* gridSystem = nullptr;
    TransportSystem* transportSystem = nullptr;
    MinerSystem* minerSystem = nullptr;
    CraftingSystem* craftingSystem = nullptr;
    StorageSystem* storageSystem = nullptr;
    DepotSystem* depotSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;

    std::unordered_map<uint32_t, InserterData> inserters;
    size_t tickAccumulator = 0;
};
