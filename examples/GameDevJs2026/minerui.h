#pragma once

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"
#include "Input/inputcomponent.h"

#include "minersystem.h"
#include "inventoryui.h"
#include "playerinventory.h"

using namespace pg;

class MinerUISystem : public System<QueuedListener<OnSDLScanCode>,
                                     QueuedListener<TickEvent>,
                                     QueuedListener<OnMouseClick>>
{
public:
    static constexpr size_t UI_VP = 2;
    static constexpr float SLOT_SIZE = 40.0f;
    static constexpr float ITEM_SIZE = 28.0f;
    static constexpr float PANEL_PADDING = 12.0f;
    static constexpr float TEXT_SCALE = 0.3f;
    static constexpr float TITLE_SCALE = 0.4f;
    static constexpr float PROGRESS_BAR_HEIGHT = 8.0f;
    static constexpr float PROGRESS_BAR_WIDTH = SLOT_SIZE;
    static constexpr float GAP_BETWEEN_PANELS = 8.0f;

    static constexpr const char* FONT_PATH = "res/font/Inter/static/Inter_28pt-Light.ttf";

    MinerUISystem(MinerSystem* minerSystem, ItemRegistry* itemRegistry,
                  PlayerInventorySystem* playerInv, InventoryUISystem* inventoryUI,
                  float screenWidth, float screenHeight)
        : minerSystem(minerSystem), itemRegistry(itemRegistry),
          playerInv(playerInv), inventoryUI(inventoryUI),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Miner UI System"; }

    bool isOpen() const { return visible; }

    bool isClickOnPanel(float x, float y) const
    {
        if (not visible) return false;
        float panelX = getPanelX();
        float panelY = getPanelY();
        float panelW = SLOT_SIZE + 2 * PANEL_PADDING;
        float titleH = 20.0f, gapAfterTitle = 6.0f, gapAfterSlot = 8.0f;
        float contentH = titleH + gapAfterTitle + SLOT_SIZE + gapAfterSlot + PROGRESS_BAR_HEIGHT;
        float panelH = contentH + 2 * PANEL_PADDING;
        return x >= panelX and x <= panelX + panelW
           and y >= panelY and y <= panelY + panelH;
    }

    void open(int gridX, int gridY)
    {
        if (visible)
            close();

        openMinerX = gridX;
        openMinerY = gridY;
        visible = true;

        // Also open the player inventory beside us
        if (inventoryUI and not inventoryUI->isOpen())
            inventoryUI->openInventory();

        // Register external click check so inventory doesn't cancel held items
        // when clicking on our panel
        inventoryUI->setExternalClickCheck([this](float x, float y) {
            return isClickOnPanel(x, y);
        });

        ensurePanelCreated();
        setPanelVisibility(true);
        lastDisplayedStack.clear();
        refreshSlot();
    }

    void close()
    {
        // Cancel any held item that came from our slot
        if (inventoryUI and inventoryUI->hasHeldItem())
            inventoryUI->cancelHeld();

        // Unregister external click check
        inventoryUI->setExternalClickCheck(nullptr);

        // Hide item/text entities
        setEntityVisibility(itemEntityId, false);
        setEntityVisibility(countTextEntityId, false);

        // Hide the panel skeleton
        setPanelVisibility(false);
        visible = false;
        openMinerX = -1;
        openMinerY = -1;
        lastDisplayedStack.clear();
    }

    virtual void onProcessEvent(const OnSDLScanCode& event) override
    {
        if (not visible)
            return;

        if (event.key == SDL_SCANCODE_ESCAPE or event.key == SDL_SCANCODE_TAB)
            close();
    }

    virtual void onProcessEvent(const TickEvent&) override
    {
        if (not visible)
            return;

        MinerData* miner = minerSystem->getMiner(openMinerX, openMinerY);
        if (not miner)
        {
            close();
            return;
        }

        const auto& stack = miner->outputSlots.getSlot(0);
        if (stack.id != lastDisplayedStack.id or stack.count != lastDisplayedStack.count)
            refreshSlot();

        refreshProgressBar();
    }

    virtual void onProcessEvent(const OnMouseClick& event) override
    {
        if (not visible or event.button != SDL_BUTTON_LEFT)
            return;

        if (not isClickOnSlot(event.pos.x, event.pos.y))
            return;

        MinerData* miner = minerSystem->getMiner(openMinerX, openMinerY);
        if (not miner)
            return;

        auto& slot = miner->outputSlots.getSlot(0);

        if (inventoryUI->hasHeldItem())
            inventoryUI->dropOnExternal(slot);
        else
            inventoryUI->pickUpFromExternal(slot);

        refreshSlot();
    }

private:
    // Compute panel position: to the left of the inventory panel
    float getPanelX() const
    {
        // Inventory panel dimensions (mirrored from InventoryUISystem constants)
        float invW = InventoryUISystem::COLS * InventoryUISystem::SLOT_SIZE
                   + (InventoryUISystem::COLS - 1) * InventoryUISystem::SLOT_SPACING
                   + 2 * InventoryUISystem::PANEL_PADDING;
        float invX = (screenWidth - invW) * 0.5f;

        float minerW = SLOT_SIZE + 2 * PANEL_PADDING;
        return invX - GAP_BETWEEN_PANELS - minerW;
    }

    float getPanelY() const
    {
        float titleH = 20.0f;
        float gapAfterTitle = 6.0f;
        float gapAfterSlot = 8.0f;
        float contentH = titleH + gapAfterTitle + SLOT_SIZE + gapAfterSlot + PROGRESS_BAR_HEIGHT;
        float panelH = contentH + 2 * PANEL_PADDING;
        return (screenHeight - panelH) * 0.5f;
    }

    void ensurePanelCreated()
    {
        if (panelCreated)
            return;
        createPanel();
        setPanelVisibility(false);
        panelCreated = true;
    }

    void setPanelVisibility(bool vis)
    {
        setEntityVisibility(backdropEntityId, vis);
        setEntityVisibility(titleEntityId, vis);
        setEntityVisibility(slotBgEntityId, vis);
        setEntityVisibility(progressBgEntityId, vis);
        setEntityVisibility(progressFillEntityId, vis);
    }

    void createPanel()
    {
        float titleH = 20.0f;
        float gapAfterTitle = 6.0f;
        float gapAfterSlot = 8.0f;

        float contentH = titleH + gapAfterTitle + SLOT_SIZE + gapAfterSlot + PROGRESS_BAR_HEIGHT;
        float panelW = SLOT_SIZE + 2 * PANEL_PADDING;
        float panelH = contentH + 2 * PANEL_PADDING;
        float panelX = getPanelX();
        float panelY = getPanelY();

        // Backdrop
        auto backdrop = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{20.0f, 20.0f, 30.0f, 220.0f});

        auto bdPos = backdrop.get<PositionComponent>();
        bdPos->setX(panelX);
        bdPos->setY(panelY);
        bdPos->setZ(97.0f);
        bdPos->setWidth(panelW);
        bdPos->setHeight(panelH);
        backdrop.get<Simple2DObject>()->setViewport(UI_VP);
        backdropEntityId = backdrop.entity->id;

        // Title text "Miner" — left-aligned with padding
        auto title = makeTTFText(ecsRef,
            panelX + PANEL_PADDING, panelY + PANEL_PADDING + 4.0f, 100.0f,
            FONT_PATH, "Miner", TITLE_SCALE,
            {255.0f, 255.0f, 255.0f, 255.0f});
        title.get<TTFText>()->setViewport(UI_VP);
        titleEntityId = title.entity->id;

        // Slot background
        float slotX = panelX + PANEL_PADDING;
        float slotY = panelY + PANEL_PADDING + titleH + gapAfterTitle;

        auto slot = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});

        auto slotPos = slot.get<PositionComponent>();
        slotPos->setX(slotX);
        slotPos->setY(slotY);
        slotPos->setZ(98.0f);
        slotPos->setWidth(SLOT_SIZE);
        slotPos->setHeight(SLOT_SIZE);
        slot.get<Simple2DObject>()->setViewport(UI_VP);
        slotBgEntityId = slot.entity->id;

        // Progress bar background
        float barX = slotX;
        float barY = slotY + SLOT_SIZE + gapAfterSlot;

        auto barBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{40.0f, 40.0f, 50.0f, 200.0f});

        auto barBgPos = barBg.get<PositionComponent>();
        barBgPos->setX(barX);
        barBgPos->setY(barY);
        barBgPos->setZ(98.0f);
        barBgPos->setWidth(PROGRESS_BAR_WIDTH);
        barBgPos->setHeight(PROGRESS_BAR_HEIGHT);
        barBg.get<Simple2DObject>()->setViewport(UI_VP);
        progressBgEntityId = barBg.entity->id;

        // Progress bar fill
        auto barFill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{80.0f, 200.0f, 80.0f, 255.0f});

        auto barFillPos = barFill.get<PositionComponent>();
        barFillPos->setX(barX);
        barFillPos->setY(barY);
        barFillPos->setZ(99.0f);
        barFillPos->setWidth(0.0f);
        barFillPos->setHeight(PROGRESS_BAR_HEIGHT);
        barFill.get<Simple2DObject>()->setViewport(UI_VP);
        progressFillEntityId = barFill.entity->id;

        // Item texture entity (hidden by default)
        float itemOffset = (SLOT_SIZE - ITEM_SIZE) * 0.5f;
        auto itemTex = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto itemTexPos = itemTex.get<PositionComponent>();
        itemTexPos->setX(slotX + itemOffset);
        itemTexPos->setY(slotY + itemOffset);
        itemTexPos->setZ(99.0f);
        itemTexPos->setVisibility(false);
        itemTex.get<Texture2DComponent>()->setViewport(UI_VP);
        itemEntityId = itemTex.entity->id;

        // Count text entity (hidden by default)
        auto countText = makeTTFText(ecsRef,
            slotX + SLOT_SIZE - 4.0f, slotY + SLOT_SIZE - 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE,
            {255.0f, 255.0f, 255.0f, 255.0f});
        countText.get<PositionComponent>()->setVisibility(false);
        countText.get<TTFText>()->setViewport(UI_VP);
        countTextEntityId = countText.entity->id;

        // Store layout positions for refresh
        cachedSlotX = slotX;
        cachedSlotY = slotY;
    }

    void refreshSlot()
    {
        MinerData* miner = minerSystem->getMiner(openMinerX, openMinerY);
        if (not miner)
            return;

        const auto& stack = miner->outputSlots.getSlot(0);
        lastDisplayedStack = stack;

        if (stack.isEmpty())
        {
            setEntityVisibility(itemEntityId, false);
            setEntityVisibility(countTextEntityId, false);
            return;
        }

        // Update item texture and show
        const auto& def = itemRegistry->get(stack.id);
        auto itemEnt = ecsRef->getEntity(itemEntityId);
        if (itemEnt)
        {
            itemEnt->get<Texture2DComponent>()->setTexture(def.textureName);
            itemEnt->get<PositionComponent>()->setVisibility(true);
        }

        // Update count text
        if (stack.count > 1)
        {
            auto textEnt = ecsRef->getEntity(countTextEntityId);
            if (textEnt)
            {
                textEnt->get<TTFText>()->setText(std::to_string(stack.count));
                textEnt->get<PositionComponent>()->setVisibility(true);
            }
        }
        else
        {
            setEntityVisibility(countTextEntityId, false);
        }
    }

    void refreshProgressBar()
    {
        MinerData* miner = minerSystem->getMiner(openMinerX, openMinerY);
        if (not miner)
            return;

        float progress = 0.0f;
        if (miner->isMining and MinerSystem::MINE_TIME_MS > 0)
            progress = static_cast<float>(miner->mineProgress) / static_cast<float>(MinerSystem::MINE_TIME_MS);

        if (progress > 1.0f) progress = 1.0f;

        auto fillEnt = ecsRef->getEntity(progressFillEntityId);
        if (fillEnt)
        {
            auto pos = fillEnt->get<PositionComponent>();
            pos->setWidth(PROGRESS_BAR_WIDTH * progress);
        }
    }

    // --- Click Handling ---

    bool isClickOnSlot(float x, float y) const
    {
        return x >= cachedSlotX and x <= cachedSlotX + SLOT_SIZE
           and y >= cachedSlotY and y <= cachedSlotY + SLOT_SIZE;
    }

    // --- Helpers ---

    void setEntityVisibility(uint64_t id, bool vis)
    {
        if (id == 0) return;
        auto ent = ecsRef->getEntity(id);
        if (ent)
            ent->get<PositionComponent>()->setVisibility(vis);
    }

    // --- Members ---

    MinerSystem* minerSystem = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    PlayerInventorySystem* playerInv = nullptr;
    InventoryUISystem* inventoryUI = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    bool visible = false;
    bool panelCreated = false;
    int openMinerX = -1;
    int openMinerY = -1;

    ItemStack lastDisplayedStack;

    // Cached layout positions
    float cachedSlotX = 0.0f;
    float cachedSlotY = 0.0f;

    // Entity IDs
    uint64_t backdropEntityId = 0;
    uint64_t titleEntityId = 0;
    uint64_t slotBgEntityId = 0;
    uint64_t itemEntityId = 0;
    uint64_t countTextEntityId = 0;
    uint64_t progressBgEntityId = 0;
    uint64_t progressFillEntityId = 0;
};
