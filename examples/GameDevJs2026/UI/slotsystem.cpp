#include "slotsystem.h"

#include "UI/prefabspec.h"
#include "UI/prefabbuilder.h"

static inline void notifyChange(SlotComponent* slot)
{
    if (slot and slot->onChange)
        slot->onChange(slot->stack);
}

SlotCategory parseSlotCategory(const std::string& s)
{
    if (s == "Input")   return SlotCategory::Input;
    if (s == "Output")  return SlotCategory::Output;
    if (s == "Hotbar")  return SlotCategory::Hotbar;
    return SlotCategory::PlayerInventory;
}

const char* slotCategoryToString(SlotCategory cat)
{
    switch (cat)
    {
        case SlotCategory::Input:           return "Input";
        case SlotCategory::Output:          return "Output";
        case SlotCategory::Hotbar:          return "Hotbar";
        case SlotCategory::PlayerInventory: return "PlayerInventory";
    }
    return "PlayerInventory";
}

EntityRef makeSlotPrefab(EntitySystem* ecs, ItemRegistry* itemRegistry, const PrefabParams& params)
{
    const float slotSize = getParamFloat(params, SlotPrefabKeys::SlotSize, DEFAULT_SLOT_SIZE);
    const float itemSize = getParamFloat(params, SlotPrefabKeys::ItemSize, DEFAULT_ITEM_SIZE);

    NodeSpec spec;
    spec.kind = "Shape2D";
    spec.name = "bg";
    spec.props = {
        {"width",    slotSize},
        {"height",   slotSize},
        {"r",        getParamFloat(params, SlotPrefabKeys::BgR, 50.0f)},
        {"g",        getParamFloat(params, SlotPrefabKeys::BgG, 50.0f)},
        {"b",        getParamFloat(params, SlotPrefabKeys::BgB, 60.0f)},
        {"a",        getParamFloat(params, SlotPrefabKeys::BgA, 200.0f)},
        {"z",        98.0f},
        {"viewport", static_cast<int>(SLOT_UI_VIEWPORT)},
    };

    {
        NodeSpec item;
        item.kind = "Texture";
        item.name = "item";
        item.props = {
            {"texture",    std::string("NoneIcon")},
            {"width",      itemSize},
            {"height",     itemSize},
            {"z",          99.0f},
            {"viewport",   static_cast<int>(SLOT_UI_VIEWPORT)},
            {"visibility", false},
        };
        item.anchors = centerInAnchors("main");
        spec.children.push_back(std::move(item));
    }

    {
        NodeSpec text;
        text.kind = "TTFText";
        text.name = "text";
        text.props = {
            {"font",       std::string(SLOT_FONT_PATH)},
            {"text",       std::string("")},
            {"scale",      DEFAULT_TEXT_SCALE},
            {"r",          255.0f},
            {"g",          255.0f},
            {"b",          255.0f},
            {"a",          255.0f},
            {"z",          100.0f},
            {"viewport",   static_cast<int>(SLOT_UI_VIEWPORT)},
            {"visibility", false},
        };
        text.anchors = {
            AnchorSpec{"main", AnchorType::Left, slotSize - 4.0f},
            AnchorSpec{"main", AnchorType::Top,  slotSize - 4.0f},
        };
        spec.children.push_back(std::move(text));
    }

    EntityRef slot = buildNode(ecs, spec);
    auto prefab = slot->get<Prefab>();
    EntityRef bg = prefab->getEntity("bg");
    EntityRef itemEnt = prefab->getEntity("item");
    EntityRef textEnt = prefab->getEntity("text");

    // Click handler: fires SlotClickedEvent on press only (click-to-grab / click-to-drop)
    ecs->attach<MouseLeftClickComponent>(bg,
        makeCallable<SlotClickedEvent>(slot->id), MouseStateTrigger::OnPress);

    // PrefabSystem auto-pins each child leaf's z to its own child-wrap (z=0).
    // Re-constrain item/text to the slot wrap's z so they always render above
    // the bg, regardless of where the caller sets the slot wrap's z (the
    // hotbar sets it to 95; inventory leaves it at 0).
    if (itemEnt and itemEnt->has<UiAnchor>())
        itemEnt->get<UiAnchor>()->setZConstrain(
            PosConstrain{slot.id, AnchorType::Z, PosOpType::Add, 1.0f});
    if (textEnt and textEnt->has<UiAnchor>())
        textEnt->get<UiAnchor>()->setZConstrain(
            PosConstrain{slot.id, AnchorType::Z, PosOpType::Add, 2.0f});

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

    // If the caller supplied a category, attach SlotComponent so the result is a
    // fully-functional slot (game identity included). Otherwise leave it as a pure
    // visual prefab — callers can attach SlotComponent themselves.
    const std::string categoryStr = getParamString(params, SlotPrefabKeys::Category);
    if (not categoryStr.empty())
    {
        const SlotCategory category = parseSlotCategory(categoryStr);
        const uint8_t  index = static_cast<uint8_t>(getParamInt(params, SlotPrefabKeys::Index, 0));
        const SlotFlags flags = static_cast<SlotFlags>(getParamInt(params, SlotPrefabKeys::Flags, 0));
        ecs->attach<SlotComponent>(slot, category, index, flags);
    }

    return slot;
}

void registerSlotFactory(PrefabFactoryRegistry* factory, ItemRegistry* itemRegistry)
{
    if (not factory)
        return;

    ParamSchema schema;
    schema.entries = {
        {SlotPrefabKeys::SlotSize, DEFAULT_SLOT_SIZE},
        {SlotPrefabKeys::ItemSize, DEFAULT_ITEM_SIZE},
        {SlotPrefabKeys::BgR,      50.0f},
        {SlotPrefabKeys::BgG,      50.0f},
        {SlotPrefabKeys::BgB,      60.0f},
        {SlotPrefabKeys::BgA,      200.0f},
        // Game-identity params: when category is empty the factory skips the
        // SlotComponent attach and returns a pure visual prefab (back-compat).
        {SlotPrefabKeys::Category, std::string{""}},
        {SlotPrefabKeys::Index,    0},
        {SlotPrefabKeys::Flags,    0},
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
    // Push the game identity through params so makeSlotPrefab attaches SlotComponent
    // itself. Same effect whether we go through the registered factory or the direct
    // fallback (both paths route through makeSlotPrefab).
    PrefabParams params = {
        {SlotPrefabKeys::SlotSize, slotSize},
        {SlotPrefabKeys::ItemSize, itemSize},
        {SlotPrefabKeys::BgR,      bgColor.x},
        {SlotPrefabKeys::BgG,      bgColor.y},
        {SlotPrefabKeys::BgB,      bgColor.z},
        {SlotPrefabKeys::BgA,      bgColor.w},
        {SlotPrefabKeys::Category, std::string(slotCategoryToString(category))},
        {SlotPrefabKeys::Index,    static_cast<int>(index)},
        {SlotPrefabKeys::Flags,    static_cast<int>(flags)},
    };

    auto* factory = ecsRef->getSystem<PrefabFactoryRegistry>();
    if (factory and factory->hasFactory("Slot"))
        return factory->build("Slot", params);

    return makeSlotPrefab(ecsRef, itemRegistry, params);
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
    notifyChange(slot);

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
        notifyChange(slot);
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
        notifyChange(slot);

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
            notifyChange(slot);
            if (hasPrefab)
                ent->get<Prefab>()->callHelper("setItem", slot->stack);

            // Source gets the target's old item
            sourceSlot->stack = oldTarget;
            notifyChange(sourceSlot);
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
            notifyChange(slot);

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
            notifyChange(sourceSlot);

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

void SlotSystem::bindSlotChange(EntityRef entity, std::function<void(const ItemStack&)> cb)
{
    // Use the EntityRef's cached Entity* (operator->) rather than ecsRef->getEntity():
    // when this is called right after the slot was created in a running ECS (lazy
    // panel creation on first machine-UI open), both the entity and its SlotComponent
    // are still pending in the cmdDispatcher and haven't been flushed into entityPool
    // / the Own<> sparse set yet. ecsRef->getEntity() only checks entityPool and would
    // return null, silently dropping the bind. The EntityRef holds the live pointer.
    if (entity and entity->has<SlotComponent>())
        entity->get<SlotComponent>()->onChange = std::move(cb);
}

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


