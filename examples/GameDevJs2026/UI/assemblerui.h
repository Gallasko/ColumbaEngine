#pragma once

#include "recipemachineuibase.h"

// Per-machine UI for the Assembler. 2 input slots stacked + 1 output slot
// (centered between inputs) + progress bar + right-side recipe panel.
//
// Currently a thin configuration over RecipeMachineUIBase. Override hooks
// here when assembler-specific behaviour diverges from the furnace shape.
class AssemblerUI : public RecipeMachineUIBase
{
public:
    AssemblerUI(ItemRegistry* itemRegistry, float screenWidth, float screenHeight)
        : RecipeMachineUIBase(/*numInputs=*/2, "Assembler",
                              itemRegistry, screenWidth, screenHeight) {}

    std::string getSystemName() const override { return "Assembler UI System"; }
};
