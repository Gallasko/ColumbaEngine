#pragma once

#include "Systems/basicsystems.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/inputcomponent.h"
#include "Input/sdlevents.h"
#include "UI/prefab.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

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

// ---- Prefab factory ----

template <typename Type>
CompList<PositionComponent, UiAnchor, Prefab> makeUiSlot(
    Type* ecs, ItemRegistry* itemRegistry,
    float slotSize = DEFAULT_SLOT_SIZE,
    float itemSize = DEFAULT_ITEM_SIZE)
{
    // Prefab entity (invisible container, takes on bg size)
    auto slot = makeAnchoredPrefab(ecs);
    auto prefab = slot.get<Prefab>();

    // Background rect
    auto bg = makeUiSimple2DShape(ecs, Shape2D::Square, slotSize, slotSize,
        constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
    bg.get<PositionComponent>()->setZ(98.0f);
    bg.get<ViewportComponent>()->setViewport(SLOT_UI_VIEWPORT);
    prefab->setMainEntity(bg.entity);

    // Item texture (centered in bg, hidden by default)
    auto item = makeUiTexture(ecs, itemSize, itemSize, "NoneIcon");
    item.get<PositionComponent>()->setZ(99.0f);
    item.get<PositionComponent>()->setVisibility(false);
    item.get<ViewportComponent>()->setViewport(SLOT_UI_VIEWPORT);

    auto itemAnchor = item.get<UiAnchor>();
    itemAnchor->centeredIn(bg.get<UiAnchor>());

    prefab->addToPrefab(item.entity, "item");

    // Count text (bottom-right of bg, hidden by default)
    auto text = makeTTFText(ecs,
        0.0f, 0.0f, 100.0f,
        SLOT_FONT_PATH, "", DEFAULT_TEXT_SCALE,
        {255.0f, 255.0f, 255.0f, 255.0f});
    text.get<PositionComponent>()->setVisibility(false);
    text.get<ViewportComponent>()->setViewport(SLOT_UI_VIEWPORT);

    auto textAnchor = text.get<UiAnchor>();
    textAnchor->setLeftAnchor(PosAnchor{bg.entity->id, AnchorType::Left});
    textAnchor->setLeftMargin(slotSize - 4.0f);
    textAnchor->setTopAnchor(PosAnchor{bg.entity->id, AnchorType::Top});
    textAnchor->setTopMargin(slotSize - 4.0f);

    prefab->addToPrefab(text.entity, "text");

    // ---- Helpers ----

    prefab->addHelper("setItem", [itemRegistry, itemSize](Prefab* self, ItemStack stack) {
        auto itemEnt = self->getEntity("item");
        auto textEnt = self->getEntity("text");

        if (stack.isEmpty())
        {
            if (itemEnt)
                itemEnt->get<PositionComponent>()->setVisibility(false);

            if (textEnt)
                textEnt->get<PositionComponent>()->setVisibility(false);
            return;
        }

        const auto& def = itemRegistry->get(stack.id);

        if (itemEnt)
        {
            itemEnt->get<Texture2DComponent>()->setTexture(def.textureName);
            auto pos = itemEnt->get<PositionComponent>();
            pos->setWidth(itemSize * def.iconWidthRatio);
            pos->setHeight(itemSize);
            pos->setVisibility(true);
        }

        if (textEnt)
        {
            if (stack.count > 1)
            {
                textEnt->get<TTFText>()->setText(std::to_string(stack.count));
                textEnt->get<PositionComponent>()->setVisibility(true);
            }
            else
            {
                textEnt->get<PositionComponent>()->setVisibility(false);
            }
        }
    });

    prefab->addHelper("clear", [](Prefab* self) {
        auto itemEnt = self->getEntity("item");
        if (itemEnt)
            itemEnt->get<PositionComponent>()->setVisibility(false);

        auto textEnt = self->getEntity("text");
        if (textEnt)
            textEnt->get<PositionComponent>()->setVisibility(false);
    });

    return slot;
}

// ---- Events ----

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

// ---- System ----

class SlotSystem : public System<Own<SlotComponent>,
                                  InitSys,
                                  QueuedListener<OnMouseClick>,
                                  QueuedListener<OnSDLMouseMotion>>
{
public:
    SlotSystem(ItemRegistry* itemRegistry)
        : itemRegistry(itemRegistry) {}

    virtual std::string getSystemName() const override { return "Slot System"; }

    void init() override;
    void execute() override;

    virtual void onProcessEvent(const OnMouseClick& event) override;
    virtual void onProcessEvent(const OnSDLMouseMotion& event) override;

    // Create a slot prefab entity with SlotComponent attached.
    // Returns the prefab entity ID. Caller positions via UiAnchor.
    uint64_t createSlot(SlotCategory category, uint8_t index,
                        SlotFlags flags = SlotFlags::None,
                        float slotSize = DEFAULT_SLOT_SIZE,
                        float itemSize = DEFAULT_ITEM_SIZE);

    bool hasHeldItem() const { return not heldItem.isEmpty(); }
    const ItemStack& getHeldItem() const { return heldItem; }
    void cancelHeld();

private:
    void pickUpFrom(uint64_t entityId);
    void dropOn(uint64_t entityId);

    void showHeldVisual();
    void hideHeldVisual();
    void updateHeldPosition();

    uint64_t slotEntityAtPosition(float x, float y) const;

    ItemRegistry* itemRegistry = nullptr;

    // Drag state
    ItemStack heldItem;
    uint64_t  heldFromEntity   = 0;
    uint64_t  heldItemEntityId = 0;
    uint64_t  heldTextEntityId = 0;

    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
};
