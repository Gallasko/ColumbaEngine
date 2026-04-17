#include "craftingui.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
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
    float px = getPanelX();
    float py = getPanelY();
    float pw = PANEL_WIDTH;
    float ph = getPanelHeight();
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
    if (activeMachineType == 0)
    {
        if (isPointInRect(x, y, craftButtonX, craftButtonY, BUTTON_W, BUTTON_H))
        {
            requestCraft();
            return;
        }

        if (isPointInRect(x, y, cancelButtonX, cancelButtonY, BUTTON_W, BUTTON_H))
        {
            ecsRef->sendEvent(HandCraftCancel{});
            return;
        }

        // Tab click
        int tabIdx = tabAtPosition(x, y);
        if (tabIdx >= 0)
        {
            setActiveTab(static_cast<CraftTab>(tabIdx));
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
            bool isDoubleClick = (activeMachineType != 0)
                              && (lastClickRowAbs == static_cast<int>(absIndex))
                              && (now - lastClickTime <= 400u);

            // In machine mode: every click locks/selects the recipe
            selectedIndex = absIndex;
            if (activeMachineType != 0 && machineSelectCallback)
            {
                const Recipe& recipe = recipeRegistry->recipes[visibleRecipes[absIndex]];
                machineSelectCallback(recipe);
            }

            // Double-click: additionally feed items from player inventory
            if (isDoubleClick && machineFeedCallback)
            {
                const Recipe& recipe = recipeRegistry->recipes[visibleRecipes[absIndex]];
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

void CraftingUISystem::setMachineMode(uint16_t tileId, const Recipe* initialLocked)
{
    activeMachineType = tileId;
    selectedIndex = 0;
    scrollOffset = 0;
    if (visible)
        applyModeToPanel();

    if (initialLocked)
    {
        for (size_t i = 0; i < visibleRecipes.size(); ++i)
        {
            if (&recipeRegistry->recipes[visibleRecipes[i]] == initialLocked)
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
    activeMachineType = 0;
    selectedIndex = 0;
    scrollOffset = 0;
    if (visible)
        applyModeToPanel();
}

void CraftingUISystem::applyModeToPanel()
{
    bool handCraft = (activeMachineType == 0);

    // Update title text
    if (titleEntityId != 0)
    {
        auto titleEnt = ecsRef->getEntity(titleEntityId);
        if (titleEnt)
        {
            const char* text = (activeMachineType == 5) ? "Furnace" :
                               (activeMachineType == 6) ? "Assembler" : "Craft";
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
    activeMachineType = 0;

    if (inventoryUI)
        inventoryUI->setExternalClickCheck(nullptr);

    setPanelVisibility(false);
    hideRowVisuals();
}

float CraftingUISystem::getPanelX() const
{
    float invW = InventoryUISystem::COLS * InventoryUISystem::SLOT_SIZE
               + (InventoryUISystem::COLS - 1) * InventoryUISystem::SLOT_SPACING
               + 2 * InventoryUISystem::PANEL_PADDING;
    float invX = (screenWidth - invW) * 0.5f;
    return invX + invW + GAP_BETWEEN_PANELS;
}

float CraftingUISystem::getPanelY() const
{
    float ph = getPanelHeight();
    return (screenHeight - ph) * 0.5f;
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
    if (not vis)
        hideRowVisuals();
}

void CraftingUISystem::hideRowVisuals()
{
    for (auto& row : rowVisuals)
    {
        setEntityVisibility(row.outputItemEntityId, false);
        setEntityVisibility(row.nameEntityId, false);
        for (size_t j = 0; j < MAX_INPUTS; ++j)
        {
            setEntityVisibility(row.ingrIconEntityId[j],  false);
            setEntityVisibility(row.ingrCountEntityId[j], false);
        }
    }
}

void CraftingUISystem::createPanel()
{
    float px = getPanelX();
    float py = getPanelY();
    float pw = PANEL_WIDTH;
    float ph = getPanelHeight();

    // Backdrop
    auto backdrop = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{20.0f, 20.0f, 30.0f, 220.0f});
    auto bdPos = backdrop.get<PositionComponent>();
    bdPos->setX(px);
    bdPos->setY(py);
    bdPos->setZ(97.0f);
    bdPos->setWidth(pw);
    bdPos->setHeight(ph);
    backdrop.get<Simple2DObject>()->setViewport(UI_VP);
    backdropEntityId = backdrop.entity->id;

    // Title
    auto title = makeTTFText(ecsRef,
        px + PANEL_PADDING, py + PANEL_PADDING + 4.0f, 100.0f,
        FONT_PATH, "Craft", TITLE_SCALE,
        {255.0f, 255.0f, 255.0f, 255.0f});
    title.get<TTFText>()->setViewport(UI_VP);
    titleEntityId = title.entity->id;

    // Tab buttons (between title and recipe list)
    {
        float tabRowY = py + PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE;
        cachedTabY = tabRowY;
        float contentW = pw - 2 * PANEL_PADDING;
        float tabW = (contentW - (TAB_COUNT - 1) * TAB_GAP) / static_cast<float>(TAB_COUNT);

        static const char* tabLabels[TAB_COUNT] = {"All", "Tools", "Mach.", "Misc"};

        for (size_t i = 0; i < TAB_COUNT; ++i)
        {
            float tx = px + PANEL_PADDING + i * (tabW + TAB_GAP);

            auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
                constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
            auto bgPos = bg.get<PositionComponent>();
            bgPos->setX(tx);
            bgPos->setY(tabRowY);
            bgPos->setZ(98.0f);
            bgPos->setWidth(tabW);
            bgPos->setHeight(TAB_ROW_H);
            bg.get<Simple2DObject>()->setViewport(UI_VP);
            tabVisuals[i].bgEntityId = bg.entity->id;

            auto label = makeTTFText(ecsRef,
                tx + 4.0f, tabRowY + 3.0f, 100.0f,
                FONT_PATH, tabLabels[i], TEXT_SCALE,
                {255.0f, 255.0f, 255.0f, 255.0f});
            label.get<TTFText>()->setViewport(UI_VP);
            tabVisuals[i].textEntityId = label.entity->id;
        }
    }

    // Row skeletons
    float listX = px + PANEL_PADDING;
    float listY = py + PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE
                + TAB_ROW_H + GAP_AFTER_TABS;
    float rowW = pw - 2 * PANEL_PADDING;

    rowVisuals.resize(VISIBLE_ROWS);
    for (size_t i = 0; i < VISIBLE_ROWS; ++i)
    {
        float ry = listY + i * (ROW_HEIGHT + ROW_SPACING);

        // Row background
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{50.0f, 50.0f, 60.0f, 200.0f});
        auto bgPos = bg.get<PositionComponent>();
        bgPos->setX(listX);
        bgPos->setY(ry);
        bgPos->setZ(98.0f);
        bgPos->setWidth(rowW);
        bgPos->setHeight(ROW_HEIGHT);
        bg.get<Simple2DObject>()->setViewport(UI_VP);
        rowVisuals[i].bgEntityId = bg.entity->id;

        // Output item icon (left side)
        float itemY = ry + (ROW_HEIGHT - ITEM_SIZE) * 0.5f;
        auto item = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto itemPos = item.get<PositionComponent>();
        itemPos->setX(listX + 6.0f);
        itemPos->setY(itemY);
        itemPos->setZ(99.0f);
        itemPos->setVisibility(false);
        item.get<Texture2DComponent>()->setViewport(UI_VP);
        rowVisuals[i].outputItemEntityId = item.entity->id;

        // Recipe name
        float textAreaX = listX + 6.0f + ITEM_SIZE + 6.0f;
        auto name = makeTTFText(ecsRef,
            textAreaX, ry + 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE,
            {255.0f, 255.0f, 255.0f, 255.0f});
        name.get<TTFText>()->setViewport(UI_VP);
        name.get<PositionComponent>()->setVisibility(false);
        rowVisuals[i].nameEntityId = name.entity->id;

        // Ingredient icon + count text slots
        float ingrY = ry + ROW_HEIGHT - INGR_ICON_SIZE - 3.0f;
        for (size_t j = 0; j < MAX_INPUTS; ++j)
        {
            float ix = textAreaX + static_cast<float>(j) * INGR_SLOT_W;

            // Count label first ("Nx"), then icon to its right
            auto cnt = makeTTFText(ecsRef,
                ix, ingrY + 2.0f, 100.0f,
                FONT_PATH, "", TEXT_SCALE, {200.0f, 200.0f, 210.0f, 255.0f});
            cnt.get<TTFText>()->setViewport(UI_VP);
            cnt.get<PositionComponent>()->setVisibility(false);
            rowVisuals[i].ingrCountEntityId[j] = cnt.entity->id;

            auto icon = make2DTexture(ecsRef, INGR_ICON_SIZE, INGR_ICON_SIZE, "NoneIcon");
            auto iPos = icon.get<PositionComponent>();
            iPos->setX(ix + 22.0f); iPos->setY(ingrY); iPos->setZ(99.0f);
            iPos->setVisibility(false);
            icon.get<Texture2DComponent>()->setViewport(UI_VP);
            rowVisuals[i].ingrIconEntityId[j] = icon.entity->id;
        }
    }

    // Progress bar
    float barX = listX;
    float barY = listY + VISIBLE_ROWS * ROW_HEIGHT
               + (VISIBLE_ROWS - 1) * ROW_SPACING + GAP_AFTER_LIST;
    float barW = rowW;

    auto barBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{40.0f, 40.0f, 50.0f, 220.0f});
    auto barBgPos = barBg.get<PositionComponent>();
    barBgPos->setX(barX);
    barBgPos->setY(barY);
    barBgPos->setZ(98.0f);
    barBgPos->setWidth(barW);
    barBgPos->setHeight(PROGRESS_BAR_H);
    barBg.get<Simple2DObject>()->setViewport(UI_VP);
    progressBgEntityId = barBg.entity->id;

    auto barFill = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{80.0f, 200.0f, 80.0f, 255.0f});
    auto barFillPos = barFill.get<PositionComponent>();
    barFillPos->setX(barX);
    barFillPos->setY(barY);
    barFillPos->setZ(99.0f);
    barFillPos->setWidth(0.0f);
    barFillPos->setHeight(PROGRESS_BAR_H);
    barFill.get<Simple2DObject>()->setViewport(UI_VP);
    progressFillEntityId = barFill.entity->id;

    cachedBarX = barX;
    cachedBarMaxW = barW;

    // Buttons (Craft on the left, Cancel on the right)
    float buttonsY = barY + PROGRESS_BAR_H + GAP_AFTER_BAR;
    craftButtonX = listX;
    craftButtonY = buttonsY;
    cancelButtonX = listX + BUTTON_W + BUTTON_GAP;
    cancelButtonY = buttonsY;

    auto craftBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{70.0f, 130.0f, 80.0f, 240.0f});
    auto craftBgPos = craftBg.get<PositionComponent>();
    craftBgPos->setX(craftButtonX);
    craftBgPos->setY(craftButtonY);
    craftBgPos->setZ(98.0f);
    craftBgPos->setWidth(BUTTON_W);
    craftBgPos->setHeight(BUTTON_H);
    craftBg.get<Simple2DObject>()->setViewport(UI_VP);
    craftButtonBgEntityId = craftBg.entity->id;

    auto craftText = makeTTFText(ecsRef,
        craftButtonX + 18.0f, craftButtonY + 5.0f, 100.0f,
        FONT_PATH, "Craft", TEXT_SCALE,
        {255.0f, 255.0f, 255.0f, 255.0f});
    craftText.get<TTFText>()->setViewport(UI_VP);
    craftButtonTextEntityId = craftText.entity->id;

    auto cancelBg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{130.0f, 70.0f, 70.0f, 240.0f});
    auto cancelBgPos = cancelBg.get<PositionComponent>();
    cancelBgPos->setX(cancelButtonX);
    cancelBgPos->setY(cancelButtonY);
    cancelBgPos->setZ(98.0f);
    cancelBgPos->setWidth(BUTTON_W);
    cancelBgPos->setHeight(BUTTON_H);
    cancelBg.get<Simple2DObject>()->setViewport(UI_VP);
    cancelButtonBgEntityId = cancelBg.entity->id;

    auto cancelText = makeTTFText(ecsRef,
        cancelButtonX + 16.0f, cancelButtonY + 5.0f, 100.0f,
        FONT_PATH, "Cancel", TEXT_SCALE,
        {255.0f, 255.0f, 255.0f, 255.0f});
    cancelText.get<TTFText>()->setViewport(UI_VP);
    cancelButtonTextEntityId = cancelText.entity->id;

    cachedListX = listX;
    cachedListY = listY;
    cachedRowW = rowW;

    refreshTabHighlights();
}

void CraftingUISystem::rebuildVisibleRecipes()
{
    visibleRecipes.clear();

    if (activeMachineType != 0)
    {
        // Machine mode: show all recipes for this machine type
        RecipeCategory cat = (activeMachineType == 5) ? RecipeCategory::Furnace
                                                      : RecipeCategory::Assembler;
        for (size_t i = 0; i < recipeRegistry->recipes.size(); ++i)
        {
            const Recipe& r = recipeRegistry->recipes[i];
            if (r.category == cat and r.machineType == activeMachineType)
                visibleRecipes.push_back(i);
        }
    }
    else
    {
        // Hand-craft mode
        for (size_t i = 0; i < recipeRegistry->recipes.size(); ++i)
        {
            const Recipe& r = recipeRegistry->recipes[i];
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
            for (size_t j = 0; j < MAX_INPUTS; ++j)
            {
                setEntityVisibility(row.ingrIconEntityId[j],  false);
                setEntityVisibility(row.ingrCountEntityId[j], false);
            }
            tintRow(row.bgEntityId, RowTint::Idle);
            continue;
        }

        size_t recipeIdx = visibleRecipes[absIdx];
        const Recipe& recipe = recipeRegistry->recipes[recipeIdx];

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
                float ry = cachedListY + rowIdx * (ROW_HEIGHT + ROW_SPACING);
                float baseX = cachedListX + 6.0f;
                float baseY = ry + (ROW_HEIGHT - ITEM_SIZE) * 0.5f;
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
        auto nameEnt = ecsRef->getEntity(row.nameEntityId);
        if (nameEnt)
        {
            nameEnt->get<TTFText>()->setText(stripCraftingPrefix(recipe.name));
            nameEnt->get<PositionComponent>()->setVisibility(true);
        }

        // Ingredient icons + count labels
        for (size_t j = 0; j < MAX_INPUTS; ++j)
        {
            if (j < recipe.inputs.size())
            {
                const auto& in  = recipe.inputs[j];
                const auto& def = itemRegistry->get(in.id);

                auto iconEnt = ecsRef->getEntity(row.ingrIconEntityId[j]);
                if (iconEnt)
                {
                    iconEnt->get<Texture2DComponent>()->setTexture(def.textureName);
                    iconEnt->get<PositionComponent>()->setVisibility(true);
                }

                auto cntEnt = ecsRef->getEntity(row.ingrCountEntityId[j]);
                if (cntEnt)
                {
                    cntEnt->get<TTFText>()->setText(std::to_string(in.count) + "x");
                    cntEnt->get<PositionComponent>()->setVisibility(true);
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
        if (activeMachineType == 0)
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
    if (activeMachineType != 0)
        return; // Progress bar is hidden in machine mode

    auto fillEnt = ecsRef->getEntity(progressFillEntityId);
    if (not fillEnt)
        return;
    float ratio = handCrafting ? handCrafting->getProgressRatio() : 0.0f;
    fillEnt->get<PositionComponent>()->setWidth(cachedBarMaxW * ratio);
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

    if (outDef.buildingTileId != 0)
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
    if (y < cachedTabY or y > cachedTabY + TAB_ROW_H)
        return -1;

    float contentW = PANEL_WIDTH - 2 * PANEL_PADDING;
    float tabW = (contentW - (TAB_COUNT - 1) * TAB_GAP) / static_cast<float>(TAB_COUNT);
    float tabStartX = getPanelX() + PANEL_PADDING;

    for (size_t i = 0; i < TAB_COUNT; ++i)
    {
        float tx = tabStartX + i * (tabW + TAB_GAP);
        if (x >= tx and x <= tx + tabW)
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
    const Recipe& recipe = recipeRegistry->recipes[recipeIdx];
    if (not handCrafting->canCraft(recipe))
        return;
    ecsRef->sendEvent(HandCraftRequest{recipeIdx});
}

int CraftingUISystem::rowAtPosition(float x, float y) const
{
    if (x < cachedListX or x > cachedListX + cachedRowW)
        return -1;
    for (size_t i = 0; i < VISIBLE_ROWS; ++i)
    {
        float ry = cachedListY + i * (ROW_HEIGHT + ROW_SPACING);
        if (y >= ry and y <= ry + ROW_HEIGHT)
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
