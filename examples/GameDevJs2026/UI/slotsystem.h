#pragma once

#include "Systems/basicsystems.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/inputcomponent.h"
#include "Input/sdlevents.h"
#include "UI/prefab.h"
#include "UI/prefabfactory.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

#include "ECS/callable.h"
#include "Registries/itemregistry.h"
#include "slotcomponent.h"

using namespace pg;

// ---- Constants ----

static constexpr size_t SLOT_UI_VIEWPORT  = 2;
static constexpr float  DEFAULT_SLOT_SIZE = 40.0f;
static constexpr float  DEFAULT_ITEM_SIZE = 28.0f;
static constexpr float  DEFAULT_TEXT_SCALE = 0.3f;

static constexpr const char* SLOT_FONT_PATH =
    "res/font/Inter/static/Inter_28pt-Light.ttf";

// ---- Events ----

struct SlotClickedEvent
{
    SlotClickedEvent(_unique_id entityId) : entityId(entityId) {}

    _unique_id entityId;
};

struct SlotPickedUpEvent
{
    uint64_t slotEntityId;
    ItemStack item;
    SlotCategory category;
};

struct SlotDroppedEvent
{
    uint64_t slotEntityId;
    ItemStack item;
    SlotCategory category;
};

// ---- Prefab factory ----
//
// The Slot prefab is registered with PrefabFactoryRegistry under the name "Slot"
// during SlotSystem::init(). Build it via:
//
//   factory->build("Slot", { {"slotSize", 40.0f}, {"itemSize", 28.0f}, ... });
//
// Param keys (all optional, defaults applied by the registry's schema):
//   "slotSize" : float
//   "itemSize" : float
//   "bgR", "bgG", "bgB", "bgA" : float (0-255)
//
// SlotComponent is attached separately by SlotSystem::createSlot — the factory
// produces a pure visual prefab with no game-specific data.

namespace SlotPrefabKeys
{
    inline constexpr const char* SlotSize = "slotSize";
    inline constexpr const char* ItemSize = "itemSize";
    inline constexpr const char* BgR      = "bgR";
    inline constexpr const char* BgG      = "bgG";
    inline constexpr const char* BgB      = "bgB";
    inline constexpr const char* BgA      = "bgA";
}

EntityRef makeSlotPrefab(EntitySystem* ecs, ItemRegistry* itemRegistry, const PrefabParams& params);

void registerSlotFactory(PrefabFactoryRegistry* factory, ItemRegistry* itemRegistry);

// ---- System ----

class SlotSystem : public System<Own<SlotComponent>,
                                  InitSys,
                                  QueuedListener<SlotClickedEvent>,
                                  QueuedListener<OnSDLMouseMotion>>
{
public:
    SlotSystem(ItemRegistry* itemRegistry)
        : itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Slot System"; }

    void init() override;
    void execute() override;

    virtual void onProcessEvent(const SlotClickedEvent& event) override;
    virtual void onProcessEvent(const OnSDLMouseMotion& event) override;

    // Create a slot prefab entity with SlotComponent attached.
    // Returns the prefab entity ID. Caller positions via UiAnchor.
    EntityRef createSlot(SlotCategory category, uint8_t index,
                        SlotFlags flags = SlotFlags::None,
                        float slotSize = DEFAULT_SLOT_SIZE,
                        float itemSize = DEFAULT_ITEM_SIZE,
                        constant::Vector4D bgColor = {50.0f, 50.0f, 60.0f, 200.0f});

    // Update a slot's data and visuals from backing data.
    void syncSlotVisual(uint64_t entityId, const ItemStack& newStack);

    // Access a SlotComponent by entity ID (for external sync).
    SlotComponent* getSlotComponent(uint64_t entityId) { return atEntity<SlotComponent>(entityId); }

    bool hasHeldItem() const { return not heldItem.isEmpty(); }
    const ItemStack& getHeldItem() const { return heldItem; }
    void cancelHeld();

private:
    void pickUpFrom(uint64_t entityId);
    void dropOn(uint64_t entityId);

    void showHeldVisual();
    void hideHeldVisual();
    void updateHeldPosition();

    ItemRegistry* itemRegistry = nullptr;

    // Drag state
    ItemStack heldItem;
    uint64_t  heldFromEntity   = 0;
    uint64_t  heldItemEntityId = 0;
    uint64_t  heldTextEntityId = 0;

    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
};
