#include "slotsystem.h"

EntityRef makeSlotPrefab(EntitySystem* ecs, ItemRegistry* itemRegistry, const PrefabParams& params)
{
    const float slotSize = getParamFloat(params, SlotPrefabKeys::SlotSize, DEFAULT_SLOT_SIZE);
    const float itemSize = getParamFloat(params, SlotPrefabKeys::ItemSize, DEFAULT_ITEM_SIZE);
    const constant::Vector4D bgColor = {
        getParamFloat(params, SlotPrefabKeys::BgR, 50.0f),
        getParamFloat(params, SlotPrefabKeys::BgG, 50.0f),
        getParamFloat(params, SlotPrefabKeys::BgB, 60.0f),
        getParamFloat(params, SlotPrefabKeys::BgA, 200.0f),
    };

    // Prefab entity (invisible container, takes on bg size)
    auto slot = makeAnchoredPrefab(ecs);
    auto prefab = slot.get<Prefab>();

    // Background rect
    auto bg = makeUiSimple2DShape(ecs, Shape2D::Square, slotSize, slotSize, bgColor);
    bg.get<PositionComponent>()->setZ(98.0f);
    bg.get<ViewportComponent>()->setViewport(SLOT_UI_VIEWPORT);
    auto bgAnchor = bg.get<UiAnchor>();
    prefab->setMainEntity(bg.entity);

    // Click handler: fires SlotClickedEvent on press only (click-to-grab / click-to-drop)
    bg.attach<MouseLeftClickComponent>(
        makeCallable<SlotClickedEvent>(slot.entity->id), MouseStateTrigger::OnPress);

    // Item texture (centered in bg, hidden by default)
    auto item = makeUiTexture(ecs, itemSize, itemSize, "NoneIcon");
    item.get<PositionComponent>()->setZ(99.0f);
    item.get<PositionComponent>()->setVisibility(false);
    item.get<ViewportComponent>()->setViewport(SLOT_UI_VIEWPORT);

    auto itemAnchor = item.get<UiAnchor>();
    itemAnchor->centeredIn(bgAnchor);

    prefab->addToPrefab(item.entity, "item");

    // Count text (bottom-right of bg, hidden by default)
    auto text = makeTTFText(ecs,
        0.0f, 0.0f, 100.0f,
        SLOT_FONT_PATH, "", DEFAULT_TEXT_SCALE,
        {255.0f, 255.0f, 255.0f, 255.0f});
    text.get<PositionComponent>()->setVisibility(false);
    text.get<ViewportComponent>()->setViewport(SLOT_UI_VIEWPORT);

    auto textAnchor = text.get<UiAnchor>();
    textAnchor->setLeftAnchor(bgAnchor->left);
    textAnchor->setLeftMargin(slotSize - 4.0f);
    textAnchor->setTopAnchor(bgAnchor->top);
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

    return slot.entity;
}

void registerSlotFactory(PrefabFactoryRegistry* factory, ItemRegistry* itemRegistry)
{
    if (not factory)
        return;

    ParamSchema schema;
    schema.entries = {
        {SlotPrefabKeys::SlotSize, ElementType::UnionType::FLOAT, ElementType{DEFAULT_SLOT_SIZE}},
        {SlotPrefabKeys::ItemSize, ElementType::UnionType::FLOAT, ElementType{DEFAULT_ITEM_SIZE}},
        {SlotPrefabKeys::BgR,      ElementType::UnionType::FLOAT, ElementType{50.0f}},
        {SlotPrefabKeys::BgG,      ElementType::UnionType::FLOAT, ElementType{50.0f}},
        {SlotPrefabKeys::BgB,      ElementType::UnionType::FLOAT, ElementType{60.0f}},
        {SlotPrefabKeys::BgA,      ElementType::UnionType::FLOAT, ElementType{200.0f}},
    };

    factory->registerFactory("Slot", std::move(schema),
        [itemRegistry](EntitySystem* ecs, const PrefabParams& params) {
            return makeSlotPrefab(ecs, itemRegistry, params);
        });
}

void SlotSystem::init()
{
    // Register the Slot factory if a PrefabFactoryRegistry is available.
    auto* factory = ecsRef->getSystem<PrefabFactoryRegistry>();
    if (factory and not factory->hasFactory("Slot"))
        registerSlotFactory(factory, itemRegistry);

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

EntityRef SlotSystem::createSlot(SlotCategory category, uint8_t index,
                                 SlotFlags flags, float slotSize, float itemSize,
                                 constant::Vector4D bgColor)
{
    PrefabParams params = {
        {SlotPrefabKeys::SlotSize, slotSize},
        {SlotPrefabKeys::ItemSize, itemSize},
        {SlotPrefabKeys::BgR,      bgColor.x},
        {SlotPrefabKeys::BgG,      bgColor.y},
        {SlotPrefabKeys::BgB,      bgColor.z},
        {SlotPrefabKeys::BgA,      bgColor.w},
    };

    EntityRef slotEnt;
    auto* factory = ecsRef->getSystem<PrefabFactoryRegistry>();
    if (factory and factory->hasFactory("Slot"))
        slotEnt = factory->build("Slot", params);
    else
        slotEnt = makeSlotPrefab(ecsRef, itemRegistry, params);

    ecsRef->attach<SlotComponent>(slotEnt, category, index, flags);

    return slotEnt;
}

// ---- Mouse events ----

void SlotSystem::onProcessEvent(const SlotClickedEvent& event)
{
    auto* slot = atEntity<SlotComponent>(event.entityId);
    if (not slot)
        return;

    if (heldItem.isEmpty())
    {
        if (not slot->isNoPickUp())
            pickUpFrom(event.entityId);
    }
    else
    {
        dropOn(event.entityId);
    }
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
    if (not slot or slot->isReadOnly() or slot->isOutputOnly())
        return;

    auto ent = ecsRef->getEntity(entityId);
    bool hasPrefab = ent and ent->has<Prefab>();

    if (slot->isEmpty())
    {
        slot->stack = heldItem;
        if (hasPrefab)
            ent->get<Prefab>()->callHelper("setItem", slot->stack);

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
        // Swap — place held item in target, return target's item to source
        auto* sourceSlot = (heldFromEntity != 0) ? atEntity<SlotComponent>(heldFromEntity) : nullptr;
        auto* sourceEnt = (heldFromEntity != 0) ? ecsRef->getEntity(heldFromEntity) : nullptr;

        if (sourceSlot and sourceEnt)
        {
            ItemStack oldTarget = slot->stack;

            // Target gets the held item
            slot->stack = heldItem;
            if (hasPrefab)
                ent->get<Prefab>()->callHelper("setItem", slot->stack);

            // Source gets the target's old item
            sourceSlot->stack = oldTarget;
            if (sourceEnt->has<Prefab>())
                sourceEnt->get<Prefab>()->callHelper("setItem", sourceSlot->stack);

            sendEvent(SlotDroppedEvent{entityId, slot->stack, slot->category});
            sendEvent(SlotDroppedEvent{heldFromEntity, oldTarget, sourceSlot->category});

            heldItem.clear();
            heldFromEntity = 0;
            hideHeldVisual();
        }
        else
        {
            // Fallback: source slot gone, keep displaced item held
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

// ---- External sync ----

void SlotSystem::syncSlotVisual(uint64_t entityId, const ItemStack& newStack)
{
    auto* slot = atEntity<SlotComponent>(entityId);
    if (not slot)
        return;

    slot->stack = newStack;

    auto ent = ecsRef->getEntity(entityId);
    if (ent and ent->has<Prefab>())
    {
        if (newStack.isEmpty())
            ent->get<Prefab>()->callHelper("clear");
        else
            ent->get<Prefab>()->callHelper("setItem", newStack);
    }
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
        auto pos = itemEnt->get<PositionComponent>();
        pos->setWidth(DEFAULT_ITEM_SIZE * def.iconWidthRatio);
        pos->setHeight(DEFAULT_ITEM_SIZE);
        pos->setVisibility(true);
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


