#include "storageui.h"

#include "2D/simple2dobject.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

// ---------------------------------------------------------------------------
// open / close
// ---------------------------------------------------------------------------

void StorageUISystem::open(int gridX, int gridY)
{
    if (visible)
        close();

    openStorageX = gridX;
    openStorageY = gridY;
    visible = true;

    if (inventoryUI and not inventoryUI->isOpen())
        inventoryUI->openInventory();

    ensurePanelCreated();
    setPanelVisibility(true);
    syncAllSlots();
}

void StorageUISystem::close()
{
    if (not visible)
        return;

    if (slotSystem->hasHeldItem())
        slotSystem->cancelHeld();

    // Sync all slots back to storage before closing
    StorageData* storage = storageSystem->getStorage(openStorageX, openStorageY);
    if (storage)
    {
        for (size_t i = 0; i < NUM_SLOTS; ++i)
            syncSlotToStorage(i);
    }

    setPanelVisibility(false);
    visible = false;
    openStorageX = -1;
    openStorageY = -1;

    // Close the companion inventory (cascades to crafting).
    if (inventoryUI and inventoryUI->isOpen())
        inventoryUI->closeInventory();
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void StorageUISystem::onProcessEvent(const OnSDLScanCode& event)
{
    if (not visible)
        return;

    if (event.key == SDL_SCANCODE_ESCAPE)
        close();
}

void StorageUISystem::onEvent(const InventoryClosedEvent&)
{
    if (visible)
        close();
}

void StorageUISystem::onProcessEvent(const TickEvent&)
{
    if (not visible)
        return;

    StorageData* storage = storageSystem->getStorage(openStorageX, openStorageY);
    if (not storage)
    {
        close();
        return;
    }

    syncAllSlots();
}

void StorageUISystem::onProcessEvent(const SlotPickedUpEvent& event)
{
    if (not visible)
        return;

    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        if (event.slotEntityId == slotEntityIds[i])
        {
            syncSlotToStorage(i);
            return;
        }
    }
}

void StorageUISystem::onProcessEvent(const SlotDroppedEvent& event)
{
    if (not visible)
        return;

    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        if (event.slotEntityId == slotEntityIds[i])
        {
            syncSlotToStorage(i);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

float StorageUISystem::getPanelX() const
{
    float invW = InventoryUISystem::COLS * InventoryUISystem::SLOT_SIZE
               + (InventoryUISystem::COLS - 1) * InventoryUISystem::SLOT_SPACING
               + 2.0f * InventoryUISystem::PANEL_PADDING;
    float invX = (screenWidth - invW) * 0.5f;

    return invX - GAP_BETWEEN_PANELS - getPanelWidth();
}

float StorageUISystem::getPanelY() const
{
    return (screenHeight - getPanelHeight()) * 0.5f;
}

// ---------------------------------------------------------------------------
// Panel creation
// ---------------------------------------------------------------------------

void StorageUISystem::ensurePanelCreated()
{
    if (panelCreated)
        return;
    createPanel();
    setPanelVisibility(false);
    panelCreated = true;
}

void StorageUISystem::setPanelVisibility(bool vis)
{
    setEntityVisibility(backdropEntityId, vis);
    setEntityVisibility(titleEntityId,    vis);

    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        auto ent = ecsRef->getEntity(slotEntityIds[i]);
        if (ent)
            ent->get<PositionComponent>()->setVisibility(vis);
    }
}

void StorageUISystem::createPanel()
{
    float panelX = getPanelX();
    float panelY = getPanelY();
    float panelW = getPanelWidth();
    float panelH = getPanelHeight();

    // Backdrop via the engine "Panel" factory. Returns a Prefab container
    // whose size tracks the inner bg shape; positioning the container moves
    // the whole prefab. The visible bg entity is named "bg" inside the prefab.
    {
        auto* factory = ecsRef->getSystem<PrefabFactoryRegistry>();
        auto panelEnt = factory->build("Panel", PrefabParams{
            {"width",    panelW},
            {"height",   panelH},
            {"r",         20.0f}, {"g", 20.0f}, {"b", 30.0f}, {"a", 220.0f},
            {"z",         97.0f},
            {"viewport", static_cast<int>(UI_VP)},
        });

        auto pos = panelEnt->get<PositionComponent>();
        pos->setX(panelX);
        pos->setY(panelY);
        backdropEntityId = panelEnt->id;

        // Click handler goes on the visible bg shape, not the invisible
        // prefab container, so the click-outside-to-close logic can detect
        // panel clicks at the right pixel area.
        if (auto bgEnt = panelEnt->get<Prefab>()->getEntity("bg"))
        {
            ecsRef->attach<MouseLeftClickComponent>(bgEnt,
                makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);
        }
    }

    // Title
    {
        auto t = makeTTFText(ecsRef,
            panelX + PANEL_PADDING, panelY + PANEL_PADDING + 4.0f, 100.0f,
            FONT_PATH, "Storage", TITLE_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        t.get<ViewportComponent>()->setViewport(UI_VP);
        titleEntityId = t.entity->id;
    }

    // Slots via SlotSystem
    float slotsStartY = panelY + PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE;

    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        auto slotRef = slotSystem->createSlot(
            SlotCategory::Input, static_cast<uint8_t>(i));
        slotEntityIds[i] = slotRef.id;

        size_t col = i % COLS;
        size_t row = i / COLS;
        float sx = panelX + PANEL_PADDING + col * (SLOT_SIZE + SLOT_SPACING);
        float sy = slotsStartY + row * (SLOT_SIZE + SLOT_SPACING);

        auto pos = slotRef.get<PositionComponent>();
        pos->setX(sx);
        pos->setY(sy);
    }
}

// ---------------------------------------------------------------------------
// Sync helpers
// ---------------------------------------------------------------------------

void StorageUISystem::syncAllSlots()
{
    StorageData* storage = storageSystem->getStorage(openStorageX, openStorageY);
    if (not storage)
        return;

    for (size_t i = 0; i < NUM_SLOTS; ++i)
        slotSystem->syncSlotVisual(slotEntityIds[i], storage->inventory.getSlot(i));
}

void StorageUISystem::syncSlotToStorage(size_t slotIndex)
{
    StorageData* storage = storageSystem->getStorage(openStorageX, openStorageY);
    if (not storage)
        return;

    auto* sc = slotSystem->getSlotComponent(slotEntityIds[slotIndex]);
    if (sc)
        storage->inventory.getSlot(slotIndex) = sc->stack;
}

// ---------------------------------------------------------------------------
// Visibility helper
// ---------------------------------------------------------------------------

void StorageUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0)
        return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}
