#include "tooltipsystem.h"

#include "2D/simple2dobject.h"
#include "2D/position.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>
#include <cstdio>   // snprintf

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void TooltipSystem::onProcessEvent(const OnSDLMouseMotion& event)
{
    cursorX = static_cast<float>(event.x);
    cursorY = static_cast<float>(event.y);

    // Determine what item (if any) is under the cursor
    ItemId id = ecsRef->getSystem<InventoryUISystem>()->itemAtPosition(cursorX, cursorY);
    if (id == ITEM_NONE)
        id = ecsRef->getSystem<HotbarSystem>()->itemAtPosition(cursorX, cursorY);

    if (id != hoveredItem)
    {
        hoveredItem = id;
        hoverStart  = static_cast<uint32_t>(SDL_GetTicks());
        if (id == ITEM_NONE)
            hideTooltip();
    }
}

void TooltipSystem::setHoveredItem(ItemId id)
{
    if (id == hoveredItem)
        return;
    hoveredItem = id;
    hoverStart  = static_cast<uint32_t>(SDL_GetTicks());
    if (id == ITEM_NONE)
        hideTooltip();
}

void TooltipSystem::onProcessEvent(const SetTooltipHoveredItemEvent& event)
{
    setHoveredItem(event.id);
}

void TooltipSystem::onProcessEvent(const TickEvent&)
{
    if (hoveredItem == ITEM_NONE)
        return;

    if (hoveredItem == shownItem)
    {
        // Keep tooltip pinned near cursor while same item is hovered
        if (visible)
            positionTooltip(cursorX, cursorY, computeHeight(lastNumBodyLines));
        return;
    }

    uint32_t elapsed = static_cast<uint32_t>(SDL_GetTicks()) - hoverStart;
    if (elapsed >= HOVER_DELAY_MS)
        showTooltip(hoveredItem);
}

// ---------------------------------------------------------------------------
// Panel creation
// ---------------------------------------------------------------------------

void TooltipSystem::ensureCreated()
{
    if (created) return;
    created = true;

    // Backdrop — follows cursor; placement done in placeAt() (setX/setY).
    auto bd = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{15.0f, 15.0f, 25.0f, 230.0f});
    auto bdPos = bd.get<PositionComponent>();
    bdPos->setX(0.0f); bdPos->setY(0.0f); bdPos->setZ(103.0f);
    bdPos->setWidth(TOOLTIP_W); bdPos->setHeight(40.0f);
    bdPos->setVisibility(false);
    bd.get<ViewportComponent>()->setViewport(UI_VP);
    backdropId = bd.entity->id;

    // Icon — anchored to backdrop top-left (X-tweak applied in showTooltip)
    auto icon = make2DTexture(ecsRef, ICON_SIZE, ICON_SIZE, "NoneIcon");
    auto iPos = icon.get<PositionComponent>();
    iPos->setZ(104.0f);
    iPos->setVisibility(false);
    icon.get<ViewportComponent>()->setViewport(UI_VP);
    iconId = icon.entity->id;
    {
        auto a = ecsRef->attach<UiAnchor>(icon.entity);
        a->setLeftAnchor(PosAnchor{backdropId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropId, AnchorType::Top});
        a->setLeftMargin(PADDING);
        a->setTopMargin(PADDING);
    }

    auto anchorLineToBackdrop = [this](EntityRef ent, float leftMargin, float topMargin) {
        auto a = ecsRef->attach<UiAnchor>(ent);
        a->setLeftAnchor(PosAnchor{backdropId, AnchorType::Left});
        a->setTopAnchor(PosAnchor{backdropId, AnchorType::Top});
        a->setLeftMargin(leftMargin);
        a->setTopMargin(topMargin);
    };

    // Line 0 — item name (right of icon, vertically centred with icon row)
    {
        auto t = makeTTFText(ecsRef, 0.0f, 0.0f, 104.0f,
            FONT_PATH, "", NAME_SCALE,
            {255.0f, 255.0f, 255.0f, 255.0f});
        t.get<ViewportComponent>()->setViewport(UI_VP);
        t.get<PositionComponent>()->setVisibility(false);
        lineIds[0] = t.entity->id;
        anchorLineToBackdrop(t.entity,
            PADDING + ICON_SIZE + 6.0f,
            PADDING + (ICON_SIZE - NAME_SCALE * 48.0f) * 0.5f);
    }
    // Body lines 1..5 — left of panel, stacked under icon row
    auto makeBodyLine = [this, &anchorLineToBackdrop](int i, const constant::Vector4D& color) {
        auto t = makeTTFText(ecsRef, 0.0f, 0.0f, 104.0f,
            FONT_PATH, "", BODY_SCALE, color);
        t.get<ViewportComponent>()->setViewport(UI_VP);
        t.get<PositionComponent>()->setVisibility(false);
        lineIds[i] = t.entity->id;
        anchorLineToBackdrop(t.entity,
            PADDING,
            PADDING + ICON_SIZE + 4.0f + static_cast<float>(i - 1) * LINE_H);
    };
    makeBodyLine(1, {160.0f, 190.0f, 220.0f, 255.0f});
    makeBodyLine(2, {210.0f, 210.0f, 210.0f, 255.0f});
    makeBodyLine(3, {255.0f, 210.0f,  80.0f, 255.0f});
    makeBodyLine(4, {210.0f, 210.0f, 210.0f, 255.0f});
    makeBodyLine(5, {210.0f, 210.0f, 210.0f, 255.0f});
}

// ---------------------------------------------------------------------------
// Show / hide
// ---------------------------------------------------------------------------

void TooltipSystem::showTooltip(ItemId id)
{
    ensureCreated();

    TooltipContent c = buildContent(id);

    // Set icon
    {
        const auto& def = itemRegistry->get(id);
        auto ent = ecsRef->getEntity(iconId);
        if (ent)
        {
            float iconW = ICON_SIZE * def.iconWidthRatio;
            if (auto tex = ent->get<Texture2DComponent>())
                tex->setTexture(def.textureName);
            if (auto pos = ent->get<PositionComponent>())
            {
                pos->setWidth(iconW);
                pos->setHeight(ICON_SIZE);
            }
        }
    }

    // Fill text lines
    // Line 0: name
    setLine(0, c.name,         {255.0f, 255.0f, 255.0f, 255.0f}, true);
    // Line 1: category
    setLine(1, c.categoryLine, {160.0f, 190.0f, 220.0f, 255.0f}, not c.categoryLine.empty());
    // Line 2: description
    setLine(2, c.descLine,     {210.0f, 210.0f, 210.0f, 255.0f}, not c.descLine.empty());
    // Line 3: source/craft header
    setLine(3, c.sourceLine,   {255.0f, 210.0f, 80.0f,  255.0f}, not c.sourceLine.empty());
    // Line 4: ingredients line 1
    setLine(4, c.ingrLine1,    {210.0f, 210.0f, 210.0f, 255.0f}, not c.ingrLine1.empty());
    // Line 5: ingredients line 2
    setLine(5, c.ingrLine2,    {210.0f, 210.0f, 210.0f, 255.0f}, not c.ingrLine2.empty());

    // Count visible body lines (lines 1-5)
    int numBody = 0;
    if (not c.categoryLine.empty()) ++numBody;
    if (not c.descLine.empty())     ++numBody;
    if (not c.sourceLine.empty())   ++numBody;
    if (not c.ingrLine1.empty())    ++numBody;
    if (not c.ingrLine2.empty())    ++numBody;
    lastNumBodyLines = numBody;

    float h = computeHeight(numBody);
    positionTooltip(cursorX, cursorY, h);

    // Show backdrop + icon
    setEntityVisibility(backdropId, true);
    setEntityVisibility(iconId, true);

    shownItem = id;
    visible   = true;
}

void TooltipSystem::hideTooltip()
{
    if (not visible and not created) return;
    setEntityVisibility(backdropId, false);
    setEntityVisibility(iconId,     false);
    for (int i = 0; i < NUM_LINES; ++i)
        setEntityVisibility(lineIds[i], false);
    visible   = false;
    shownItem = ITEM_NONE;
}

// ---------------------------------------------------------------------------
// Positioning
// ---------------------------------------------------------------------------

float TooltipSystem::computeHeight(int numBodyLines) const
{
    // Name row = ICON_SIZE height (icon + name side-by-side)
    // Each body line = LINE_H
    return 2.0f * PADDING + ICON_SIZE + static_cast<float>(numBodyLines) * LINE_H;
}

void TooltipSystem::placeAt(float tx, float ty, float h, int numBodyLines)
{
    // Backdrop position is screen-clamped against the cursor; children follow
    // via anchors set up in ensureCreated().
    auto ent = ecsRef->getEntity(backdropId);
    if (ent)
    {
        if (auto pos = ent->get<PositionComponent>())
        {
            pos->setX(tx);
            pos->setY(ty);
            pos->setWidth(TOOLTIP_W);
            pos->setHeight(h);
        }
    }

    // Adjust icon's leftMargin to account for the per-item icon width
    // (anchor was set up with PADDING; centre the visible icon in ICON_SIZE).
    if (auto iEnt = ecsRef->getEntity(iconId))
    {
        if (auto iPos = iEnt->get<PositionComponent>())
        {
            float iconW = iPos->getWidth();
            if (auto a = iEnt->get<UiAnchor>())
                a->setLeftMargin(PADDING + (ICON_SIZE - iconW) * 0.5f);
        }
    }

    (void)numBodyLines;
}

void TooltipSystem::positionTooltip(float cx, float cy, float h)
{
    float tx = cx + 14.0f;
    float ty = cy - h - 8.0f;

    // Clamp right edge
    if (tx + TOOLTIP_W > screenWidth - 4.0f)
        tx = screenWidth - TOOLTIP_W - 4.0f;
    // Clamp top — flip below cursor if too close to top
    if (ty < 4.0f)
        ty = cy + 24.0f;
    // Clamp bottom
    if (ty + h > screenHeight - 4.0f)
        ty = screenHeight - h - 4.0f;

    placeAt(tx, ty, h, lastNumBodyLines);
}

// ---------------------------------------------------------------------------
// Content building
// ---------------------------------------------------------------------------

TooltipSystem::TooltipContent TooltipSystem::buildContent(ItemId id) const
{
    TooltipContent c;
    const ItemDef& def = itemRegistry->get(id);

    c.name = def.name;
    c.categoryLine = categoryName(def.category);
    c.descLine = def.description;

    // Determine preferred recipe category based on open machine UI
    std::string preferMachineName; // empty = prefer HandCraft
    auto* uiCoordinator = ecsRef->getSystem<MachineUICoordinator>();
    if (uiCoordinator and uiCoordinator->isAnyOpen())
        preferMachineName = uiCoordinator->getOpenMachineName();

    // Search for a recipe that produces this item
    const Recipe* preferred = nullptr;
    const Recipe* fallback  = nullptr;

    for (const auto& recipe : recipeRegistry->all())
    {
        for (const auto& out : recipe.outputs)
        {
            if (out.id != id) continue;

            if (preferMachineName.empty())
            {
                // Hand-craft mode: prefer HandCraft, fall back to anything
                if (recipe.category == RecipeCategory::HandCraft)
                    { preferred = &recipe; break; }
                else if (not fallback)
                    fallback = &recipe;
            }
            else
            {
                // Machine mode: prefer the open machine's recipe
                if (recipe.machineName == preferMachineName)
                    { preferred = &recipe; break; }
                else if (not fallback)
                    fallback = &recipe;
            }
        }
        if (preferred) break;
    }

    const Recipe* recipe = preferred ? preferred : fallback;

    if (recipe)
    {
        // Source line: "Furnace — 2.0s"
        float secs = static_cast<float>(recipe->craftTimeMs) / 1000.0f;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s - %.1fs",
            machineLabel(recipe->machineName), secs);
        c.sourceLine = buf;

        // Ingredient lines (up to 3 ingredients split across 2 lines)
        std::string ingrStr;
        for (size_t i = 0; i < recipe->inputs.size(); ++i)
        {
            if (i > 0) ingrStr += "  +  ";
            const auto& ing = recipe->inputs[i];
            char ibuf[48];
            std::snprintf(ibuf, sizeof(ibuf), "%ux %s",
                static_cast<unsigned>(ing.count),
                itemRegistry->get(ing.id).name.c_str());
            ingrStr += ibuf;

            // Split onto second line after 2nd ingredient
            if (i == 1 and recipe->inputs.size() > 2)
            {
                c.ingrLine1 = ingrStr;
                ingrStr.clear();
            }
        }
        if (c.ingrLine1.empty())
            c.ingrLine1 = ingrStr;
        else
            c.ingrLine2 = ingrStr;
    }
    else if (def.worldSourceTier != 255)
    {
        // Not craftable — comes from the world
        switch (def.worldSourceTier)
        {
        case 0:  c.sourceLine = "Mining (bare hands)";      break;
        case 1:  c.sourceLine = "Mining (Stone Pickaxe+)";  break;
        case 2:  c.sourceLine = "Mining (Iron Pickaxe+)";   break;
        default: c.sourceLine = "Mining";                   break;
        }
    }

    return c;
}

const char* TooltipSystem::categoryName(ItemCategory cat)
{
    switch (cat)
    {
    case ItemCategory::Resource:     return "Resource";
    case ItemCategory::Intermediate: return "Intermediate";
    case ItemCategory::Product:      return "Product";
    case ItemCategory::Building:     return "Building";
    default:                         return "";
    }
}

const char* TooltipSystem::machineLabel(const std::string& machineName)
{
    if (machineName.empty()) return "Hand";
    return machineName.c_str();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void TooltipSystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
    {
        if (auto pos = ent->get<PositionComponent>())
            pos->setVisibility(vis);
    }
}

void TooltipSystem::setLine(int lineIdx, const std::string& text,
                             constant::Vector4D colour, bool vis)
{
    if (lineIdx < 0 or lineIdx >= NUM_LINES) return;
    uint64_t id = lineIds[lineIdx];
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (not ent) return;

    if (auto ttf = ent->get<TTFText>())
    {
        ttf->setText(text);
        ttf->colors = colour;
    }
    if (auto pos = ent->get<PositionComponent>())
        pos->setVisibility(vis);
}
