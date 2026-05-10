#pragma once

#include "Systems/basicsystems.h"

#include "imachineui.h"
#include "inventoryui.h"

#include <string>
#include <unordered_map>

class CraftingUISystem;

using namespace pg;

// Centralised orchestration for every machine UI in the game. World code
// (gamesystem.cpp) calls openMachineUI(gx, gy, tileName) — the coordinator
// looks up the registered IMachineUI for that machine name and runs the
// common open/close flow:
//
//  - close any other UI that's already open
//  - open the player inventory when the UI requires it
//  - suppress the right-side recipe panel for UIs that own their own
//    sidebar (Depot)
//  - call ui->open(...) / ui->close()
//
// Adding a new machine type = implement IMachineUI and call registerUI()
// once during startup.
class MachineUICoordinator : public System<Listener<InventoryClosedEvent>>
{
public:
    MachineUICoordinator(InventoryUISystem* inventoryUI,
                         CraftingUISystem*  craftingUI)
        : inventoryUI(inventoryUI), craftingUI(craftingUI) {}

    std::string getSystemName() const override { return "Machine UI Coordinator"; }

    void registerUI(const std::string& machineName, IMachineUI* ui)
    {
        if (ui)
            registry[machineName] = ui;
    }

    // Returns false if no UI is registered for the given machine name.
    bool openMachineUI(int gridX, int gridY, const std::string& machineName);

    // Idempotent. Triggered on ESC, on InventoryClosedEvent, or directly.
    void closeMachineUI();

    bool isAnyOpen() const { return activeUI != nullptr; }
    IMachineUI* getActiveUI() const { return activeUI; }
    std::string getOpenMachineName() const
    {
        return activeUI ? activeUI->getOpenMachineName() : std::string{};
    }

    void onEvent(const InventoryClosedEvent&) override
    {
        if (activeUI)
            closeMachineUI();
    }

private:
    InventoryUISystem* inventoryUI = nullptr;
    CraftingUISystem*  craftingUI  = nullptr;

    std::unordered_map<std::string, IMachineUI*> registry;
    IMachineUI* activeUI = nullptr;

    // Tracks whether THIS coordinator suppressed the recipe panel on open
    // so close() only un-suppresses what it itself toggled.
    bool suppressedRecipePanel = false;
};
