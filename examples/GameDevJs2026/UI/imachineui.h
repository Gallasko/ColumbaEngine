#pragma once

#include <string>

// Interface implemented by every per-machine UI (FurnaceUI, AssemblerUI,
// MinerUI, DepotUI, StorageUI, ...). The coordinator holds a registry of
// these and routes world clicks to the right one.
//
// Implementations stay independent ECS systems with their own event
// listeners — the interface only formalises the lifecycle hooks the
// coordinator needs to call.

struct MachineUIDescriptor
{
    // The companion player inventory must be visible while this UI is open.
    // Coordinator opens it before calling open() and closes it on close().
    bool requiresInventory = true;

    // The right-side recipe panel (CraftingUISystem) should switch to
    // machine mode showing recipes for this machine name.
    bool wantsRecipePanel = false;

    // The right-side recipe panel must be hidden entirely while this UI
    // is open (Depot uses this — it has its own mission section instead).
    bool suppressesRecipePanel = false;
};

class IMachineUI
{
public:
    virtual ~IMachineUI() = default;

    virtual MachineUIDescriptor descriptor() const = 0;
    virtual void open(int gridX, int gridY, const std::string& machineName) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // Optional: the open machine's display name (e.g. "Furnace"). Tooltip
    // and other queriers can use this. Defaults to empty.
    virtual std::string getOpenMachineName() const { return {}; }
};
