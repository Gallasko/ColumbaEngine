#include "craftingui.h"
#include "machinedemosystem.h"

#include "2D/simple2dobject.h"
#include "UI/sizer.h"
#include "2D/texture.h"
#include "2D/position.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>
#include <cstring>
#include <unordered_set>

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
    if (not visible and not suppressed)
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
    if (auto* inventoryUI = ecsRef->getSystem<InventoryUISystem>())
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
    if (activeMachineName.empty())
    {
        if (auto* machineDemo = ecsRef->getSystem<MachineDemoSystem>())
        {
            for (size_t i = 0; i < visibleRecipes.size(); ++i)
            {
                size_t recipeIdx = visibleRecipes[i];
                auto demoEnt = ecsRef->getEntity(rowVisuals[recipeIdx].demoBtnEntityId);
                if (not demoEnt)
                    continue;

                auto demoPos = demoEnt->get<PositionComponent>();
                if (not demoPos->isVisible())
                    continue;

                // Hit area slightly larger than the "?" text
                if (isPointInRect(x, y, demoPos->getX() - 4.0f, demoPos->getY(), 20.0f, ROW_HEIGHT))
                {
                    const Recipe& recipe = recipeRegistry->get(recipeIdx);
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

    // Row hit test
    int rowIndex = rowAtPosition(x, y);
    if (rowIndex >= 0)
    {
        size_t absIndex = static_cast<size_t>(rowIndex);
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

// ---------------------------------------------------------------------------
// setMachineMode / clearMachineMode
// ---------------------------------------------------------------------------

void CraftingUISystem::setMachineMode(const std::string& machineName, const Recipe* initialLocked)
{
    activeMachineName = machineName;
    selectedIndex = 0;
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
    ensureSelectionVisible();
    refreshRows();
}

// ---------------------------------------------------------------------------
// open / close
// ---------------------------------------------------------------------------

void CraftingUISystem::open()
{
    visible = true;
    ensurePanelCreated();

    setPanelVisibility(true);
    applyModeToPanel();
    refreshProgressBar();
}

void CraftingUISystem::close()
{
    if (not visible)
        return;

    visible = false;
    activeMachineName.clear();

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
    setEntityVisibility(recipeLayoutEntityId, vis);
    // Row bg + item + name visibility is driven by rebuildVisibleRecipes() / refreshRows()
    if (not vis)
        hideRowVisuals();
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
    uint64_t invBackdropId = ecsRef->getSystem<InventoryUISystem>()->getBackdropEntityId();

    float pw = PANEL_WIDTH;
    float ph = getPanelHeight();
    float rowW = pw - 2 * PANEL_PADDING;

    // Backdrop via the engine "Panel" factory — anchored to inventory's right
    // edge + gap, vertically centered in window.
    auto* factory = ecsRef->getSystem<PrefabFactoryRegistry>();
    auto panelEnt = factory->build("Panel", PrefabParams{
        {"width", pw}, {"height", ph},
        {"r", 20.0f}, {"g", 20.0f}, {"b", 30.0f}, {"a", 220.0f},
        {"z", 97.0f},
        {"viewport", static_cast<int>(UI_VP)},
    });
    backdropEntityId = panelEnt->id;

    if (auto bgEnt = panelEnt->get<Prefab>()->getEntity("bg"))
    {
        ecsRef->attach<MouseLeftClickComponent>(bgEnt,
            makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);
        ecsRef->attach<EntityName>(bgEnt, "CraftingPanel");
    }

    auto bdAnchor = panelEnt->get<UiAnchor>();
    bdAnchor->setLeftAnchor(PosAnchor{invBackdropId, AnchorType::Right});
    bdAnchor->setLeftMargin(GAP_BETWEEN_PANELS);
    bdAnchor->setVerticalCenter(PosAnchor{windowId, AnchorType::VerticalCenter});

    // Title via the engine "Text" factory — anchored to backdrop.
    auto titleEnt = factory->build("Text", PrefabParams{
        {"x", 0.0f}, {"y", 0.0f}, {"z", 100.0f},
        {"font",     std::string(FONT_PATH)},
        {"text",     std::string("Craft")},
        {"scale",    TITLE_SCALE},
        {"viewport", static_cast<int>(UI_VP)},
    });
    titleEntityId = titleEnt->id;

    auto titleAnchor = titleEnt->get<UiAnchor>();
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

    // Scrollable vertical layout for recipe rows
    float listTopMargin = PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE
                        + TAB_ROW_H + GAP_AFTER_TABS;
    float listH = VISIBLE_ROWS * ROW_HEIGHT + (VISIBLE_ROWS - 1) * ROW_SPACING;

    auto layout = makeVerticalLayout(ecsRef, 0, 0, rowW, listH, true);
    recipeLayoutEntityId = layout.entity->id;
    auto vLayout = layout.get<VerticalLayout>();
    vLayout->spacing = ROW_SPACING;
    vLayout->scrollSpeed = ROW_HEIGHT + ROW_SPACING;
    ecsRef->attach<ViewportComponent>(layout.entity)->setViewport(UI_VP);
    layout.get<PositionComponent>()->setZ(101.0f);
    ecsRef->attach<MouseLeftClickComponent>(layout.entity,
        makeCallable<PanelWasClickedEvent>(), MouseStateTrigger::OnPress);

    auto layoutAnchor = layout.get<UiAnchor>();
    layoutAnchor->setLeftAnchor(PosAnchor{backdropEntityId, AnchorType::Left});
    layoutAnchor->setLeftMargin(PANEL_PADDING);
    layoutAnchor->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
    layoutAnchor->setTopMargin(listTopMargin);

    // Scrollbar thumb — positioned by layout's verticalScrollBar
    {
        auto scrollThumb = makeUiSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{120.0f, 120.0f, 140.0f, 220.0f});
        auto thumbPos = scrollThumb.get<PositionComponent>();
        thumbPos->setZ(101.0f);
        thumbPos->setWidth(SCROLLBAR_WIDTH);
        thumbPos->setHeight(40.0f);
        thumbPos->setVisibility(false);
        scrollThumb.get<ViewportComponent>()->setViewport(UI_VP);

        vLayout->setVerticalScrollBar(scrollThumb.entity);
    }

    // One row per recipe
    size_t totalRecipes = recipeRegistry->count();
    rowVisuals.resize(totalRecipes);
    for (size_t i = 0; i < totalRecipes; ++i)
    {
        // Row background — positioned by layout
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
        auto bgPos = bg.get<PositionComponent>();
        bgPos->setZ(98.0f);
        bgPos->setWidth(rowW);
        bgPos->setHeight(ROW_HEIGHT);
        bgPos->setVisibility(false);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        rowVisuals[i].bgEntityId = bg.entity->id;

        vLayout->addEntity(bg.entity);

        // Output item icon — anchored to row bg
        auto item = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto itemPos = item.get<PositionComponent>();
        itemPos->setZ(99.0f);
        itemPos->setVisibility(false);
        item.get<ViewportComponent>()->setViewport(UI_VP);
        rowVisuals[i].outputItemEntityId = item.entity->id;
        ecsRef->attach<ClippedTo>(item.entity, recipeLayoutEntityId);

        auto itemAnchor = ecsRef->attach<UiAnchor>(item.entity);
        itemAnchor->setLeftAnchor(PosAnchor{rowVisuals[i].bgEntityId, AnchorType::Left});
        itemAnchor->setLeftMargin(6.0f);
        itemAnchor->setVerticalCenter(PosAnchor{rowVisuals[i].bgEntityId, AnchorType::VerticalCenter});

        // Recipe name — anchored to row bg
        auto name = makeTTFText(ecsRef,
            0.0f, 0.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE,
            {255.0f, 255.0f, 255.0f, 255.0f});
        name.get<ViewportComponent>()->setViewport(UI_VP);
        name.get<PositionComponent>()->setVisibility(false);
        rowVisuals[i].nameEntityId = name.entity->id;
        ecsRef->attach<ClippedTo>(name.entity, recipeLayoutEntityId);

        auto nameAnchor = ecsRef->attach<UiAnchor>(name.entity);
        nameAnchor->setLeftAnchor(PosAnchor{rowVisuals[i].bgEntityId, AnchorType::Left});
        nameAnchor->setLeftMargin(6.0f + ITEM_SIZE + 6.0f);
        nameAnchor->setTopAnchor(PosAnchor{rowVisuals[i].bgEntityId, AnchorType::Top});
        nameAnchor->setTopMargin(4.0f);

        // Ingredient icon + count text slots — anchored to row bg
        float ingrTopOffset = ROW_HEIGHT - INGR_ICON_SIZE - 3.0f;
        for (size_t j = 0; j < MAX_INPUTS; ++j)
        {
            float ingrLeftOffset = 6.0f + ITEM_SIZE + 6.0f + static_cast<float>(j) * INGR_SLOT_W;

            auto cnt = makeTTFText(ecsRef,
                0.0f, 0.0f, 100.0f,
                FONT_PATH, "", TEXT_SCALE, {200.0f, 200.0f, 210.0f, 255.0f});
            cnt.get<ViewportComponent>()->setViewport(UI_VP);
            cnt.get<PositionComponent>()->setVisibility(false);
            rowVisuals[i].ingrCountEntityId[j] = cnt.entity->id;
            ecsRef->attach<ClippedTo>(cnt.entity, recipeLayoutEntityId);

            auto cntAnchor = ecsRef->attach<UiAnchor>(cnt.entity);
            cntAnchor->setLeftAnchor(PosAnchor{rowVisuals[i].bgEntityId, AnchorType::Left});
            cntAnchor->setLeftMargin(ingrLeftOffset);
            cntAnchor->setTopAnchor(PosAnchor{rowVisuals[i].bgEntityId, AnchorType::Top});
            cntAnchor->setTopMargin(ingrTopOffset + 2.0f);

            auto icon = make2DTexture(ecsRef, INGR_ICON_SIZE, INGR_ICON_SIZE, "NoneIcon");
            auto iPos = icon.get<PositionComponent>();
            iPos->setZ(99.0f);
            iPos->setVisibility(false);
            icon.get<ViewportComponent>()->setViewport(UI_VP);
            rowVisuals[i].ingrIconEntityId[j] = icon.entity->id;
            ecsRef->attach<ClippedTo>(icon.entity, recipeLayoutEntityId);

            auto iconAnchor = ecsRef->attach<UiAnchor>(icon.entity);
            iconAnchor->setLeftAnchor(PosAnchor{rowVisuals[i].bgEntityId, AnchorType::Left});
            iconAnchor->setLeftMargin(ingrLeftOffset + 22.0f);
            iconAnchor->setTopAnchor(PosAnchor{rowVisuals[i].bgEntityId, AnchorType::Top});
            iconAnchor->setTopMargin(ingrTopOffset);
        }

        // Per-row "?" demo button — anchored to row bg
        auto demoBtn = makeTTFText(ecsRef,
            0.0f, 0.0f, 100.0f,
            FONT_PATH, "?", TEXT_SCALE,
            {180.0f, 180.0f, 255.0f, 255.0f});
        demoBtn.get<ViewportComponent>()->setViewport(UI_VP);
        demoBtn.get<PositionComponent>()->setVisibility(false);
        rowVisuals[i].demoBtnEntityId = demoBtn.entity->id;
        ecsRef->attach<ClippedTo>(demoBtn.entity, recipeLayoutEntityId);

        auto demoAnchor = ecsRef->attach<UiAnchor>(demoBtn.entity);
        demoAnchor->setLeftAnchor(PosAnchor{rowVisuals[i].bgEntityId, AnchorType::Left});
        demoAnchor->setLeftMargin(rowW - 14.0f - SCROLLBAR_WIDTH - 4.0f);
        demoAnchor->setTopAnchor(PosAnchor{rowVisuals[i].bgEntityId, AnchorType::Top});
        demoAnchor->setTopMargin(4.0f);
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

    auto* handCrafting = ecsRef->getSystem<HandCraftingSystem>();

    if (!activeMachineName.empty())
    {
        // Machine mode: show recipes for this machine type, gated by unlock conditions
        RecipeCategory cat = (activeMachineName == "Furnace") ? RecipeCategory::Furnace
                                                              : RecipeCategory::Assembler;
        for (size_t i = 0; i < recipeRegistry->count(); ++i)
        {
            const Recipe& r = recipeRegistry->get(i);
            if (r.category != cat or r.machineName != activeMachineName)
                continue;
            if (handCrafting and not handCrafting->isUnlocked(r))
                continue;
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

    // Build lookup set for O(1) visibility check
    std::unordered_set<size_t> visibleSet(visibleRecipes.begin(), visibleRecipes.end());

    // Show/hide row containers — layout skips invisible children
    for (size_t i = 0; i < rowVisuals.size(); ++i)
    {
        bool vis = visibleSet.count(i) > 0;
        setEntityVisibility(rowVisuals[i].bgEntityId, vis);
        if (not vis)
        {
            setEntityVisibility(rowVisuals[i].outputItemEntityId, false);
            setEntityVisibility(rowVisuals[i].nameEntityId, false);
            setEntityVisibility(rowVisuals[i].demoBtnEntityId, false);
            for (size_t j = 0; j < MAX_INPUTS; ++j)
            {
                setEntityVisibility(rowVisuals[i].ingrIconEntityId[j],  false);
                setEntityVisibility(rowVisuals[i].ingrCountEntityId[j], false);
            }
        }
    }

    if (visibleRecipes.empty())
    {
        selectedIndex = 0;
        return;
    }

    if (selectedIndex >= visibleRecipes.size())
    {
        selectedIndex = visibleRecipes.size() - 1;
        ensureSelectionVisible();
    }
}

void CraftingUISystem::moveSelection(int delta)
{
    if (visibleRecipes.empty())
        return;

    int s = static_cast<int>(selectedIndex) + delta;
    if (s < 0)
        s = 0;
    if (s >= static_cast<int>(visibleRecipes.size()))
        s = static_cast<int>(visibleRecipes.size()) - 1;
    selectedIndex = static_cast<size_t>(s);

    ensureSelectionVisible();
    refreshRows();
}

void CraftingUISystem::ensureSelectionVisible()
{
    if (visibleRecipes.empty() or recipeLayoutEntityId == 0)
        return;

    auto layoutEnt = ecsRef->getEntity(recipeLayoutEntityId);
    if (not layoutEnt)
        return;
    auto vLayout = layoutEnt->get<VerticalLayout>();

    float viewH = VISIBLE_ROWS * ROW_HEIGHT + (VISIBLE_ROWS - 1) * ROW_SPACING;
    float itemTop = static_cast<float>(selectedIndex) * (ROW_HEIGHT + ROW_SPACING);
    float itemBottom = itemTop + ROW_HEIGHT;

    if (itemTop < vLayout->yOffset)
        vLayout->yOffset = itemTop;
    else if (itemBottom > vLayout->yOffset + viewH)
        vLayout->yOffset = itemBottom - viewH;

    ecsRef->sendEvent(LayoutScrolledEvent{recipeLayoutEntityId});
}

void CraftingUISystem::refreshRows()
{
    // Rebuild the recipe list in case unlock conditions changed.
    size_t prevSelected = selectedIndex;
    rebuildVisibleRecipes();
    if (visibleRecipes.empty())
        return;
    if (prevSelected != selectedIndex)
        ensureSelectionVisible();

    auto* handCrafting = ecsRef->getSystem<HandCraftingSystem>();
    auto* machineDemo  = ecsRef->getSystem<MachineDemoSystem>();

    for (size_t visIdx = 0; visIdx < visibleRecipes.size(); ++visIdx)
    {
        size_t recipeIdx = visibleRecipes[visIdx];
        auto& row = rowVisuals[recipeIdx];
        const Recipe& recipe = recipeRegistry->get(recipeIdx);

        // Output icon (position handled by anchor, just update texture + visibility)
        if (not recipe.outputs.empty())
        {
            const auto& outDef = itemRegistry->get(recipe.outputs.front().id);
            auto itemEnt = ecsRef->getEntity(row.outputItemEntityId);
            if (itemEnt)
            {
                itemEnt->get<Texture2DComponent>()->setTexture(outDef.textureName);
                auto pos = itemEnt->get<PositionComponent>();
                float iconW = ITEM_SIZE * outDef.iconWidthRatio;
                pos->setWidth(iconW);
                pos->setHeight(ITEM_SIZE);
                pos->setVisibility(true);
            }
        }
        else
        {
            setEntityVisibility(row.outputItemEntityId, false);
        }

        // Recipe name (position handled by anchor)
        auto nameEnt = ecsRef->getEntity(row.nameEntityId);
        if (nameEnt)
        {
            nameEnt->get<TTFText>()->setText(stripCraftingPrefix(recipe.name));
            nameEnt->get<PositionComponent>()->setVisibility(true);
        }

        // Ingredient icons + count labels (positions handled by anchors)
        for (size_t j = 0; j < MAX_INPUTS; ++j)
        {
            if (j < recipe.inputs.size())
            {
                const auto& in  = recipe.inputs[j];
                const auto& def = itemRegistry->get(in.id);

                auto cntEnt = ecsRef->getEntity(row.ingrCountEntityId[j]);
                if (cntEnt)
                {
                    cntEnt->get<TTFText>()->setText(std::to_string(in.count) + "x");
                    cntEnt->get<PositionComponent>()->setVisibility(true);
                }

                auto iconEnt = ecsRef->getEntity(row.ingrIconEntityId[j]);
                if (iconEnt)
                {
                    iconEnt->get<Texture2DComponent>()->setTexture(def.textureName);
                    iconEnt->get<PositionComponent>()->setVisibility(true);
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
            if (visIdx == selectedIndex)
                tint = handCrafting->canCraft(recipe) ? RowTint::SelectedOk : RowTint::SelectedBad;
            else if (handCrafting->canCraft(recipe))
                tint = RowTint::CanCraft;
            else
                tint = RowTint::Insufficient;
        }
        else
        {
            tint = (visIdx == selectedIndex) ? RowTint::SelectedOk : RowTint::Idle;
        }
        tintRow(row.bgEntityId, tint);

        // Per-row "?" demo button (hand-craft mode only)
        bool showDemo = false;
        if (activeMachineName.empty() and machineDemo and not recipe.outputs.empty())
        {
            std::string buildName = itemRegistry->get(recipe.outputs.front().id).buildingName;
            if (!buildName.empty() and machineDemo->hasDemoForTile(buildName))
                showDemo = true;
        }
        setEntityVisibility(row.demoBtnEntityId, showDemo);
    }
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
    auto* handCrafting = ecsRef->getSystem<HandCraftingSystem>();
    float ratio = handCrafting ? handCrafting->getProgressRatio() : 0.0f;
    fillEnt->get<PositionComponent>()->setWidth(barMaxW * ratio);
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

    refreshTabHighlights();
    rebuildVisibleRecipes();
    ensureSelectionVisible();
    refreshRows();
}

int CraftingUISystem::tabAtPosition(float x, float y) const
{
    for (size_t i = 0; i < TAB_COUNT; ++i)
    {
        auto tabEnt = ecsRef->getEntity(tabVisuals[i].bgEntityId);
        if (not tabEnt)
            continue;
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

    auto* handCrafting = ecsRef->getSystem<HandCraftingSystem>();
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
    for (size_t i = 0; i < visibleRecipes.size(); ++i)
    {
        size_t recipeIdx = visibleRecipes[i];
        auto rowEnt = ecsRef->getEntity(rowVisuals[recipeIdx].bgEntityId);
        if (not rowEnt)
            continue;

        auto pos = rowEnt->get<PositionComponent>();
        if (not pos->isObservable())
            continue;

        if (x >= pos->x and x <= pos->x + pos->width and
            y >= pos->y and y <= pos->y + ROW_HEIGHT)
            return static_cast<int>(i);
    }

    return -1;
}

void CraftingUISystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0)
        return;

    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}

