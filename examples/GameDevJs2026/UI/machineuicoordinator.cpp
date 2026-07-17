#include "machineuicoordinator.h"
#include "craftingui.h"

bool MachineUICoordinator::openMachineUI(int gridX, int gridY, const std::string& machineName)
{
    auto it = registry.find(machineName);
    if (it == registry.end())
        return false;

    IMachineUI* ui = it->second;
    if (not ui)
        return false;

    // Switch from a different UI cleanly.
    if (activeUI and activeUI != ui)
        closeMachineUI();

    const MachineUIDescriptor desc = ui->descriptor();

    auto* inventoryUI = ecsRef->getSystem<InventoryUISystem>();
    if (desc.requiresInventory and inventoryUI and not inventoryUI->isOpen())
        inventoryUI->openInventory();

    if (desc.suppressesRecipePanel)
    {
        if (auto* craftingUI = ecsRef->getSystem<CraftingUISystem>())
        {
            craftingUI->setSuppressed(true);
            if (craftingUI->isOpen())
                craftingUI->close();
            suppressedRecipePanel = true;
        }
    }

    activeUI = ui;
    ui->open(gridX, gridY, machineName);
    return true;
}

void MachineUICoordinator::closeMachineUI()
{
    if (not activeUI)
        return;

    // Clear activeUI BEFORE running close so the InventoryClosedEvent that
    // close() may trigger doesn't re-enter this function.
    IMachineUI* prev = activeUI;
    activeUI = nullptr;

    prev->close();

    if (suppressedRecipePanel)
    {
        if (auto* craftingUI = ecsRef->getSystem<CraftingUISystem>())
            craftingUI->setSuppressed(false);
        suppressedRecipePanel = false;
    }
}
