#include "storageui.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

// ---------------------------------------------------------------------------
// isClickOnPanel
// ---------------------------------------------------------------------------

bool StorageUISystem::isClickOnPanel(float x, float y) const
{
    if (not visible) return false;
    float px = getPanelX();
    float py = getPanelY();
    return x >= px and x <= px + getPanelWidth()
       and y >= py and y <= py + getPanelHeight();
}

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

    inventoryUI->setExternalClickCheck([this](float x, float y) {
        return isClickOnPanel(x, y);
    });

    ensurePanelCreated();
    setPanelVisibility(true);
    refreshAllSlots();
}

void StorageUISystem::close()
{
    if (inventoryUI and inventoryUI->hasHeldItem())
        inventoryUI->cancelHeld();

    inventoryUI->setExternalClickCheck(nullptr);

    // Hide item/count entities
    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        setEntityVisibility(slotItemEntityId[i],  false);
        setEntityVisibility(slotCountEntityId[i], false);
    }

    setPanelVisibility(false);
    visible = false;
    openStorageX = -1;
    openStorageY = -1;
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void StorageUISystem::onProcessEvent(const OnSDLScanCode& event)
{
    if (not visible) return;
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
    if (not visible) return;

    StorageData* storage = storageSystem->getStorage(openStorageX, openStorageY);
    if (not storage)
    {
        close();
        return;
    }

    refreshAllSlots();
}

void StorageUISystem::onProcessEvent(const OnMouseClick& event)
{
    if (not visible or event.button != SDL_BUTTON_LEFT)
        return;

    StorageData* storage = storageSystem->getStorage(openStorageX, openStorageY);
    if (not storage)
        return;

    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        if (isClickOnSlot(i, event.pos.x, event.pos.y))
        {
            auto& slot = storage->inventory.getSlot(i);
            if (inventoryUI->hasHeldItem())
                inventoryUI->dropOnExternal(slot);
            else
                inventoryUI->pickUpFromExternal(slot);
            refreshAllSlots();
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
    if (panelCreated) return;
    createPanel();
    setPanelVisibility(false);
    panelCreated = true;
}

void StorageUISystem::setPanelVisibility(bool vis)
{
    setEntityVisibility(backdropEntityId, vis);
    setEntityVisibility(titleEntityId,    vis);
    for (size_t i = 0; i < NUM_SLOTS; ++i)
        setEntityVisibility(slotBgEntityId[i], vis);
}

void StorageUISystem::createPanel()
{
    float panelX = getPanelX();
    float panelY = getPanelY();
    float panelW = getPanelWidth();
    float panelH = getPanelHeight();

    float slotsStartY = panelY + PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE;

    // Backdrop
    {
        auto bd = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{20.0f, 20.0f, 30.0f, 220.0f});
        auto pos = bd.get<PositionComponent>();
        pos->setX(panelX); pos->setY(panelY); pos->setZ(97.0f);
        pos->setWidth(panelW); pos->setHeight(panelH);
        bd.get<Simple2DObject>()->setViewport(UI_VP);
        backdropEntityId = bd.entity->id;
    }

    // Title
    {
        auto t = makeTTFText(ecsRef,
            panelX + PANEL_PADDING, panelY + PANEL_PADDING + 4.0f, 100.0f,
            FONT_PATH, "Storage", TITLE_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        t.get<TTFText>()->setViewport(UI_VP);
        titleEntityId = t.entity->id;
    }

    // Slot backgrounds + item/count entities
    for (size_t i = 0; i < NUM_SLOTS; ++i)
    {
        size_t col = i % COLS;
        size_t row = i / COLS;

        float sx = panelX + PANEL_PADDING + col * (SLOT_SIZE + SLOT_SPACING);
        float sy = slotsStartY + row * (SLOT_SIZE + SLOT_SPACING);

        cachedSlotX[i] = sx;
        cachedSlotY[i] = sy;

        // Slot background
        auto slotBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
        auto pos = slotBg.get<PositionComponent>();
        pos->setX(sx); pos->setY(sy); pos->setZ(98.0f);
        pos->setWidth(SLOT_SIZE); pos->setHeight(SLOT_SIZE);
        slotBg.get<Simple2DObject>()->setViewport(UI_VP);
        slotBgEntityId[i] = slotBg.entity->id;

        // Item texture
        float offset = (SLOT_SIZE - ITEM_SIZE) * 0.5f;
        auto itemTex = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto iPos = itemTex.get<PositionComponent>();
        iPos->setX(sx + offset); iPos->setY(sy + offset); iPos->setZ(99.0f);
        iPos->setVisibility(false);
        itemTex.get<Texture2DComponent>()->setViewport(UI_VP);
        slotItemEntityId[i] = itemTex.entity->id;

        // Count text
        auto countTxt = makeTTFText(ecsRef,
            sx + SLOT_SIZE - 4.0f, sy + SLOT_SIZE - 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE, {255.0f, 255.0f, 255.0f, 255.0f});
        countTxt.get<PositionComponent>()->setVisibility(false);
        countTxt.get<TTFText>()->setViewport(UI_VP);
        slotCountEntityId[i] = countTxt.entity->id;
    }
}

// ---------------------------------------------------------------------------
// Refresh helpers
// ---------------------------------------------------------------------------

void StorageUISystem::refreshItemSlotDisplay(uint64_t itemEntId, uint64_t countEntId,
                                              const ItemStack& stack)
{
    if (stack.isEmpty())
    {
        setEntityVisibility(itemEntId,  false);
        setEntityVisibility(countEntId, false);
        return;
    }

    const auto& def = itemRegistry->get(stack.id);
    auto itemEnt = ecsRef->getEntity(itemEntId);
    if (itemEnt)
    {
        itemEnt->get<Texture2DComponent>()->setTexture(def.textureName);
        itemEnt->get<PositionComponent>()->setVisibility(true);
    }

    if (stack.count > 1)
    {
        auto countEnt = ecsRef->getEntity(countEntId);
        if (countEnt)
        {
            countEnt->get<TTFText>()->setText(std::to_string(stack.count));
            countEnt->get<PositionComponent>()->setVisibility(true);
        }
    }
    else
    {
        setEntityVisibility(countEntId, false);
    }
}

void StorageUISystem::refreshSlot(size_t slotIndex)
{
    StorageData* storage = storageSystem->getStorage(openStorageX, openStorageY);
    if (not storage) return;

    const auto& stack = storage->inventory.getSlot(slotIndex);
    refreshItemSlotDisplay(slotItemEntityId[slotIndex], slotCountEntityId[slotIndex], stack);
}

void StorageUISystem::refreshAllSlots()
{
    for (size_t i = 0; i < NUM_SLOTS; ++i)
        refreshSlot(i);
}

// ---------------------------------------------------------------------------
// Visibility helper
// ---------------------------------------------------------------------------

void StorageUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}
