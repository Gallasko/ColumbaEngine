#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "handcraftingsystem.h"
#include "inventoryui.h"
#include "playerinventory.h"
#include "reciperegistry.h"
#include "worldfacts.h"

using namespace pg;

// Docked side-panel (right of the inventory) that lists the player's
// unlocked hand-craft recipes, lets them pick one, and shows a progress
// bar while a craft is in-flight.
// In machine mode (setMachineMode) it shows that machine's recipes instead.
class CraftingUISystem : public System<InitSys,
                                       QueuedListener<OnSDLScanCode>,
                                       QueuedListener<OnMouseClick>,
                                       QueuedListener<TickEvent>,
                                       QueuedListener<OnSDLMouseWheel>,
                                       Listener<HandCraftCompletedEvent>,
                                       Listener<InventoryOpenedEvent>,
                                       Listener<InventoryClosedEvent>>
{
public:
    static constexpr size_t UI_VP = 2;
    static constexpr size_t VISIBLE_ROWS = 6;
    static constexpr float PANEL_PADDING = 12.0f;
    static constexpr float TITLE_H = 22.0f;
    static constexpr float GAP_AFTER_TITLE = 6.0f;
    static constexpr float ROW_HEIGHT = 40.0f;
    static constexpr float ROW_SPACING = 4.0f;
    static constexpr float ITEM_SIZE = 28.0f;
    static constexpr float PANEL_WIDTH = 210.0f;
    static constexpr float GAP_AFTER_LIST = 10.0f;
    static constexpr float PROGRESS_BAR_H = 10.0f;
    static constexpr float GAP_AFTER_BAR = 8.0f;
    static constexpr float BUTTON_H = 26.0f;
    static constexpr float BUTTON_W = 80.0f;
    static constexpr float BUTTON_GAP = 8.0f;
    static constexpr float GAP_BETWEEN_PANELS = 8.0f;

    static constexpr size_t MAX_INPUTS    = 3;
    static constexpr float INGR_ICON_SIZE = 16.0f;
    static constexpr float INGR_SLOT_W   = 40.0f;

    static constexpr float TEXT_SCALE = 0.28f;
    static constexpr float TITLE_SCALE = 0.38f;

    static constexpr const char* FONT_PATH = "res/font/Inter/static/Inter_28pt-Light.ttf";

    CraftingUISystem(HandCraftingSystem* handCrafting,
                     RecipeRegistry* recipeRegistry,
                     ItemRegistry* itemRegistry,
                     PlayerInventorySystem* playerInv,
                     pg::WorldFacts* worldFacts,
                     InventoryUISystem* inventoryUI,
                     float screenWidth, float screenHeight)
        : handCrafting(handCrafting), recipeRegistry(recipeRegistry),
          itemRegistry(itemRegistry), playerInv(playerInv),
          worldFacts(worldFacts), inventoryUI(inventoryUI),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Crafting UI System"; }

    bool isOpen() const { return visible; }

    bool isClickOnPanel(float x, float y) const;

    void init() override {}

    // Switch the recipe list to show recipes for the given machine type.
    // Call before openInventory() so it takes effect when the panel opens.
    // Also safe to call while the panel is already open.
    void setMachineMode(uint16_t tileId);
    void clearMachineMode();

    // --- Inventory sync ------------------------------------------------

    virtual void onProcessEvent(const TickEvent&) override;
    virtual void onEvent(const HandCraftCompletedEvent&) override;
    virtual void onEvent(const InventoryOpenedEvent&) override;
    virtual void onEvent(const InventoryClosedEvent&) override;
    virtual void onProcessEvent(const OnSDLScanCode& event) override;
    virtual void onProcessEvent(const OnMouseClick& event) override;
    virtual void onProcessEvent(const OnSDLMouseWheel& event) override;

private:
    // --- Open / close --------------------------------------------------

    void open();
    void close();

    // --- Layout --------------------------------------------------------

    float getPanelX() const;
    float getPanelY() const;
    float getPanelHeight() const;

    // --- Panel creation ------------------------------------------------

    void ensurePanelCreated();
    void setPanelVisibility(bool vis);
    void hideRowVisuals();
    void createPanel();

    // --- List management -----------------------------------------------

    void rebuildVisibleRecipes();
    void moveSelection(int delta);
    void ensureSelectionVisible();

    // --- Mode visuals --------------------------------------------------

    // Apply title / button / progress-bar visibility for the current
    // activeMachineType, then rebuild and refresh the recipe list.
    void applyModeToPanel();

    // --- Rendering -----------------------------------------------------

    void refreshRows();

    enum class RowTint
    {
        Idle,
        CanCraft,
        Insufficient,
        SelectedOk,
        SelectedBad
    };

    void tintRow(uint64_t bgId, RowTint tint);
    void refreshProgressBar();

    // --- Craft request -------------------------------------------------

    void requestCraft();

    // --- Hit testing ---------------------------------------------------

    int rowAtPosition(float x, float y) const;

    static bool isPointInRect(float x, float y, float rx, float ry, float rw, float rh)
    {
        return x >= rx and x <= rx + rw and y >= ry and y <= ry + rh;
    }

    // --- Helpers -------------------------------------------------------

    void setEntityVisibility(uint64_t id, bool vis);

    // --- Members -------------------------------------------------------

    HandCraftingSystem* handCrafting = nullptr;
    RecipeRegistry* recipeRegistry = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    PlayerInventorySystem* playerInv = nullptr;
    pg::WorldFacts* worldFacts = nullptr;
    InventoryUISystem* inventoryUI = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    bool visible = false;
    bool panelCreated = false;

    // 0 = hand-craft mode; 5 = Furnace; 6 = Assembler
    uint16_t activeMachineType = 0;

    std::vector<size_t> visibleRecipes; // Indices into recipeRegistry->recipes
    size_t selectedIndex = 0;
    size_t scrollOffset = 0;

    struct RowVisual
    {
        uint64_t bgEntityId         = 0;
        uint64_t outputItemEntityId = 0;
        uint64_t nameEntityId       = 0;
        std::array<uint64_t, MAX_INPUTS> ingrIconEntityId  = {};
        std::array<uint64_t, MAX_INPUTS> ingrCountEntityId = {};
    };
    std::vector<RowVisual> rowVisuals;

    uint64_t backdropEntityId = 0;
    uint64_t titleEntityId = 0;
    uint64_t progressBgEntityId = 0;
    uint64_t progressFillEntityId = 0;
    uint64_t craftButtonBgEntityId = 0;
    uint64_t craftButtonTextEntityId = 0;
    uint64_t cancelButtonBgEntityId = 0;
    uint64_t cancelButtonTextEntityId = 0;

    float craftButtonX = 0.0f, craftButtonY = 0.0f;
    float cancelButtonX = 0.0f, cancelButtonY = 0.0f;
    float cachedBarX = 0.0f, cachedBarMaxW = 0.0f;
    float cachedListX = 0.0f, cachedListY = 0.0f, cachedRowW = 0.0f;
};
