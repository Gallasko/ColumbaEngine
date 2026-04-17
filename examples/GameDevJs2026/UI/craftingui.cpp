#include "craftingui.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

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

    // Craft button
    if (isPointInRect(x, y, craftButtonX, craftButtonY, BUTTON_W, BUTTON_H))
    {
        requestCraft();
        return;
    }

    // Cancel button
    if (isPointInRect(x, y, cancelButtonX, cancelButtonY, BUTTON_W, BUTTON_H))
    {
        ecsRef->sendEvent(HandCraftCancel{});
        return;
    }

    // Row hit test
    int rowIndex = rowAtPosition(x, y);
    if (rowIndex >= 0)
    {
        size_t absIndex = scrollOffset + static_cast<size_t>(rowIndex);
        if (absIndex < visibleRecipes.size())
        {
            selectedIndex = absIndex;
            refreshRows();
        }
    }
}

void CraftingUISystem::open()
{
    visible = true;
    ensurePanelCreated();
    rebuildVisibleRecipes();

    // Pair our click region with the inventory UI so clicks on our
    // panel don't cancel held items / close the inventory.
    if (inventoryUI)
    {
        inventoryUI->setExternalClickCheck(
            [this](float x, float y) { return isClickOnPanel(x, y); });
    }

    setPanelVisibility(true);
    refreshRows();
    refreshProgressBar();
}

void CraftingUISystem::close()
{
    visible = false;

    // Only clear the external click check if it's still pointing at us.
    // (Other UIs may have overridden it — best-effort.)
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
        setEntityVisibility(row.itemEntityId, false);
        setEntityVisibility(row.nameEntityId, false);
        setEntityVisibility(row.statusEntityId, false);
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

    // Row skeletons
    float listX = px + PANEL_PADDING;
    float listY = py + PANEL_PADDING + TITLE_H + GAP_AFTER_TITLE;
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

        // Item icon
        float itemY = ry + (ROW_HEIGHT - ITEM_SIZE) * 0.5f;
        auto item = make2DTexture(ecsRef, ITEM_SIZE, ITEM_SIZE, "NoneIcon");
        auto itemPos = item.get<PositionComponent>();
        itemPos->setX(listX + 6.0f);
        itemPos->setY(itemY);
        itemPos->setZ(99.0f);
        itemPos->setVisibility(false);
        item.get<Texture2DComponent>()->setViewport(UI_VP);
        rowVisuals[i].itemEntityId = item.entity->id;

        // Recipe name
        auto name = makeTTFText(ecsRef,
            listX + 6.0f + ITEM_SIZE + 6.0f, ry + 4.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE,
            {255.0f, 255.0f, 255.0f, 255.0f});
        name.get<TTFText>()->setViewport(UI_VP);
        name.get<PositionComponent>()->setVisibility(false);
        rowVisuals[i].nameEntityId = name.entity->id;

        // Ingredient / status line
        auto status = makeTTFText(ecsRef,
            listX + 6.0f + ITEM_SIZE + 6.0f, ry + 22.0f, 100.0f,
            FONT_PATH, "", TEXT_SCALE,
            {180.0f, 180.0f, 190.0f, 255.0f});
        status.get<TTFText>()->setViewport(UI_VP);
        status.get<PositionComponent>()->setVisibility(false);
        rowVisuals[i].statusEntityId = status.entity->id;
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
}

void CraftingUISystem::rebuildVisibleRecipes()
{
    visibleRecipes.clear();
    for (size_t i = 0; i < recipeRegistry->recipes.size(); ++i)
    {
        const Recipe& r = recipeRegistry->recipes[i];
        if (r.category != RecipeCategory::HandCraft)
            continue;
        if (not handCrafting->isUnlocked(r))
            continue;
        visibleRecipes.push_back(i);
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
            setEntityVisibility(row.itemEntityId, false);
            setEntityVisibility(row.nameEntityId, false);
            setEntityVisibility(row.statusEntityId, false);
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
            setEntityVisibility(row.itemEntityId, false);
            setEntityVisibility(row.nameEntityId, false);
            setEntityVisibility(row.statusEntityId, false);
            tintRow(row.bgEntityId, RowTint::Idle);
            continue;
        }

        size_t recipeIdx = visibleRecipes[absIdx];
        const Recipe& recipe = recipeRegistry->recipes[recipeIdx];

        // Output icon: first output item.
        if (not recipe.outputs.empty())
        {
            const auto& outDef = itemRegistry->get(recipe.outputs.front().id);
            auto itemEnt = ecsRef->getEntity(row.itemEntityId);
            if (itemEnt)
            {
                itemEnt->get<Texture2DComponent>()->setTexture(outDef.textureName);
                itemEnt->get<PositionComponent>()->setVisibility(true);
            }
        }
        else
        {
            setEntityVisibility(row.itemEntityId, false);
        }

        // Recipe name
        auto nameEnt = ecsRef->getEntity(row.nameEntityId);
        if (nameEnt)
        {
            nameEnt->get<TTFText>()->setText(recipe.name);
            nameEnt->get<PositionComponent>()->setVisibility(true);
        }

        // Status line: "have/need" for each input.
        auto statusEnt = ecsRef->getEntity(row.statusEntityId);
        if (statusEnt)
        {
            statusEnt->get<TTFText>()->setText(buildIngredientSummary(recipe));
            statusEnt->get<PositionComponent>()->setVisibility(true);
        }

        // Background tint: selected / can-craft / insufficient.
        RowTint tint = RowTint::Idle;
        if (absIdx == selectedIndex)
            tint = handCrafting->canCraft(recipe) ? RowTint::SelectedOk : RowTint::SelectedBad;
        else if (handCrafting->canCraft(recipe))
            tint = RowTint::CanCraft;
        else
            tint = RowTint::Insufficient;
        tintRow(row.bgEntityId, tint);
    }
}

std::string CraftingUISystem::buildIngredientSummary(const Recipe& recipe) const
{
    const auto& inv = playerInv->getInventory();
    std::string out;
    for (size_t i = 0; i < recipe.inputs.size(); ++i)
    {
        const auto& in = recipe.inputs[i];
        const auto& def = itemRegistry->get(in.id);
        uint16_t have = inv.countItem(in.id);

        if (i > 0) out += ", ";
        out += std::to_string(have);
        out += "/";
        out += std::to_string(in.count);
        out += " ";
        out += def.name;
    }
    return out;
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
    auto fillEnt = ecsRef->getEntity(progressFillEntityId);
    if (not fillEnt)
        return;
    float ratio = handCrafting ? handCrafting->getProgressRatio() : 0.0f;
    fillEnt->get<PositionComponent>()->setWidth(cachedBarMaxW * ratio);
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
