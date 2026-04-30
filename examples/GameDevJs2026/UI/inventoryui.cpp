#include "inventoryui.h"

#include "2D/simple2dobject.h"
#include "UI/sizer.h"
#include "2D/position.h"

#include <SDL2/SDL.h>

void InventoryUISystem::init()
{
    ensurePanelCreated();
}

ItemId InventoryUISystem::itemAtPosition(float x, float y) const
{
    if (not visible)
        return ITEM_NONE;

    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        auto ent = ecsRef->getEntity(slotEntityIds[i]);
        if (not ent)
            continue;

        auto pos = ent->get<PositionComponent>();

        if (x >= pos->x and x <= pos->x + SLOT_SIZE and
            y >= pos->y and y <= pos->y + SLOT_SIZE)
        {
            const auto& stack = playerInv->getInventory().getSlot(i);

            return stack.isEmpty() ? ITEM_NONE : stack.id;
        }
    }

    return ITEM_NONE;
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void InventoryUISystem::onProcessEvent(const OnSDLScanCode& event)
{
    if (event.key == SDL_SCANCODE_TAB)
    {
        if (visible)
            closeInventory();
        else
            openInventory();
    }
}

void InventoryUISystem::onProcessEvent(const SlotPickedUpEvent& event)
{
    if (not visible)
        return;

    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        if (event.slotEntityId == slotEntityIds[i])
        {
            syncSlotToInventory(i);
            return;
        }
    }
}

void InventoryUISystem::onProcessEvent(const SlotDroppedEvent& event)
{
    if (not visible)
        return;

    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        if (event.slotEntityId == slotEntityIds[i])
        {
            syncSlotToInventory(i);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Open / Close
// ---------------------------------------------------------------------------

void InventoryUISystem::openInventory()
{
    visible = true;
    ensurePanelCreated();
    setPanelVisibility(true);
    syncAllSlots();
    ecsRef->sendEvent(InventoryOpenedEvent{});
}

void InventoryUISystem::closeInventory()
{
    if (not visible)
        return;

    if (slotSystem->hasHeldItem())
        slotSystem->cancelHeld();

    // Sync all slots back to player inventory
    for (size_t i = 0; i < NUM_SLOTS; ++i)
        syncSlotToInventory(i);

    setPanelVisibility(false);
    visible = false;
    ecsRef->sendEvent(InventoryClosedEvent{});
}

void InventoryUISystem::cancelHeld()
{
    slotSystem->cancelHeld();

    // After cancelHeld, sync all inventory slots to capture returned items
    for (size_t i = 0; i < NUM_SLOTS; ++i)
        syncSlotToInventory(i);

    if (visible)
        syncAllSlots();
}

void InventoryUISystem::refreshAllSlots()
{
    if (not panelCreated)
        return;

    syncAllSlots();
}

// ---------------------------------------------------------------------------
// Panel creation / visibility
// ---------------------------------------------------------------------------

void InventoryUISystem::ensurePanelCreated()
{
    if (panelCreated)
        return;

    createPanel();
    setPanelVisibility(false);
    panelCreated = true;
}

void InventoryUISystem::setPanelVisibility(bool vis)
{
    setEntityVisibility(backdropEntityId, vis);

    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        auto ent = ecsRef->getEntity(slotEntityIds[i]);
        if (ent)
            ent->get<PositionComponent>()->setVisibility(vis);
    }
}

void InventoryUISystem::createPanel()
{
    auto windowEnt = ecsRef->getEntity("__MainWindow");
    auto windowAnchor = windowEnt->get<UiAnchor>();

    float panelW = COLS * SLOT_SIZE + (COLS - 1) * SLOT_SPACING + 2 * PANEL_PADDING;
    float panelH = ROWS * SLOT_SIZE + (ROWS - 1) * SLOT_SPACING + 2 * PANEL_PADDING;

    // Backdrop — centered in __MainWindow
    auto backdrop = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{20.0f, 20.0f, 30.0f, 220.0f});

    auto bdPos = backdrop.get<PositionComponent>();
    bdPos->setZ(97.0f);
    bdPos->setWidth(panelW);
    bdPos->setHeight(panelH);
    backdrop.get<ViewportComponent>()->setViewport(INV_UI_VIEWPORT);
    backdropEntityId = backdrop.entity->id;

    ecsRef->attach<MouseLeftClickComponent>(backdrop.entity,
        makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);

    auto bdAnchor = ecsRef->attach<UiAnchor>(backdrop.entity);
    bdAnchor->centeredIn(windowAnchor);

    // Horizontal layout for slot grid (fitToAxis wraps into 5-column rows)
    float contentW = COLS * SLOT_SIZE + (COLS - 1) * SLOT_SPACING;
    float contentH = ROWS * SLOT_SIZE + (ROWS - 1) * SLOT_SPACING;
    auto layout = makeHorizontalLayout(ecsRef, 0, 0, contentW, contentH, false);
    auto hLayout = layout.get<HorizontalLayout>();
    hLayout->fitToAxis = true;
    hLayout->spacing = SLOT_SPACING;

    auto layoutAnchor = layout.get<UiAnchor>();
    layoutAnchor->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
    layoutAnchor->setLeftMargin(PANEL_PADDING);
    layoutAnchor->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
    layoutAnchor->setTopMargin(PANEL_PADDING);

    // Slots via SlotSystem
    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        auto slotRef = slotSystem->createSlot(
            SlotCategory::PlayerInventory, static_cast<uint8_t>(i));
        slotEntityIds[i] = slotRef.id;

        hLayout->addEntity(slotRef.entity);
    }
}

// ---------------------------------------------------------------------------
// Sync helpers
// ---------------------------------------------------------------------------

void InventoryUISystem::syncAllSlots()
{
    for (size_t i = 0; i < NUM_SLOTS; ++i)
        slotSystem->syncSlotVisual(slotEntityIds[i], playerInv->getInventory().getSlot(i));
}

void InventoryUISystem::syncSlotToInventory(size_t index)
{
    auto* sc = slotSystem->getSlotComponent(slotEntityIds[index]);
    if (sc)
        playerInv->getInventory().getSlot(index) = sc->stack;
}

// ---------------------------------------------------------------------------
// Visibility helper
// ---------------------------------------------------------------------------

void InventoryUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0)
        return;

    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}
