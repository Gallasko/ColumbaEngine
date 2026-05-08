#include "storageui.h"

#include "2D/simple2dobject.h"
#include "2D/position.h"
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
    const float panelW = getPanelWidth();
    const float panelH = getPanelHeight();

    // Anchor target: storage panel sits left of the inventory panel, vertically
    // centred against it (or against __MainWindow as fallback).
    uint64_t anchorTargetId = 0;
    if (inventoryUI)
        anchorTargetId = inventoryUI->getBackdropEntityId();
    if (anchorTargetId == 0)
    {
        auto windowEnt = ecsRef->getEntity("__MainWindow");
        if (windowEnt) anchorTargetId = windowEnt->id;
    }

    auto* factory = ecsRef->getSystem<PrefabFactoryRegistry>();
    {
        auto panelEnt = factory->build("Panel", PrefabParams{
            {"width",    panelW},
            {"height",   panelH},
            {"r",         20.0f}, {"g", 20.0f}, {"b", 30.0f}, {"a", 220.0f},
            {"z",         97.0f},
            {"viewport", static_cast<int>(UI_VP)},
        });
        backdropEntityId = panelEnt->id;

        auto a = panelEnt->get<UiAnchor>();
        a->setRightAnchor(PosAnchor{anchorTargetId, AnchorType::Left});
        a->setRightMargin(GAP_BETWEEN_PANELS);
        a->setVerticalCenter(PosAnchor{anchorTargetId, AnchorType::VerticalCenter});

        if (auto bgEnt = panelEnt->get<Prefab>()->getEntity("bg"))
        {
            ecsRef->attach<MouseLeftClickComponent>(bgEnt,
                makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);
        }
    }

    {
        auto titleEnt = factory->build("Text", PrefabParams{
            {"x", 0.0f}, {"y", 0.0f},
            {"z",        100.0f},
            {"font",     std::string(FONT_PATH)},
            {"text",     std::string("Storage")},
            {"scale",    TITLE_SCALE},
            {"viewport", static_cast<int>(UI_VP)},
        });
        titleEntityId = titleEnt->id;

        auto a = ecsRef->attach<UiAnchor>(titleEnt);
        a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setLeftMargin(PANEL_PADDING);
        a->setTopMargin(PANEL_PADDING + 4.0f);
    }

    // Slots via SlotSystem (UiAnchor auto-attached by createSlot)
    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        auto slotRef = slotSystem->createSlot(
            SlotCategory::Input, static_cast<uint8_t>(i));
        slotEntityIds[i] = slotRef.id;

        size_t col = i % COLS;
        size_t row = i / COLS;

        auto a = slotRef.get<UiAnchor>();
        a->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        a->setLeftMargin(PANEL_PADDING + col * (SLOT_SIZE + SLOT_SPACING));
        a->setTopMargin(PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE
                       + row * (SLOT_SIZE + SLOT_SPACING));
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
