#include "slotsystem.h"

void SlotSystem::init()
{
    // Held-item visual entities (hidden by default, follow cursor during drag)
    auto tex = make2DTexture(ecsRef, DEFAULT_ITEM_SIZE, DEFAULT_ITEM_SIZE, "NoneIcon");
    tex.get<PositionComponent>()->setZ(101.0f);
    tex.get<PositionComponent>()->setVisibility(false);
    tex.get<ViewportComponent>()->setViewport(SLOT_UI_VIEWPORT);
    heldItemEntityId = tex.entity->id;

    auto text = makeTTFText(ecsRef,
        0.0f, 0.0f, 102.0f,
        SLOT_FONT_PATH, "", DEFAULT_TEXT_SCALE,
        {255.0f, 255.0f, 255.0f, 255.0f});
    text.get<PositionComponent>()->setVisibility(false);
    text.get<ViewportComponent>()->setViewport(SLOT_UI_VIEWPORT);
    heldTextEntityId = text.entity->id;
}

void SlotSystem::execute()
{
    // Visual updates happen through prefab helpers called by external code
    // or by the drag-and-drop handlers below. No per-frame polling needed.
}

uint64_t SlotSystem::createSlot(SlotCategory category, uint8_t index,
                                 SlotFlags flags, float slotSize, float itemSize)
{
    auto slot = makeUiSlot(ecsRef, itemRegistry, slotSize, itemSize);

    createOwnedComponent<SlotComponent>(slot.entity.operator->(), category, index, flags);

    return slot.entity->id;
}

// ---- Mouse events ----

void SlotSystem::onProcessEvent(const SlotClickedEvent& event)
{
    if (heldItem.isEmpty())
        pickUpFrom(event.entityId);
    else
        dropOn(event.entityId);
}

void SlotSystem::onProcessEvent(const OnSDLMouseMotion& event)
{
    lastMouseX = static_cast<float>(event.x);
    lastMouseY = static_cast<float>(event.y);

    if (not heldItem.isEmpty())
        updateHeldPosition();
}

// ---- Drag and drop ----

void SlotSystem::pickUpFrom(uint64_t entityId)
{
    auto* slot = atEntity<SlotComponent>(entityId);
    if (not slot or slot->isEmpty() or slot->isReadOnly())
        return;

    heldItem = slot->stack;
    heldFromEntity = entityId;
    slot->stack.clear();

    auto ent = ecsRef->getEntity(entityId);
    if (ent and ent->has<Prefab>())
        ent->get<Prefab>()->callHelper("clear");

    showHeldVisual();
    updateHeldPosition();

    sendEvent(SlotPickedUpEvent{entityId, heldItem, slot->category});
}

void SlotSystem::dropOn(uint64_t entityId)
{
    auto* slot = atEntity<SlotComponent>(entityId);
    if (not slot or slot->isReadOnly())
        return;

    auto ent = ecsRef->getEntity(entityId);
    bool hasPrefab = ent and ent->has<Prefab>();

    if (slot->isEmpty())
    {
        slot->stack = heldItem;
        if (hasPrefab) ent->get<Prefab>()->callHelper("setItem", slot->stack);

        auto dropped = heldItem;
        heldItem.clear();
        heldFromEntity = 0;
        hideHeldVisual();

        sendEvent(SlotDroppedEvent{entityId, dropped, slot->category});
    }
    else if (slot->stack.id == heldItem.id)
    {
        // Stack merge
        uint16_t maxStack = itemRegistry->get(slot->stack.id).maxStack;
        uint16_t space = maxStack - slot->stack.count;
        uint16_t toAdd = std::min(space, heldItem.count);
        slot->stack.count += toAdd;
        heldItem.count -= toAdd;

        if (hasPrefab) ent->get<Prefab>()->callHelper("setItem", slot->stack);

        if (heldItem.count == 0)
        {
            heldItem.clear();
            heldFromEntity = 0;
            hideHeldVisual();

            sendEvent(SlotDroppedEvent{entityId, ItemStack{slot->stack.id, toAdd}, slot->category});
        }
        else
        {
            showHeldVisual();
        }
    }
    else
    {
        // Swap
        ItemStack temp = slot->stack;
        slot->stack = heldItem;
        heldItem = temp;
        heldFromEntity = entityId;

        if (hasPrefab)
            ent->get<Prefab>()->callHelper("setItem", slot->stack);
        showHeldVisual();

        sendEvent(SlotDroppedEvent{entityId, slot->stack, slot->category});
    }
}

void SlotSystem::cancelHeld()
{
    if (heldItem.isEmpty())
        return;

    // Return the item to its source slot
    if (heldFromEntity != 0)
    {
        auto* sourceSlot = atEntity<SlotComponent>(heldFromEntity);
        if (sourceSlot and sourceSlot->isEmpty())
        {
            sourceSlot->stack = heldItem;

            auto ent = ecsRef->getEntity(heldFromEntity);
            if (ent and ent->has<Prefab>())
                ent->get<Prefab>()->callHelper("setItem", sourceSlot->stack);
        }
    }

    heldItem.clear();
    heldFromEntity = 0;
    hideHeldVisual();
}

// ---- Held visual ----

void SlotSystem::showHeldVisual()
{
    if (heldItem.isEmpty())
        return;

    const auto& def = itemRegistry->get(heldItem.id);

    auto itemEnt = ecsRef->getEntity(heldItemEntityId);
    if (itemEnt)
    {
        itemEnt->get<Texture2DComponent>()->setTexture(def.textureName);
        itemEnt->get<PositionComponent>()->setVisibility(true);
    }

    auto textEnt = ecsRef->getEntity(heldTextEntityId);
    if (textEnt)
    {
        if (heldItem.count > 1)
        {
            textEnt->get<TTFText>()->setText(std::to_string(heldItem.count));
            textEnt->get<PositionComponent>()->setVisibility(true);
        }
        else
        {
            textEnt->get<PositionComponent>()->setVisibility(false);
        }
    }

    updateHeldPosition();
}

void SlotSystem::hideHeldVisual()
{
    auto itemEnt = ecsRef->getEntity(heldItemEntityId);
    if (itemEnt)
        itemEnt->get<PositionComponent>()->setVisibility(false);

    auto textEnt = ecsRef->getEntity(heldTextEntityId);
    if (textEnt)
        textEnt->get<PositionComponent>()->setVisibility(false);
}

void SlotSystem::updateHeldPosition()
{
    constexpr float offset = 8.0f;

    auto itemEnt = ecsRef->getEntity(heldItemEntityId);
    if (itemEnt)
    {
        auto pos = itemEnt->get<PositionComponent>();
        pos->setX(lastMouseX + offset);
        pos->setY(lastMouseY + offset);
    }

    auto textEnt = ecsRef->getEntity(heldTextEntityId);
    if (textEnt)
    {
        auto pos = textEnt->get<PositionComponent>();
        pos->setX(lastMouseX + offset + DEFAULT_ITEM_SIZE - 4.0f);
        pos->setY(lastMouseY + offset + DEFAULT_ITEM_SIZE - 4.0f);
    }
}


