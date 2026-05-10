#pragma once

#include "recipemachineuibase.h"

// Per-machine UI for the Furnace. 1 input slot + 1 output slot + progress
// bar + right-side recipe panel.
//
// Currently a thin configuration over RecipeMachineUIBase. Override hooks
// here when furnace-specific behaviour diverges from the assembler shape.
class FurnaceUI : public RecipeMachineUIBase
{
public:
    FurnaceUI(CraftingSystem* craftingSystem, ItemRegistry* itemRegistry,
              PlayerInventorySystem* playerInv, InventoryUISystem* inventoryUI,
              SlotSystem* slotSystem,
              float screenWidth, float screenHeight)
        : RecipeMachineUIBase(/*numInputs=*/1, "Furnace",
                              craftingSystem, itemRegistry,
                              playerInv, inventoryUI, slotSystem,
                              screenWidth, screenHeight) {}

    std::string getSystemName() const override { return "Furnace UI System"; }
};
