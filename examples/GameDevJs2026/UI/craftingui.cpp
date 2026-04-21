#include "craftingui.h"
#include "machinedemosystem.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "2D/position.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>
#include <cstring>

// Strip common crafting prefixes from recipe names shown in the UI.
static std::string stripCraftingPrefix(const std::string& name)
{
    static const char* prefixes[] = {
        "Hand-Smelt ", "Hand-Craft ", "Smelt ", "Make ", "Craft "
    };
    for (const char* p : prefixes)
    {
        size_t len = std::strlen(p);
        if (name.size() > len && name.compare(0, len, p) == 0)
            return name.substr(len);
    }
    return name;
}

bool CraftingUISystem::isClickOnPanel(float x, float y) const
{
    if (not visible)
        return false;
    auto bdEnt = ecsRef->getEntity(backdropEntityId);
    if (not bdEnt) return false;
    auto bdPos = bdEnt->get<PositionComponent>();
    float px = bdPos->getX();
    float py = bdPos->getY();
    float pw = bdPos->getWidth();
    float ph = bdPos->getHeight();
    return x >= px and x <= px + pw and y >= py and y <= py + ph;
}

void CraftingUISystem::onProcessEvent(const TickEvent&)
{
    if (visible)
    {
        refreshProgressBar();
        // Row "can craft" status can change whenever the inventory
        // contents change (belt deposit, miner pickup, etc.), so
        // refresh each tick — cheap enough for ~6 rows.
        refreshRows();
    }
}

void CraftingUISystem::onEvent(const InventoryOpenedEvent&)
{
    if (not visible)
        open();
}

void CraftingUISystem::onEvent(const InventoryClosedEvent&)
{
    if (visible)
        close();
}

void CraftingUISystem::onEvent(const HandCraftCompletedEvent&)
{
    if (visible)
    {
        refreshRows();
        refreshProgressBar();
    }
    if (inventoryUI)
        inventoryUI->refreshAllSlots();
}

void CraftingUISystem::onProcessEvent(const OnSDLScanCode& event)
{
    if (not visible)
        return;

    if (event.key == SDL_SCANCODE_UP)
        moveSelection(-1);
    else if (event.key == SDL_SCANCODE_DOWN)
        moveSelection(+1);
}

void CraftingUISystem::onProcessEvent(const OnMouseClick& event)
{
    if (not visible or event.button != SDL_BUTTON_LEFT)
        return;

    float x = event.pos.x;
    float y = event.pos.y;

    // Craft / Cancel buttons only active in hand-craft mode
    if (activeMachineName.empty())
    {
        auto craftBtnEnt = ecsRef->getEntity(craftButtonBgEntityId);
        if (craftBtnEnt)
        {
            auto pos = craftBtnEnt->get<PositionComponent>();
            if (isPointInRect(x, y, pos->getX(), pos->getY(), BUTTON_W, BUTTON_H))
            {
                requestCraft();
                return;
            }
        }

        auto cancelBtnEnt = ecsRef->getEntity(cancelButtonBgEntityId);
        if (cancelBtnEnt)
        {
            auto pos = cancelBtnEnt->get<PositionComponent>();
            if (isPointInRect(x, y, pos->getX(), pos->getY(), BUTTON_W, BUTTON_H))
            {
                ecsRef->sendEvent(HandCraftCancel{});
                return;
            }
        }

        // Tab click
        int tabIdx = tabAtPosition(x, y);
        if (tabIdx >= 0)
        {
            setActiveTab(static_cast<CraftTab>(tabIdx));
            return;
        }
    }

    // Per-row "?" demo button click (hand-craft mode)
    if (activeMachineName.empty() and machineDemo)
    {
        for (size_t i = 0; i < rowVisuals.size(); ++i)
        {
            auto demoEnt = ecsRef->getEntity(rowVisuals[i].demoBtnEntityId);
            if (not demoEnt) continue;
            auto demoPos = demoEnt->get<PositionComponent>();
            if (not demoPos->isVisible()) continue;
            // Hit area slightly larger than the "?" text
            if (isPointInRect(x, y, demoPos->getX() - 4.0f, demoPos->getY(),
                              20.0f, ROW_HEIGHT))
            {
                size_t absIdx = scrollOffset + i;
                if (absIdx < visibleRecipes.size())
                {
                    const Recipe& recipe = recipeRegistry->get(visibleRecipes[absIdx]);
                    if (not recipe.outputs.empty())
                    {
                        std::string buildName = itemRegistry->get(recipe.outputs.front().id).buildingName;
                        if (!buildName.empty())
                        {
                            machineDemo->openDemo(buildName);
                            return;
                        }
                    }
                }
            }
        }
    }

    // Scrollbar track click — jump to position + start drag
    if (visibleRecipes.size() > VISIBLE_ROWS)
    {
        auto tr = getScrollTrackRect();
        // Wider hit area (8px padding on each side) for easier clicking
        if (isPointInRect(x, y, tr.x - 8.0f, tr.y, tr.w + 16.0f, tr.h))
        {
            draggingScrollbar = true;
            scrollToTrackY(y);
            return;
        }
    }

    // Row hit test
    int rowIndex = rowAtPosition(x, y);
    if (rowIndex >= 0)
    {
        size_t absIndex = scrollOffset + static_cast<size_t>(rowIndex);
        if (absIndex < visibleRecipes.size())
        {
            uint32_t now = static_cast<uint32_t>(SDL_GetTicks());
            bool isDoubleClick = (!activeMachineName.empty())
                              && (lastClickRowAbs == static_cast<int>(absIndex))
                              && (now - lastClickTime <= 400u);

            // In machine mode: every click locks/selects the recipe
            selectedIndex = absIndex;
            if (!activeMachineName.empty() && machineSelectCallback)
            {
                const Recipe& recipe = recipeRegistry->get(visibleRecipes[absIndex]);
                machineSelectCallback(recipe);
            }

            // Double-click: additionally feed items from player inventory
            if (isDoubleClick && machineFeedCallback)
            {
                const Recipe& recipe = recipeRegistry->get(visibleRecipes[absIndex]);
                machineFeedCallback(recipe);
                lastClickTime   = 0;
                lastClickRowAbs = -1;
            }
            else
            {
                lastClickTime   = now;
                lastClickRowAbs = static_cast<int>(absIndex);
            }

            refreshRows();
        }
    }
}

void CraftingUISystem::onProcessEvent(const OnSDLMouseWheel& event)
{
    if (not visible or not panelCreated or visibleRecipes.empty())
        return;

    int maxOffset = static_cast<int>(visibleRecipes.size()) - static_cast<int>(VISIBLE_ROWS);
    if (maxOffset <= 0)
        return;

    // SDL: event.y > 0 = scroll up = show earlier recipes (decrease offset)
    int newOffset = static_cast<int>(scrollOffset) - event.y;
    if (newOffset < 0) newOffset = 0;
    if (newOffset > maxOffset) newOffset = maxOffset;
    scrollOffset = static_cast<size_t>(newOffset);
    refreshRows();
}

// ---------------------------------------------------------------------------
// setMachineMode / clearMachineMode
// ---------------------------------------------------------------------------

void CraftingUISystem::setMachineMode(const std::string& machineName, const Recipe* initialLocked)
{
    activeMachineName = machineName;
    selectedIndex = 0;
    scrollOffset = 0;
    if (visible)
        applyModeToPanel();

    if (initialLocked)
    {
        for (size_t i = 0; i < visibleRecipes.size(); ++i)
        {
            if (&recipeRegistry->get(visibleRecipes[i]) == initialLocked)
            {
                selectedIndex = i;
                ensureSelectionVisible();
                refreshRows();
                break;
            }
        }
    }
}

void CraftingUISystem::clearMachineMode()
{
    activeMachineName.clear();
    selectedIndex = 0;
    scrollOffset = 0;
    if (visible)
        applyModeToPanel();
}

void CraftingUISystem::applyModeToPanel()
{
    bool handCraft = activeMachineName.empty();

    // Update title text
    if (titleEntityId != 0)
    {
        auto titleEnt = ecsRef->getEntity(titleEntityId);
        if (titleEnt)
        {
            const char* text = (activeMachineName == "Furnace") ? "Furnace" :
                               (activeMachineName == "Assembler") ? "Assembler" : "Craft";
            titleEnt->get<TTFText>()->setText(text);
        }
    }

    // Show/hide hand-craft-only elements (buttons + progress bar + tabs)
    setEntityVisibility(craftButtonBgEntityId,   handCraft);
    setEntityVisibility(craftButtonTextEntityId, handCraft);
    setEntityVisibility(cancelButtonBgEntityId,   handCraft);
    setEntityVisibility(cancelButtonTextEntityId, handCraft);
    setEntityVisibility(progressBgEntityId,       handCraft);
    setEntityVisibility(progressFillEntityId,     handCraft);
    for (auto& tab : tabVisuals)
    {
        setEntityVisibility(tab.bgEntityId, handCraft);
        setEntityVisibility(tab.textEntityId, handCraft);
    }

    if (handCraft)
    {
        activeTab = CraftTab::All;
        refreshTabHighlights();
    }

    rebuildVisibleRecipes();
    refreshRows();
}

// ---------------------------------------------------------------------------
// open / close
// ---------------------------------------------------------------------------

void CraftingUISystem::open()
{
    visible = true;
    ensurePanelCreated();

    if (inventoryUI)
    {
        inventoryUI->setExternalClickCheck(
            [this](float x, float y) { return isClickOnPanel(x, y); });
    }

    setPanelVisibility(true);
    applyModeToPanel();
    refreshProgressBar();
}

void CraftingUISystem::close()
{
    visible = false;
    activeMachineName.clear();
    draggingScrollbar = false;

    if (inventoryUI)
        inventoryUI->setExternalClickCheck(nullptr);

    setPanelVisibility(false);
    hideRowVisuals();
}

float CraftingUISystem::getPanelHeight() const
{
    float listH = VISIBLE_ROWS * ROW_HEIGHT + (VISIBLE_ROWS - 1) * ROW_SPACING;
    return 2 * PANEL_PADDING
         + TITLE_H + GAP_AFTER_TITLE
         + TAB_ROW_H + GAP_AFTER_TABS
         + listH
         + GAP_AFTER_LIST + PROGRESS_BAR_H
         + GAP_AFTER_BAR + BUTTON_H;
}

void CraftingUISystem::ensurePanelCreated()
{
    if (panelCreated)
        return;
    createPanel();
    panelCreated = true;
    setPanelVisibility(false);
}

void CraftingUISystem::setPanelVisibility(bool vis)
{
    setEntityVisibility(backdropEntityId, vis);
    setEntityVisibility(titleEntityId, vis);
    setEntityVisibility(progressBgEntityId, vis);
    setEntityVisibility(progressFillEntityId, vis);
    setEntityVisibility(craftButtonBgEntityId, vis);
    setEntityVisibility(craftButtonTextEntityId, vis);
    setEntityVisibility(cancelButtonBgEntityId, vis);
    setEntityVisibility(cancelButtonTextEntityId, vis);
    for (auto& tab : tabVisuals)
    {
        setEntityVisibility(tab.bgEntityId, vis);
        setEntityVisibility(tab.textEntityId, vis);
    }
    for (auto& row : rowVisuals)
        setEntityVisibility(row.bgEntityId, vis);
    // Item + name visibility is driven by refreshRows()
    // Scrollbar visibility is driven by refreshScrollbar()
    if (not vis)
    {
        hideRowVisuals();
        setEntityVisibility(scrollTrackEntityId, false);
        setEntityVisibility(scrollThumbEntityId, false);
    }
}

void CraftingUISystem::hideRowVisuals()
{
    for (auto& row : rowVisuals)
    {
        setEntityVisibility(row.outputItemEntityId, false);
        setEntityVisibility(row.nameEntityId, false);
        setEntityVisibility(row.demoBtnEntityId, false);
        for (size_t j = 0; j < MAX_INPUTS; ++j)
        {
            setEntityVisibility(row.ingrIconEntityId[j],  false);
            setEntityVisibility(row.ingrCountEntityId[j], false);
        }
    }
}

void CraftingUISystem::createPanel()
{
    auto windowEnt = ecsRef->getEntity("__MainWindow");
    auto windowId  = windowEnt->id;
    uint64_t invBackdropId = inventoryUI->getBackdropEntityId();

    float pw = PANEL_WIDTH;
    float ph = getPanelHeight();
    float rowW = pw - 2 * PANEL_PADDING;

    // Backdrop — anchored to inventory right + gap, vertically centered in window
    auto backdrop = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{20.0f, 20.0f, 30.0f, 220.0f});
    auto bdPos = backdrop.get<PositionComponent>();
    bdPos->setZ(97.0f);
    bdPos->setWidth(pw);
    bdPos->setHeight(ph);
    backdrop.get<ViewportComponent>()->setViewport(UI_VP);
    backdropEntityId = backdrop.entity->id;

    auto bdAnchor = ecsRef->attach<UiAnchor>(backdrop.entity);
    bdAnchor->setLeftAnchor(PosAnchor{invBackdropId, AnchorType::Right});
    bdAnchor->setLeftMargin(GAP_BETWEEN_PANELS);
    bdAnchor->setVerticalCenter(PosAnchor{windowId, AnchorType::VerticalCenter});

    // Title — anchored to backdrop
    auto title = makeTTFText(ecsRef,
        0.0f, 0.0f, 100.0f,
        FONT_PATH, "Craft", TITLE_SCALE,
        {255.0f, 255.0f, 255.0f, 255.0f});
    title.get<ViewportComponent>()->setViewport(UI_VP);
    titleEntityId = title.entity->id;

    auto titleAnchor = ecsRef->attach<UiAnchor>(title.entity);
    titleAnchor->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
    titleAnchor->setLeftMargin(PANEL_PADDING);
    titleAnchor->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
    titleAnchor->setTopMargin(PANEL_PADDING + 4.0f);

    // Tab buttons (between title and recipe list)
    {
        float tabTopMargin = PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE;
        float contentW = pw - 2 * PANEL_PADDING;
        float tabW = (contentW - (TAB_COUNT - 1) * TAB_GAP) / static_cast<float>(TAB_COUNT);

        static const char* tabLabels[TAB_COUNT] = {"All", "Tools", "Mach.", "Misc"};

        for (size_t i = 0; i < TAB_COUNT; ++i)
        {
            float tabLeftMargin = PANEL_PADDING + i * (tabW + TAB_GAP);

            auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
                constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
            auto bgPos = bg.get<PositionComponent>();
            bgPos->setZ(98.0f);
            bgPos->setWidth(tabW);
            bgPos->setHeight(TAB_ROW_H);
            bg.get<ViewportComponent>()->setViewport(UI_VP);
            tabVisuals[i].bgEntityId = bg.entity->id;

            auto tabAnchor = ecsRef->attach<UiAnchor>(bg.entity);
            tabAnchor->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
            tabAnchor->setLeftMargin(tabLeftMargin);
            tabAnchor->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
            tabAnchor->setTopMargin(tabTopMargin);

            auto label = makeTTFText(ecsRef,
                0.0f, 0.0f, 100.0f,
                FONT_PATH, tabLabels[i], TEXT_SCALE,
                {255.0f, 255.0f, 255.0f, 255.0f});
            label.get<ViewportComponent>()->setViewport(UI_VP);
            tabVisuals[i].textEntityId = label.entity->id;

            auto labelAnchor = ecsRef->attach<UiAnchor>(label.entity);
            labelAnchor->setLeftAnchor(PosAnchor{tabVisuals[i].bgEntityId, AnchorType::Left});
            labelAnchor->setLeftMargin(4.0f);
            labelAnchor->setTopAnchor(PosAnchor{tabVisuals[i].bgEntityId, AnchorType::Top});
            labelAnchor->setTopMargin(3.0f);
        }
    }

    // Row skeletons — anchored to backdrop
    float listTopMargin = PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE
                        + TAB_ROW_H + GAP_AFTER_TABS;

    rowVisuals.resize(VISIBLE_ROWS);
    for (size_t i = 0; i < VISIBLE_ROWS; ++i)
    {
        float rowTopMargin = listTopMargin + i * (ROW_HEIGHT + ROW_SPACING);

        // Row background
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
        auto bgPos = bg.get<PositionComponent>();
        bgPos->setZ(98.0f);
        bgPos->setWidth(rowW);
        bgPos->setHeight(ROW_HEIGHT);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        rowVisuals[i].bgEntityId = bg.entity->id;

        auto rowAnchor = ecsRef->attach<UiAnchor>(bg.entity);
        rowAnchor->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        rowAnchor->setLeftMargin(PANEL_PADDING);
        rowAnchor->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        rowAnchor->setTopMargin(rowTopMargin);

        // Output item icon (hidden; positioned in refreshRows)
        auto item = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto itemPos = item.get<PositionComponent>();
        itemPos->setZ(99.0f);
        itemPos->setVisibility(false);
        item.get<ViewportComponent>()->setViewport(UI_VP);
        rowVisuals[i].outputItemEntityId = item.entity->id;

        // Recipe name (hidden; positioned in refreshRows)
        auto name = makeTTFText(ecsRef,
            0.0f, 0.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE,
            {255.0f, 255.0f, 255.0f, 255.0f});
        name.get<ViewportComponent>()->setViewport(UI_VP);
        name.get<PositionComponent>()->setVisibility(false);
        rowVisuals[i].nameEntityId = name.entity->id;

        // Ingredient icon + count text slots (hidden; positioned in refreshRows)
        for (size_t j = 0; j < MAX_INPUTS; ++j)
        {
            auto cnt = makeTTFText(ecsRef,
                0.0f, 0.0f, 100.0f,
                FONT_PATH, "", TEXT_SCALE, {200.0f, 200.0f, 210.0f, 255.0f});
            cnt.get<ViewportComponent>()->setViewport(UI_VP);
            cnt.get<PositionComponent>()->setVisibility(false);
            rowVisuals[i].ingrCountEntityId[j] = cnt.entity->id;

            auto icon = make2DTexture(ecsRef, INGR_ICON_SIZE, INGR_ICON_SIZE, "NoneIcon");
            auto iPos = icon.get<PositionComponent>();
            iPos->setZ(99.0f);
            iPos->setVisibility(false);
            icon.get<ViewportComponent>()->setViewport(UI_VP);
            rowVisuals[i].ingrIconEntityId[j] = icon.entity->id;
        }

        // Per-row "?" demo button (hidden; shown in refreshRows for machine recipes)
        auto demoBtn = makeTTFText(ecsRef,
            0.0f, 0.0f, 100.0f,
            FONT_PATH, "?", TEXT_SCALE,
            {180.0f, 180.0f, 255.0f, 255.0f});
        demoBtn.get<ViewportComponent>()->setViewport(UI_VP);
        demoBtn.get<PositionComponent>()->setVisibility(false);
        rowVisuals[i].demoBtnEntityId = demoBtn.entity->id;
    }

    // Scrollbar (track + thumb) — anchored to backdrop, right edge of list area
    {
        float listH = VISIBLE_ROWS * ROW_HEIGHT + (VISIBLE_ROWS - 1) * ROW_SPACING;

        auto scrollTrack = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{40.0f, 40.0f, 50.0f, 150.0f});
        auto trackPos = scrollTrack.get<PositionComponent>();
        trackPos->setZ(100.0f);
        trackPos->setWidth(SCROLLBAR_WIDTH);
        trackPos->setHeight(listH);
        trackPos->setVisibility(false);
        scrollTrack.get<ViewportComponent>()->setViewport(UI_VP);
        scrollTrackEntityId = scrollTrack.entity->id;

        auto trackAnchor = ecsRef->attach<UiAnchor>(scrollTrack.entity);
        trackAnchor->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
        trackAnchor->setLeftMargin(pw - PANEL_PADDING - SCROLLBAR_WIDTH);
        trackAnchor->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
        trackAnchor->setTopMargin(listTopMargin);

        auto scrollThumb = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{120.0f, 120.0f, 140.0f, 220.0f});
        auto thumbPos = scrollThumb.get<PositionComponent>();
        thumbPos->setZ(101.0f);
        thumbPos->setWidth(SCROLLBAR_WIDTH);
        thumbPos->setHeight(40.0f);
        thumbPos->setVisibility(false);
        scrollThumb.get<ViewportComponent>()->setViewport(UI_VP);
        scrollThumbEntityId = scrollThumb.entity->id;

        auto thumbAnchor = ecsRef->attach<UiAnchor>(scrollThumb.entity);
        thumbAnchor->setLeftAnchor(PosAnchor{scrollTrackEntityId, AnchorType::Left});
        thumbAnchor->setTopAnchor(PosAnchor{scrollTrackEntityId, AnchorType::Top});
        thumbAnchor->setTopMargin(0.0f);
    }

    // Progress bar — anchored to backdrop
    float barTopMargin = listTopMargin + VISIBLE_ROWS * ROW_HEIGHT
                       + (VISIBLE_ROWS - 1) * ROW_SPACING + GAP_AFTER_LIST;

    auto barBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{40.0f, 40.0f, 50.0f, 220.0f});
    auto barBgPos = barBg.get<PositionComponent>();
    barBgPos->setZ(98.0f);
    barBgPos->setWidth(rowW);
    barBgPos->setHeight(PROGRESS_BAR_H);
    barBg.get<ViewportComponent>()->setViewport(UI_VP);
    progressBgEntityId = barBg.entity->id;

    auto barBgAnchor = ecsRef->attach<UiAnchor>(barBg.entity);
    barBgAnchor->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
    barBgAnchor->setLeftMargin(PANEL_PADDING);
    barBgAnchor->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
    barBgAnchor->setTopMargin(barTopMargin);

    auto barFill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{80.0f, 200.0f, 80.0f, 255.0f});
    auto barFillPos = barFill.get<PositionComponent>();
    barFillPos->setZ(99.0f);
    barFillPos->setWidth(0.0f);
    barFillPos->setHeight(PROGRESS_BAR_H);
    barFill.get<ViewportComponent>()->setViewport(UI_VP);
    progressFillEntityId = barFill.entity->id;

    auto barFillAnchor = ecsRef->attach<UiAnchor>(barFill.entity);
    barFillAnchor->setLeftAnchor(PosAnchor{progressBgEntityId, AnchorType::Left});
    barFillAnchor->setTopAnchor(PosAnchor{progressBgEntityId, AnchorType::Top});

    // Buttons — anchored to backdrop
    float buttonsTopMargin = barTopMargin + PROGRESS_BAR_H + GAP_AFTER_BAR;

    auto craftBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{70.0f, 130.0f, 80.0f, 240.0f});
    auto craftBgPos = craftBg.get<PositionComponent>();
    craftBgPos->setZ(98.0f);
    craftBgPos->setWidth(BUTTON_W);
    craftBgPos->setHeight(BUTTON_H);
    craftBg.get<ViewportComponent>()->setViewport(UI_VP);
    craftButtonBgEntityId = craftBg.entity->id;

    auto craftBgAnchor = ecsRef->attach<UiAnchor>(craftBg.entity);
    craftBgAnchor->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
    craftBgAnchor->setLeftMargin(PANEL_PADDING);
    craftBgAnchor->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
    craftBgAnchor->setTopMargin(buttonsTopMargin);

    auto craftText = makeTTFText(ecsRef,
        0.0f, 0.0f, 100.0f,
        FONT_PATH, "Craft", TEXT_SCALE,
        {255.0f, 255.0f, 255.0f, 255.0f});
    craftText.get<ViewportComponent>()->setViewport(UI_VP);
    craftButtonTextEntityId = craftText.entity->id;

    auto craftTextAnchor = ecsRef->attach<UiAnchor>(craftText.entity);
    craftTextAnchor->setLeftAnchor(PosAnchor{craftButtonBgEntityId, AnchorType::Left});
    craftTextAnchor->setLeftMargin(18.0f);
    craftTextAnchor->setTopAnchor(PosAnchor{craftButtonBgEntityId, AnchorType::Top});
    craftTextAnchor->setTopMargin(5.0f);

    auto cancelBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{130.0f, 70.0f, 70.0f, 240.0f});
    auto cancelBgPos = cancelBg.get<PositionComponent>();
    cancelBgPos->setZ(98.0f);
    cancelBgPos->setWidth(BUTTON_W);
    cancelBgPos->setHeight(BUTTON_H);
    cancelBg.get<ViewportComponent>()->setViewport(UI_VP);
    cancelButtonBgEntityId = cancelBg.entity->id;

    auto cancelBgAnchor = ecsRef->attach<UiAnchor>(cancelBg.entity);
    cancelBgAnchor->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
    cancelBgAnchor->setLeftMargin(PANEL_PADDING + BUTTON_W + BUTTON_GAP);
    cancelBgAnchor->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
    cancelBgAnchor->setTopMargin(buttonsTopMargin);

    auto cancelText = makeTTFText(ecsRef,
        0.0f, 0.0f, 100.0f,
        FONT_PATH, "Cancel", TEXT_SCALE,
        {255.0f, 255.0f, 255.0f, 255.0f});
    cancelText.get<ViewportComponent>()->setViewport(UI_VP);
    cancelButtonTextEntityId = cancelText.entity->id;

    auto cancelTextAnchor = ecsRef->attach<UiAnchor>(cancelText.entity);
    cancelTextAnchor->setLeftAnchor(PosAnchor{cancelButtonBgEntityId, AnchorType::Left});
    cancelTextAnchor->setLeftMargin(16.0f);
    cancelTextAnchor->setTopAnchor(PosAnchor{cancelButtonBgEntityId, AnchorType::Top});
    cancelTextAnchor->setTopMargin(5.0f);

    refreshTabHighlights();
}

void CraftingUISystem::rebuildVisibleRecipes()
{
    visibleRecipes.clear();

    if (!activeMachineName.empty())
    {
        // Machine mode: show all recipes for this machine type
        RecipeCategory cat = (activeMachineName == "Furnace") ? RecipeCategory::Furnace
                                                              : RecipeCategory::Assembler;
        for (size_t i = 0; i < recipeRegistry->count(); ++i)
        {
            const Recipe& r = recipeRegistry->get(i);
            if (r.category == cat and r.machineName == activeMachineName)
                visibleRecipes.push_back(i);
        }
    }
    else
    {
        // Hand-craft mode
        for (size_t i = 0; i < recipeRegistry->count(); ++i)
        {
            const Recipe& r = recipeRegistry->get(i);
            if (r.category != RecipeCategory::HandCraft)
                continue;
            if (not handCrafting->isUnlocked(r))
                continue;
            if (activeTab != CraftTab::All and classifyRecipe(r) != activeTab)
                continue;
            visibleRecipes.push_back(i);
        }
    }

    if (visibleRecipes.empty())
    {
        selectedIndex = 0;
        scrollOffset = 0;
        return;
    }

    if (selectedIndex >= visibleRecipes.size())
        selectedIndex = visibleRecipes.size() - 1;
    ensureSelectionVisible();
}

void CraftingUISystem::moveSelection(int delta)
{
    if (visibleRecipes.empty())
        return;

    int s = static_cast<int>(selectedIndex) + delta;
    if (s < 0) s = 0;
    if (s >= static_cast<int>(visibleRecipes.size()))
        s = static_cast<int>(visibleRecipes.size()) - 1;
    selectedIndex = static_cast<size_t>(s);

    ensureSelectionVisible();
    refreshRows();
}

void CraftingUISystem::ensureSelectionVisible()
{
    if (visibleRecipes.empty())
        return;

    if (selectedIndex < scrollOffset)
        scrollOffset = selectedIndex;
    else if (selectedIndex >= scrollOffset + VISIBLE_ROWS)
        scrollOffset = selectedIndex - VISIBLE_ROWS + 1;
}

void CraftingUISystem::refreshRows()
{
    // Rebuild the recipe list in case unlock conditions changed.
    size_t prevSelected = selectedIndex;
    rebuildVisibleRecipes();
    if (visibleRecipes.empty())
    {
        for (auto& row : rowVisuals)
        {
            setEntityVisibility(row.outputItemEntityId, false);
            setEntityVisibility(row.nameEntityId, false);
            setEntityVisibility(row.demoBtnEntityId, false);
            for (size_t j = 0; j < MAX_INPUTS; ++j)
            {
                setEntityVisibility(row.ingrIconEntityId[j],  false);
                setEntityVisibility(row.ingrCountEntityId[j], false);
            }
            tintRow(row.bgEntityId, RowTint::Idle);
        }
        return;
    }
    if (prevSelected != selectedIndex)
        ensureSelectionVisible();

    for (size_t rowIdx = 0; rowIdx < VISIBLE_ROWS; ++rowIdx)
    {
        size_t absIdx = scrollOffset + rowIdx;
        auto& row = rowVisuals[rowIdx];

        if (absIdx >= visibleRecipes.size())
        {
            setEntityVisibility(row.outputItemEntityId, false);
            setEntityVisibility(row.nameEntityId, false);
            setEntityVisibility(row.demoBtnEntityId, false);
            for (size_t j = 0; j < MAX_INPUTS; ++j)
            {
                setEntityVisibility(row.ingrIconEntityId[j],  false);
                setEntityVisibility(row.ingrCountEntityId[j], false);
            }
            tintRow(row.bgEntityId, RowTint::Idle);
            continue;
        }

        // Read row bg position (auto-updated by anchoring)
        auto rowBgEnt = ecsRef->getEntity(row.bgEntityId);
        if (not rowBgEnt) continue;
        auto rowBgPos = rowBgEnt->get<PositionComponent>();
        float rowX = rowBgPos->getX();
        float rowY = rowBgPos->getY();

        size_t recipeIdx = visibleRecipes[absIdx];
        const Recipe& recipe = recipeRegistry->get(recipeIdx);

        // Output icon: first output item.
        if (not recipe.outputs.empty())
        {
            const auto& outDef = itemRegistry->get(recipe.outputs.front().id);
            auto itemEnt = ecsRef->getEntity(row.outputItemEntityId);
            if (itemEnt)
            {
                itemEnt->get<Texture2DComponent>()->setTexture(outDef.textureName);
                auto pos = itemEnt->get<PositionComponent>();
                float iconW = ITEM_SIZE * outDef.iconWidthRatio;
                float baseX = rowX + 6.0f;
                float baseY = rowY + (ROW_HEIGHT - ITEM_SIZE) * 0.5f;
                pos->setWidth(iconW);
                pos->setHeight(ITEM_SIZE);
                pos->setX(baseX + (ITEM_SIZE - iconW) * 0.5f);
                pos->setY(baseY);
                pos->setVisibility(true);
            }
        }
        else
        {
            setEntityVisibility(row.outputItemEntityId, false);
        }

        // Recipe name (strip redundant "Craft "/"Smelt "/etc. prefix)
        float textAreaX = rowX + 6.0f + ITEM_SIZE + 6.0f;
        auto nameEnt = ecsRef->getEntity(row.nameEntityId);
        if (nameEnt)
        {
            nameEnt->get<TTFText>()->setText(stripCraftingPrefix(recipe.name));
            auto namePos = nameEnt->get<PositionComponent>();
            namePos->setX(textAreaX);
            namePos->setY(rowY + 4.0f);
            namePos->setVisibility(true);
        }

        // Ingredient icons + count labels
        float ingrY = rowY + ROW_HEIGHT - INGR_ICON_SIZE - 3.0f;
        for (size_t j = 0; j < MAX_INPUTS; ++j)
        {
            if (j < recipe.inputs.size())
            {
                const auto& in  = recipe.inputs[j];
                const auto& def = itemRegistry->get(in.id);
                float ix = textAreaX + static_cast<float>(j) * INGR_SLOT_W;

                auto cntEnt = ecsRef->getEntity(row.ingrCountEntityId[j]);
                if (cntEnt)
                {
                    cntEnt->get<TTFText>()->setText(std::to_string(in.count) + "x");
                    auto cntPos = cntEnt->get<PositionComponent>();
                    cntPos->setX(ix);
                    cntPos->setY(ingrY + 2.0f);
                    cntPos->setVisibility(true);
                }

                auto iconEnt = ecsRef->getEntity(row.ingrIconEntityId[j]);
                if (iconEnt)
                {
                    iconEnt->get<Texture2DComponent>()->setTexture(def.textureName);
                    auto iPos = iconEnt->get<PositionComponent>();
                    iPos->setX(ix + 22.0f);
                    iPos->setY(ingrY);
                    iPos->setVisibility(true);
                }
            }
            else
            {
                setEntityVisibility(row.ingrIconEntityId[j],  false);
                setEntityVisibility(row.ingrCountEntityId[j], false);
            }
        }

        // Background tint
        RowTint tint = RowTint::Idle;
        if (activeMachineName.empty())
        {
            // Hand-craft mode: colour by can-craft status
            if (absIdx == selectedIndex)
                tint = handCrafting->canCraft(recipe) ? RowTint::SelectedOk : RowTint::SelectedBad;
            else if (handCrafting->canCraft(recipe))
                tint = RowTint::CanCraft;
            else
                tint = RowTint::Insufficient;
        }
        else
        {
            // Machine mode: just highlight selected
            tint = (absIdx == selectedIndex) ? RowTint::SelectedOk : RowTint::Idle;
        }
        tintRow(row.bgEntityId, tint);

        // Per-row "?" demo button (hand-craft mode only, for machine recipes with a demo)
        bool showDemo = false;
        if (activeMachineName.empty() and machineDemo and not recipe.outputs.empty())
        {
            std::string buildName = itemRegistry->get(recipe.outputs.front().id).buildingName;
            if (!buildName.empty() and machineDemo->hasDemoForTile(buildName))
                showDemo = true;
        }
        if (showDemo)
        {
            auto demoEnt = ecsRef->getEntity(row.demoBtnEntityId);
            if (demoEnt)
            {
                float rowW = rowBgPos->getWidth();
                auto demoPos = demoEnt->get<PositionComponent>();
                demoPos->setX(rowX + rowW - 14.0f - SCROLLBAR_WIDTH - 4.0f);
                demoPos->setY(rowY + 4.0f);
                demoPos->setVisibility(true);
            }
        }
        else
        {
            setEntityVisibility(row.demoBtnEntityId, false);
        }
    }

    refreshScrollbar();
}

void CraftingUISystem::tintRow(uint64_t bgId, RowTint tint)
{
    auto ent = ecsRef->getEntity(bgId);
    if (not ent)
        return;

    constant::Vector4D c;
    switch (tint)
    {
    case RowTint::CanCraft:     c = {70.0f,  110.0f, 80.0f,  220.0f}; break;
    case RowTint::Insufficient: c = {60.0f,  60.0f,  70.0f,  200.0f}; break;
    case RowTint::SelectedOk:   c = {90.0f,  180.0f, 110.0f, 240.0f}; break;
    case RowTint::SelectedBad:  c = {130.0f, 90.0f,  90.0f,  240.0f}; break;
    case RowTint::Idle:
    default:                    c = {50.0f,  50.0f,  60.0f,  200.0f}; break;
    }
    ent->get<Simple2DObject>()->setColors(c);
}

void CraftingUISystem::refreshProgressBar()
{
    if (!activeMachineName.empty())
        return; // Progress bar is hidden in machine mode

    auto fillEnt = ecsRef->getEntity(progressFillEntityId);
    if (not fillEnt)
        return;
    auto barBgEnt = ecsRef->getEntity(progressBgEntityId);
    float barMaxW = barBgEnt ? barBgEnt->get<PositionComponent>()->getWidth() : 0.0f;
    float ratio = handCrafting ? handCrafting->getProgressRatio() : 0.0f;
    fillEnt->get<PositionComponent>()->setWidth(barMaxW * ratio);
}

void CraftingUISystem::refreshScrollbar()
{
    size_t totalRecipes = visibleRecipes.size();
    bool scrollable = totalRecipes > VISIBLE_ROWS;

    setEntityVisibility(scrollTrackEntityId, visible and scrollable);
    setEntityVisibility(scrollThumbEntityId, visible and scrollable);

    if (not scrollable)
        return;

    float listH = VISIBLE_ROWS * ROW_HEIGHT + (VISIBLE_ROWS - 1) * ROW_SPACING;

    float thumbH = (static_cast<float>(VISIBLE_ROWS) / static_cast<float>(totalRecipes)) * listH;
    if (thumbH < 16.0f)
        thumbH = 16.0f;

    size_t maxOffset = totalRecipes - VISIBLE_ROWS;
    float trackTravel = listH - thumbH;
    float thumbY = (static_cast<float>(scrollOffset) / static_cast<float>(maxOffset)) * trackTravel;

    auto thumbEnt = ecsRef->getEntity(scrollThumbEntityId);
    if (thumbEnt)
    {
        thumbEnt->get<PositionComponent>()->setHeight(thumbH);
        thumbEnt->get<UiAnchor>()->setTopMargin(thumbY);
    }
}

// ---------------------------------------------------------------------------
// Tab helpers
// ---------------------------------------------------------------------------

CraftingUISystem::CraftTab CraftingUISystem::classifyRecipe(const Recipe& recipe) const
{
    if (recipe.outputs.empty())
        return CraftTab::Misc;

    const auto& outDef = itemRegistry->get(recipe.outputs.front().id);

    if (outDef.toolTier > 0)
        return CraftTab::Tools;

    if (!outDef.buildingName.empty())
        return CraftTab::Machines;

    return CraftTab::Misc;
}

void CraftingUISystem::setActiveTab(CraftTab tab)
{
    if (tab == activeTab)
        return;

    activeTab = tab;
    selectedIndex = 0;
    scrollOffset = 0;

    refreshTabHighlights();
    rebuildVisibleRecipes();
    refreshRows();
}

int CraftingUISystem::tabAtPosition(float x, float y) const
{
    for (size_t i = 0; i < TAB_COUNT; ++i)
    {
        auto tabEnt = ecsRef->getEntity(tabVisuals[i].bgEntityId);
        if (not tabEnt) continue;
        auto pos = tabEnt->get<PositionComponent>();
        float tx = pos->getX();
        float ty = pos->getY();
        float tw = pos->getWidth();
        if (x >= tx and x <= tx + tw and y >= ty and y <= ty + TAB_ROW_H)
            return static_cast<int>(i);
    }
    return -1;
}

void CraftingUISystem::refreshTabHighlights()
{
    for (size_t i = 0; i < TAB_COUNT; ++i)
    {
        auto ent = ecsRef->getEntity(tabVisuals[i].bgEntityId);
        if (not ent)
            continue;

        constant::Vector4D c;
        if (static_cast<size_t>(activeTab) == i)
            c = {80.0f, 140.0f, 200.0f, 240.0f};   // Active: bright blue
        else
            c = {50.0f, 50.0f, 60.0f, 200.0f};      // Inactive: default dark

        ent->get<Simple2DObject>()->setColors(c);
    }
}

void CraftingUISystem::requestCraft()
{
    if (visibleRecipes.empty() or selectedIndex >= visibleRecipes.size())
        return;
    if (handCrafting->isActive())
        return;
    size_t recipeIdx = visibleRecipes[selectedIndex];
    const Recipe& recipe = recipeRegistry->get(recipeIdx);
    if (not handCrafting->canCraft(recipe))
        return;
    ecsRef->sendEvent(HandCraftRequest{recipeIdx});
}

int CraftingUISystem::rowAtPosition(float x, float y) const
{
    for (size_t i = 0; i < rowVisuals.size(); ++i)
    {
        auto rowEnt = ecsRef->getEntity(rowVisuals[i].bgEntityId);
        if (not rowEnt) continue;
        auto pos = rowEnt->get<PositionComponent>();
        float rx = pos->getX();
        float ry = pos->getY();
        float rw = pos->getWidth();
        if (x >= rx and x <= rx + rw and y >= ry and y <= ry + ROW_HEIGHT)
            return static_cast<int>(i);
    }
    return -1;
}

void CraftingUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}

// ---------------------------------------------------------------------------
// Scrollbar drag helpers
// ---------------------------------------------------------------------------

CraftingUISystem::TrackRect CraftingUISystem::getScrollTrackRect() const
{
    auto bdEnt = ecsRef->getEntity(backdropEntityId);
    if (not bdEnt)
        return {0, 0, 0, 0};
    auto bdPos = bdEnt->get<PositionComponent>();
    float bx = bdPos->getX();
    float by = bdPos->getY();

    float listTopMargin = PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE
                        + TAB_ROW_H + GAP_AFTER_TABS;
    float listH = VISIBLE_ROWS * ROW_HEIGHT + (VISIBLE_ROWS - 1) * ROW_SPACING;

    return { bx + PANEL_WIDTH - PANEL_PADDING - SCROLLBAR_WIDTH,
             by + listTopMargin,
             SCROLLBAR_WIDTH,
             listH };
}

void CraftingUISystem::scrollToTrackY(float mouseY)
{
    auto tr = getScrollTrackRect();
    if (tr.h <= 0.0f)
        return;

    float ratio = (mouseY - tr.y) / tr.h;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    size_t maxOffset = visibleRecipes.size() - VISIBLE_ROWS;
    scrollOffset = static_cast<size_t>(ratio * static_cast<float>(maxOffset) + 0.5f);
    if (scrollOffset > maxOffset)
        scrollOffset = maxOffset;
    refreshRows();
}

void CraftingUISystem::onProcessEvent(const OnMouseMove& event)
{
    if (not draggingScrollbar or not visible)
        return;
    scrollToTrackY(event.pos.y);
}

void CraftingUISystem::onProcessEvent(const OnMouseRelease& event)
{
    draggingScrollbar = false;
}
