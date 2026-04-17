#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"

#include "inventoryui.h"
#include "hotbarsystem.h"
#include "machineui.h"
#include "playerinventory.h"
#include "itemregistry.h"
#include "reciperegistry.h"

#include <cstdint>

using namespace pg;

// Shows a small tooltip after 200ms of hovering over an inventory or hotbar slot.
// The tooltip displays the item name, category, description, and how to obtain it
// (crafting recipe or world mining source).  When the machine UI is open the
// recipe shown is filtered to prefer that machine's recipes.
class TooltipSystem : public System<QueuedListener<OnSDLMouseMotion>,
                                    QueuedListener<TickEvent>>
{
public:
    static constexpr size_t   UI_VP          = 2;
    static constexpr float    TOOLTIP_W      = 220.0f;
    static constexpr float    PADDING        = 8.0f;
    static constexpr float    ICON_SIZE      = 28.0f;
    static constexpr float    LINE_H         = 17.0f;   // approx height per body line
    static constexpr float    NAME_SCALE     = 0.45f;
    static constexpr float    BODY_SCALE     = 0.32f;
    static constexpr uint32_t HOVER_DELAY_MS = 200u;

    static constexpr const char* FONT_PATH =
        "res/font/Inter/static/Inter_28pt-Light.ttf";

    TooltipSystem(InventoryUISystem* inventoryUI,
                  HotbarSystem*      hotbar,
                  MachineUISystem*   machineUI,
                  PlayerInventorySystem* playerInv,
                  ItemRegistry*      itemRegistry,
                  RecipeRegistry*    recipeRegistry,
                  float screenW, float screenH)
        : inventoryUI(inventoryUI), hotbar(hotbar), machineUI(machineUI),
          playerInv(playerInv), itemRegistry(itemRegistry),
          recipeRegistry(recipeRegistry),
          screenWidth(screenW), screenHeight(screenH) {}

    virtual std::string getSystemName() const override { return "Tooltip System"; }

    virtual void onProcessEvent(const OnSDLMouseMotion& event) override;
    virtual void onProcessEvent(const TickEvent& event) override;

private:
    // --- Panel creation -------------------------------------------------

    void ensureCreated();

    // --- Show / hide ----------------------------------------------------

    void showTooltip(ItemId id);
    void hideTooltip();

    // --- Positioning ----------------------------------------------------

    // Compute tooltip height from number of visible body lines.
    float computeHeight(int numBodyLines) const;

    // Place all entities at (tx, ty); resize backdrop to (tooltipW, h).
    void placeAt(float tx, float ty, float h, int numBodyLines);

    // Clamp tooltip to screen and place above cursor.
    void positionTooltip(float cx, float cy, float h);

    // --- Content --------------------------------------------------------

    struct TooltipContent
    {
        std::string name;
        std::string categoryLine;  // e.g. "Resource"
        std::string descLine;      // item description (may be empty)
        std::string sourceLine;    // "Furnace — 2.0s" or "Mining (Stone Pickaxe+)"
        std::string ingrLine1;     // ingredient list, first row
        std::string ingrLine2;     // ingredient list, second row (may be empty)
    };

    TooltipContent buildContent(ItemId id) const;

    static const char* categoryName(ItemCategory cat);
    static const char* machineLabel(uint16_t machineType);  // 0=Hand, 5=Furnace, 6=Assembler

    // --- Helpers --------------------------------------------------------

    void setEntityVisibility(uint64_t id, bool vis);

    // Set text + colour + visibility on a pre-created line entity.
    void setLine(int lineIdx, const std::string& text,
                 constant::Vector4D colour, bool vis);

    // --- Members --------------------------------------------------------

    InventoryUISystem*    inventoryUI    = nullptr;
    HotbarSystem*         hotbar         = nullptr;
    MachineUISystem*      machineUI      = nullptr;
    PlayerInventorySystem* playerInv     = nullptr;
    ItemRegistry*         itemRegistry   = nullptr;
    RecipeRegistry*       recipeRegistry = nullptr;
    float screenWidth  = 0.0f;
    float screenHeight = 0.0f;

    bool     created     = false;
    bool     visible     = false;
    ItemId   hoveredItem = ITEM_NONE;
    ItemId   shownItem   = ITEM_NONE;
    uint32_t hoverStart  = 0;
    float    cursorX     = 0.0f;
    float    cursorY     = 0.0f;

    // How many body lines were used last time showTooltip() ran.
    int lastNumBodyLines = 0;

    // Entity IDs
    uint64_t backdropId = 0;
    uint64_t iconId     = 0;

    // Lines: 0=name, 1=category, 2=description, 3=source, 4=ingr1, 5=ingr2
    static constexpr int NUM_LINES = 6;
    uint64_t lineIds[NUM_LINES] = {};
};
