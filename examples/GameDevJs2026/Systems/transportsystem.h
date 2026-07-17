#pragma once

#include "Systems/basicsystems.h"

#include "gridsystem.h"
#include "itemregistry.h"

struct BeltCell
{
    ItemId   itemId   = ITEM_NONE;
    uint64_t entityId = 0;  // ECS visual entity for the item sprite
};

struct BeltItemGrid
{
    static constexpr int WIDTH  = Grid::WIDTH;
    static constexpr int HEIGHT = Grid::HEIGHT;

    std::array<std::array<BeltCell, WIDTH>, HEIGHT> cells = {};

    BeltCell& get(int x, int y) { return cells[y][x]; }
    const BeltCell& get(int x, int y) const { return cells[y][x]; }
};

struct SavedBeltItem
{
    int x = 0, y = 0;
    ItemId itemId = ITEM_NONE;
};

class TransportSystem : public pg::System<pg::Listener<pg::TickEvent>, pg::Listener<BuildingRemovedEvent>, pg::SaveSys>
{
public:
    static constexpr size_t TRANSPORT_TICK_MS = 250;

    explicit TransportSystem(ItemRegistry* itemRegistry)
        : itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Transport System"; }

    // SaveSys
    virtual void save(pg::Archive& archive) override;
    virtual void load(const pg::UnserializedObject& serializedString) override;

    virtual void onEvent(const pg::TickEvent& event) override
    {
        tickAccumulator += static_cast<size_t>(event.tick);
    }

    virtual void onEvent(const BuildingRemovedEvent& event) override;

    void execute() override;

    bool tryPlaceItem(int x, int y, ItemId id);

    ItemId tryTakeItem(int x, int y);

    ItemId peekItem(int x, int y) const
    {
        if (not ecsRef->getSystem<GridSystem>()->getGrid().isInBounds(x, y))
            return ITEM_NONE;
        return beltGrid.get(x, y).itemId;
    }

    const BeltItemGrid& getBeltGrid() const { return beltGrid; }

private:
    struct MoveEntry
    {
        int fromX, fromY;
        int toX, toY;
        ItemId itemId;
    };

    static uint8_t travelDirection(int fromX, int fromY, int toX, int toY)
    {
        int dx = toX - fromX;
        int dy = toY - fromY;
        if (dx > 0)
            return 0;
        if (dy > 0)
            return 1;
        if (dx < 0)
            return 2;
        return 3;
    }

    void transportTick();

    static constexpr float ITEM_Y_OFFSET = -2.0f; // Shift items slightly upward on belts

    void createItemVisual(int x, int y, ItemId id);
    void destroyItemVisual(int x, int y);
    void moveItemVisual(int fromX, int fromY, int toX, int toY);

    ItemRegistry* itemRegistry = nullptr;

    BeltItemGrid beltGrid;
    size_t tickAccumulator = 0;

    // Round-robin state: last travel direction served at each cell
    std::array<std::array<uint8_t, Grid::WIDTH>, Grid::HEIGHT> lastServedDir = {};
};
