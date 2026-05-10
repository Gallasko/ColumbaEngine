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
    FurnaceUI(ItemRegistry* itemRegistry, float screenWidth, float screenHeight)
        : RecipeMachineUIBase(/*numInputs=*/1, "Furnace",
                              itemRegistry, screenWidth, screenHeight) {}

    std::string getSystemName() const override { return "Furnace UI System"; }
};
