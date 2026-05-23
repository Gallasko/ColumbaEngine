#include "recipemachineuibase.h"
#include "craftingui.h"
#include "machinedemosystem.h"
#include "machineuihelpers.h"

#include "UI/prefabspec.h"
#include "UI/prefabbuilder.h"
#include "UI/prefab.h"
#include "2D/simple2dobject.h"
#include "2D/position.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

// ---------------------------------------------------------------------------
// open / close
// ---------------------------------------------------------------------------

void RecipeMachineUIBase::open(int gridX, int gridY, const std::string& machineName)
{
    if (visible)
        close();

    openMachineX    = gridX;
    openMachineY    = gridY;
    openMachineName = machineName.empty() ? machineNameLabel : machineName;
    visible = true;

    // Switch the recipe panel into machine mode for this machine.
    if (auto* craftingUI = ecsRef->getSystem<CraftingUISystem>())
    {
        MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(gridX, gridY);
        const Recipe* locked = machine ? machine->lockedRecipe : nullptr;

        craftingUI->setMachineFeedCallback([this](const Recipe& recipe) {
            feedMachineFromPlayer(recipe);
        });
        craftingUI->setMachineSelectCallback([this](const Recipe& recipe) {
            MachineData* m = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY);
            if (m)
                m->lockedRecipe = &recipe;
        });

        craftingUI->setMachineMode(openMachineName, locked);
    }

    ensurePanelCreated();
    updateForMachineType();
    setPanelVisibility(true);
    syncAllSlots();
    refreshProgressBar();
}

void RecipeMachineUIBase::close()
{
    if (not visible)
        return;

    auto* slotSystem = ecsRef->getSystem<SlotSystem>();
    if (slotSystem->hasHeldItem())
        slotSystem->cancelHeld();

    if (auto* craftingUI = ecsRef->getSystem<CraftingUISystem>())
    {
        craftingUI->setMachineFeedCallback(nullptr);
        craftingUI->setMachineSelectCallback(nullptr);
        craftingUI->clearMachineMode();
    }

    setPanelVisibility(false);
    visible = false;
    openMachineX = -1;
    openMachineY = -1;
    openMachineName.clear();

    auto* inventoryUI = ecsRef->getSystem<InventoryUISystem>();
    if (inventoryUI and inventoryUI->isOpen())
        inventoryUI->closeInventory();
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void RecipeMachineUIBase::onProcessEvent(const OnSDLScanCode& event)
{
    if (not visible)
        return;

    if (event.key == SDL_SCANCODE_ESCAPE)
        close();
}

void RecipeMachineUIBase::onEvent(const InventoryClosedEvent&)
{
    if (visible)
        close();
}

void RecipeMachineUIBase::onProcessEvent(const TickEvent&)
{
    if (not visible)
        return;

    MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY);
    if (not machine)
    {
        close();
        return;
    }

    syncAllSlots();
    refreshProgressBar();
}

void RecipeMachineUIBase::onProcessEvent(const OnMouseClick& event)
{
    if (not visible or event.button != SDL_BUTTON_LEFT)
        return;

    // "?" demo button click — read live position from the button entity.
    if (demoBtnBg and pg_machineui::hitButtonEntity(ecsRef, demoBtnBg.id, event.pos.x, event.pos.y))
    {
        std::string name = openMachineName;
        close();
        ecsRef->getSystem<MachineDemoSystem>()->openDemo(name);
    }
}

// ---------------------------------------------------------------------------
// Panel creation
// ---------------------------------------------------------------------------

void RecipeMachineUIBase::ensurePanelCreated()
{
    if (panelCreated)
        return;

    createPanel();
    setPanelVisibility(false);
    panelCreated = true;
}

void RecipeMachineUIBase::setPanelVisibility(bool vis)
{
    // Everything (slots included) lives inside the prefab tree now, so PrefabSystem's
    // observable-cascade carries the visibility down from the backdrop leaf.

    if (backdrop)
    {
        auto prefab = backdrop->get<PositionComponent>();
        prefab->setVisibility(vis);
    }
}

void RecipeMachineUIBase::updateForMachineType()
{
    // The spec is built with this instance's `numInputs` baked in, so layout values are
    // already correct. The only per-open thing that changes is the title text.
    if (title and title->has<TTFText>())
        title->get<TTFText>()->setText(openMachineName.c_str());
}

void RecipeMachineUIBase::createPanel()
{
    // ---- Precompute layout values (depend on this instance's numInputs) ----
    const int   n         = numInputs;
    const float panelW    = getPanelWidth();
    const float slotsTop  = PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE;
    const float slotsH    = n * SLOT_SIZE + (n - 1) * SLOT_SPACING;
    const float barTop    = slotsTop + slotsH + GAP_AFTER_SLOTS;
    const float panelH    = barTop + PROGRESS_H + PANEL_PADDING;
    const float inputColLeft  = PANEL_PADDING;
    const float outputColLeft = PANEL_PADDING + SLOT_SIZE + ARROW_GAP;
    const float outputSlotTop = (n == 2) ? slotsTop + (SLOT_SIZE + SLOT_SPACING) * 0.5f : slotsTop;
    cachedBarMaxW = panelW - 2.0f * PANEL_PADDING;

    const uint64_t anchorTargetId = pg_machineui::resolveLeftPanelAnchor(ecsRef, ecsRef->getSystem<InventoryUISystem>());
    const std::string initialTitle = openMachineName.empty() ? machineNameLabel : openMachineName;

    // ---- Build the static structure declaratively. ----
    //
    // Outer NodeSpec is the backdrop (Shape2D). Always-wrap turns it into a Prefab container
    // so children can be addressed by name post-build. Sibling structure inside the backdrop:
    //
    //   bg (Shape2D, the leaf -> this node's mainEntity)
    //   ├── title         (TTFText)
    //   ├── inputSlot0    (Slot factory — produces SlotComponent-bearing entity)
    //   ├── inputSlot1    (Slot factory — same)
    //   ├── outputSlot    (Slot factory — same)
    //   ├── progressBg    (Shape2D)
    //   ├── progressFill  (Shape2D, width=0 initially, updated per tick)
    //   └── demoBtnBg     (Shape2D + child demoBtnText) — composite, exposes as wrap
    //
    // The Slot factory accepts category/index/flags via params, so slots come out of the
    // spec fully-functional (SlotComponent attached) — no post-build attach step needed.
    NodeSpec spec;
    spec.kind = "Shape2D";
    spec.name = "bg";
    spec.props = {
        {"width",    panelW},
        {"height",   panelH},
        {"r",        20.0f}, {"g", 20.0f}, {"b", 30.0f}, {"a", 220.0f},
        {"z",        97.0f},
        {"viewport", static_cast<int>(UI_VP)},
    };

    spec.anchors = {
        AnchorSpec{anchorTargetId, AnchorType::Right,          AnchorType::Left,           GAP_BETWEEN_PANELS},
        AnchorSpec{anchorTargetId, AnchorType::VerticalCenter, AnchorType::VerticalCenter, 0.0f},
    };

    {
        NodeSpec t;
        t.kind = "TTFText";
        t.name = "title";
        t.props = {
            {"z",        100.0f},
            {"font",     std::string(FONT_PATH)},
            {"text",     initialTitle},
            {"scale",    TITLE_SCALE},
            {"viewport", static_cast<int>(UI_VP)},
        };
        t.anchors = {
            AnchorSpec{"main", AnchorType::Left, PANEL_PADDING},
            AnchorSpec{"main", AnchorType::Top,  PANEL_PADDING + 4.0f},
        };
        spec.children.push_back(std::move(t));
    }

    // ---- Slots — both input slots (slot 1 hidden below for 1-input machines) and output. ----
    // The Slot factory attaches SlotComponent when `category` is supplied, so these come out
    // of buildNode ready to use.
    const float inputSlotTops[2] = {slotsTop, slotsTop + SLOT_SIZE + SLOT_SPACING};

    auto makeInputSlotNode = [&](int idx) {
        NodeSpec slot;
        slot.kind = "Slot";
        slot.name = (idx == 0) ? "inputSlot0" : "inputSlot1";
        slot.props = {
            {SlotPrefabKeys::SlotSize, SLOT_SIZE},
            {SlotPrefabKeys::ItemSize, ITEM_SIZE},
            {SlotPrefabKeys::Category, std::string("Input")},
            {SlotPrefabKeys::Index,    idx},
        };

        slot.anchors = {
            AnchorSpec{"main", AnchorType::Left, inputColLeft},
            AnchorSpec{"main", AnchorType::Top,  inputSlotTops[idx]},
        };

        return slot;
    };

    spec.children.push_back(makeInputSlotNode(0));
    spec.children.push_back(makeInputSlotNode(1));

    {
        NodeSpec slot;
        slot.kind = "Slot";
        slot.name = "outputSlot";
        slot.props = {
            {SlotPrefabKeys::SlotSize, SLOT_SIZE},
            {SlotPrefabKeys::ItemSize, ITEM_SIZE},
            {SlotPrefabKeys::Category, std::string("Output")},
            {SlotPrefabKeys::Index,    0},
            {SlotPrefabKeys::Flags,    static_cast<int>(SlotFlags::OutputOnly)},
        };

        slot.anchors = {
            AnchorSpec{"main", AnchorType::Left, outputColLeft},
            AnchorSpec{"main", AnchorType::Top,  outputSlotTop},
        };

        spec.children.push_back(std::move(slot));
    }

    {
        NodeSpec bg;
        bg.kind = "Shape2D";
        bg.name = "progressBg";
        bg.props = {
            {"width",    cachedBarMaxW},
            {"height",   PROGRESS_H},
            {"r",        40.0f}, {"g", 40.0f}, {"b", 50.0f}, {"a", 200.0f},
            {"z",        98.0f},
            {"viewport", static_cast<int>(UI_VP)},
        };

        bg.anchors = {
            AnchorSpec{"main", AnchorType::Left, PANEL_PADDING},
            AnchorSpec{"main", AnchorType::Top,  barTop},
        };

        spec.children.push_back(std::move(bg));
    }

    {
        NodeSpec fill;
        fill.kind = "Shape2D";
        fill.name = "progressFill";
        fill.props = {
            {"width",    0.0f},
            {"height",   PROGRESS_H},
            {"r",        80.0f}, {"g", 200.0f}, {"b", 80.0f}, {"a", 255.0f},
            {"z",        99.0f},
            {"viewport", static_cast<int>(UI_VP)},
        };

        fill.anchors = {
            AnchorSpec{"main", AnchorType::Left, PANEL_PADDING},
            AnchorSpec{"main", AnchorType::Top,  barTop},
        };

        spec.children.push_back(std::move(fill));
    }

    {
        // The "?" demo button: a small Shape2D with a TTFText child. Because the button has a
        // child, its wrap is composite and stays addressable via `getEntity("demoBtnBg")`.
        NodeSpec btn;
        btn.kind = "Shape2D";
        btn.name = "demoBtnBg";
        btn.props = {
            {"width",    20.0f},
            {"height",   20.0f},
            {"r",        60.0f}, {"g", 60.0f}, {"b", 100.0f}, {"a", 220.0f},
            {"z",        100.0f},
            {"viewport", static_cast<int>(UI_VP)},
        };

        btn.anchors = {
            AnchorSpec{"main", AnchorType::Right, AnchorType::Right, PANEL_PADDING},
            AnchorSpec{"main", AnchorType::Top,   PANEL_PADDING},
        };

        NodeSpec txt;
        txt.kind = "TTFText";
        txt.name = "demoBtnText";
        txt.props = {
            {"z",        101.0f},
            {"font",     std::string(FONT_PATH)},
            {"text",     std::string("?")},
            {"scale",    0.35f},
            {"r",        255.0f}, {"g", 255.0f}, {"b", 255.0f}, {"a", 255.0f},
            {"viewport", static_cast<int>(UI_VP)},
        };

        // Anchors against this button's own "main" = the demoBtn Shape2D leaf.
        txt.anchors = {
            AnchorSpec{"main", AnchorType::Left, 5.0f},
            AnchorSpec{"main", AnchorType::Top,  2.0f},
        };

        btn.children.push_back(std::move(txt));

        spec.children.push_back(std::move(btn));
    }

    backdrop = buildNode(ecsRef, spec);
    auto prefab = backdrop->get<Prefab>();

    // ---- Cache leaf handles ----
    bgLeaf       = prefab->getEntity("bg");           // outer's mainEntity (Shape2D leaf)
    title        = prefab->getEntity("title");
    progressBg   = prefab->getEntity("progressBg");
    progressFill = prefab->getEntity("progressFill");
    demoBtnBg    = prefab->getEntity("demoBtnBg");    // composite wrap (has demoBtnText child)

    if (demoBtnBg and demoBtnBg->has<Prefab>())
        demoBtnText = demoBtnBg->get<Prefab>()->getEntity("demoBtnText");

    // ---- DEBUG: diagnostic logs for panel-placement issue ----
    // Hypothesis: `anchorTargetId` resolves to an entity without `UiAnchor` (e.g. __MainWindow
    // fallback), so the wrap's spec.anchors are silently skipped, leaving wrap at (0,0). That
    // would explain children appearing at (0+margin, 0+margin).
    {
        auto targetEnt = ecsRef->getEntity(anchorTargetId);
        LOG_INFO("RecipeMachineUI",
            "createPanel: anchorTargetId=" << anchorTargetId
            << " targetExists=" << (targetEnt ? 1 : 0)
            << " targetHasUiAnchor=" << (targetEnt and targetEnt->has<UiAnchor>() ? 1 : 0)
            << " backdrop.id=" << backdrop.id
            << " backdropHasUiAnchor=" << (backdrop->has<UiAnchor>() ? 1 : 0)
            << " backdropHasPrefab="   << (backdrop->has<Prefab>()   ? 1 : 0)
            << " bgLeaf.id=" << bgLeaf.id
            << " bgLeafHasPrefab="     << (bgLeaf and bgLeaf->has<Prefab>() ? 1 : 0));

        auto bp = backdrop->get<PositionComponent>();
        auto bg = bgLeaf->get<PositionComponent>();
        LOG_INFO("RecipeMachineUI",
            "createPanel (post-build, pre-tick): backdrop=("
            << bp->x << "," << bp->y << "," << bp->width << "," << bp->height << ")"
            << " bgLeaf=("
            << (bg ? bg->x : -1) << "," << (bg ? bg->y : -1) << ","
            << (bg ? bg->width : -1) << "," << (bg ? bg->height : -1) << ")");
    }

    // ---- Pre-existing panel-wide click absorber ----
    if (bgLeaf)
        ecsRef->attach<MouseLeftClickComponent>(bgLeaf, makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);

    // ---- Slot handles (slots were built by the Slot factory inside the NodeSpec tree) ----
    inputSlots[0] = prefab->getEntity("inputSlot0");
    inputSlots[1] = prefab->getEntity("inputSlot1");
    outputSlot    = prefab->getEntity("outputSlot");

    // Bind write-back callbacks once. Lambdas re-resolve the machine via the panel's
    // (openMachineX, openMachineY) members each call, so they survive across reopens
    // and tolerate the machine being destroyed (lookup returns null).
    auto* slotSystem = ecsRef->getSystem<SlotSystem>();
    for (int i = 0; i < n; ++i)
    {
        if (not inputSlots[i])
            continue;

        slotSystem->bindSlotChange(inputSlots[i], [this, i](const ItemStack& s) {
            if (auto* m = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY))
                m->inputSlots.getSlot(static_cast<size_t>(i)) = s;
        });
    }
    if (outputSlot)
    {
        slotSystem->bindSlotChange(outputSlot, [this](const ItemStack& s) {
            if (auto* m = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY))
                m->outputSlots.getSlot(0) = s;
        });
    }

    // Hide unused input slot for 1-input machines (separate from panel-wide visibility).
    if (n < 2 and inputSlots[1])
        inputSlots[1]->get<PositionComponent>()->setVisibility(false);
}

// ---------------------------------------------------------------------------
// Sync helpers
// ---------------------------------------------------------------------------

void RecipeMachineUIBase::syncAllSlots()
{
    MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY);
    if (not machine)
        return;

    auto* slotSystem = ecsRef->getSystem<SlotSystem>();
    for (int i = 0; i < numInputs; ++i)
        slotSystem->syncSlotVisual(inputSlots[i].id, machine->inputSlots.getSlot(static_cast<size_t>(i)));

    slotSystem->syncSlotVisual(outputSlot.id, machine->outputSlots.getSlot(0));
}

void RecipeMachineUIBase::refreshProgressBar()
{
    MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY);
    if (not machine)
        return;

    float progress = 0.0f;
    if (machine->currentRecipe and machine->currentRecipe->craftTimeMs > 0)
    {
        progress = static_cast<float>(machine->craftProgress) / static_cast<float>(machine->currentRecipe->craftTimeMs);
        if (progress > 1.0f)
            progress = 1.0f;
    }

    if (progressFill)
        progressFill->get<PositionComponent>()->setWidth(cachedBarMaxW * progress);
}

// ---------------------------------------------------------------------------
// Feed machine from player inventory (double-click callback)
// ---------------------------------------------------------------------------

void RecipeMachineUIBase::feedMachineFromPlayer(const Recipe& recipe)
{
    MachineData* machine = ecsRef->getSystem<CraftingSystem>()->getMachine(openMachineX, openMachineY);
    if (not machine)
        return;

    auto* playerInv = ecsRef->getSystem<PlayerInventorySystem>();
    for (const auto& ing : recipe.inputs)
        if (not playerInv->hasItem(ing.id, ing.count))
            return;

    for (const auto& ing : recipe.inputs)
    {
        playerInv->getInventory().remove(ing.id, ing.count);
        machine->inputSlots.insert(ing.id, ing.count, *itemRegistry);
    }

    syncAllSlots();
    if (auto* inventoryUI = ecsRef->getSystem<InventoryUISystem>())
        inventoryUI->refreshAllSlots();
}
