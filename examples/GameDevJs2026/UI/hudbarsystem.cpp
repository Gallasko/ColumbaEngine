#include "hudbarsystem.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"

#include <SDL2/SDL.h>

void HudBarSystem::init()
{
    createButtons();
}

void HudBarSystem::onEvent(const TickEvent&)
{
    updateMissionButtonVisibility();
    updateTicketDisplay();
}

void HudBarSystem::onProcessEvent(const OnMouseClick& event)
{
    if (event.button != SDL_BUTTON_LEFT)
        return;

    for (size_t i = 0; i < NUM_BUTTONS; ++i)
    {
        if (not isClickOnButton(i, event.pos.x, event.pos.y))
            continue;

        switch (i)
        {
            case BTN_INVENTORY:
                if (inventoryUI)
                {
                    if (inventoryUI->isOpen())
                        inventoryUI->closeInventory();
                    else
                        inventoryUI->openInventory();
                }
                break;

            case BTN_MISSIONS:
                if (missionToggle)
                    missionToggle();
                break;

            case BTN_SETTINGS:
                // Placeholder — no settings panel yet
                break;
        }
        return;
    }
}

void HudBarSystem::createButtons()
{
    if (buttonsCreated)
        return;
    buttonsCreated = true;

    // Placeholder icons from PixelwoodIcons atlas
    static const char* ICON_TEXTURES[NUM_BUTTONS] = {
        "PixelwoodIcons.147",   // Backpack / chest
        "PixelwoodIcons.168",   // Mission / scroll
        "PixelwoodIcons.189",   // Settings / gear
    };

    float totalWidth = NUM_BUTTONS * BUTTON_SIZE + (NUM_BUTTONS - 1) * BUTTON_GAP;
    float startX = screenWidth - MARGIN_RIGHT - totalWidth;
    float startY = MARGIN_TOP;

    for (size_t i = 0; i < NUM_BUTTONS; ++i)
    {
        float bx = startX + i * (BUTTON_SIZE + BUTTON_GAP);
        float by = startY;
        buttonX[i] = bx;
        buttonY[i] = by;

        // Button background
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{40.0f, 40.0f, 50.0f, 180.0f});
        auto bgPos = bg.get<PositionComponent>();
        bgPos->setX(bx);
        bgPos->setY(by);
        bgPos->setZ(95.0f);
        bgPos->setWidth(BUTTON_SIZE);
        bgPos->setHeight(BUTTON_SIZE);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        buttonBgId[i] = bg.entity->id;

        // Icon centered in button
        float iconOffset = (BUTTON_SIZE - ICON_SIZE) * 0.5f;
        auto icon = make2DTexture(ecsRef, ICON_SIZE, ICON_SIZE, ICON_TEXTURES[i]);
        auto iconPos = icon.get<PositionComponent>();
        iconPos->setX(bx + iconOffset);
        iconPos->setY(by + iconOffset);
        iconPos->setZ(96.0f);
        icon.get<ViewportComponent>()->setViewport(UI_VP);
        buttonIconId[i] = icon.entity->id;
    }

    // Mission button starts hidden until first depot is placed
    updateMissionButtonVisibility();
}

void HudBarSystem::updateMissionButtonVisibility()
{
    bool shouldShow = worldFacts and worldFacts->getFact<bool>("depot_placed");

    if (shouldShow == missionButtonVisible)
        return;
    missionButtonVisible = shouldShow;

    auto setVis = [this](uint64_t id, bool vis) {
        if (id == 0) return;
        auto ent = ecsRef->getEntity(id);
        if (ent)
            ent->get<PositionComponent>()->setVisibility(vis);
    };

    setVis(buttonBgId[BTN_MISSIONS], shouldShow);
    setVis(buttonIconId[BTN_MISSIONS], shouldShow);
}

bool HudBarSystem::isClickOnButton(size_t idx, float x, float y) const
{
    if (idx == BTN_MISSIONS and not missionButtonVisible)
        return false;

    return x >= buttonX[idx] and x <= buttonX[idx] + BUTTON_SIZE
       and y >= buttonY[idx] and y <= buttonY[idx] + BUTTON_SIZE;
}

void HudBarSystem::createTicketDisplay()
{
    // Position below the mission button
    float cx = buttonX[BTN_MISSIONS];
    float cy = buttonY[BTN_MISSIONS] + BUTTON_SIZE + 6.0f;

    // Background pill
    auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{30.0f, 30.0f, 40.0f, 180.0f});
    auto bgPos = bg.get<PositionComponent>();
    bgPos->setX(cx - 4.0f);
    bgPos->setY(cy);
    bgPos->setZ(95.0f);
    bgPos->setWidth(BUTTON_SIZE + 8.0f);
    bgPos->setHeight(20.0f);
    bgPos->setVisibility(false);
    bg.get<ViewportComponent>()->setViewport(UI_VP);
    ticketBgId = bg.entity->id;

    // Ticket icon (small)
    auto icon = make2DTexture(ecsRef, 14.0f, 14.0f, "PixelwoodIcons.103");
    auto iconPos = icon.get<PositionComponent>();
    iconPos->setX(cx);
    iconPos->setY(cy + 3.0f);
    iconPos->setZ(96.0f);
    iconPos->setVisibility(false);
    icon.get<ViewportComponent>()->setViewport(UI_VP);
    ticketIconId = icon.entity->id;

    // Count text
    auto txt = makeTTFText(ecsRef, cx + 16.0f, cy + 2.0f, 97.0f,
        FONT_PATH, "0", TEXT_SCALE, {255.0f, 220.0f, 100.0f, 255.0f});
    txt.get<PositionComponent>()->setVisibility(false);
    txt.get<ViewportComponent>()->setViewport(UI_VP);
    ticketTextId = txt.entity->id;
}

void HudBarSystem::updateTicketDisplay()
{
    if (not playerInv)
        return;

    uint16_t count = static_cast<uint16_t>(
        std::min(playerInv->getTickets(), static_cast<uint32_t>(65535)));

    // Show ticket display once missions are unlocked (even at 0)
    bool shouldShow = missionButtonVisible;

    if (shouldShow and ticketBgId == 0)
        createTicketDisplay();

    if (shouldShow != ticketDisplayVisible)
    {
        ticketDisplayVisible = shouldShow;
        auto setVis = [this](uint64_t id, bool vis) {
            if (id == 0) return;
            auto ent = ecsRef->getEntity(id);
            if (ent)
                ent->get<PositionComponent>()->setVisibility(vis);
        };
        setVis(ticketBgId, shouldShow);
        setVis(ticketIconId, shouldShow);
        setVis(ticketTextId, shouldShow);
    }

    if (shouldShow and count != lastTicketCount)
    {
        lastTicketCount = count;
        auto ent = ecsRef->getEntity(ticketTextId);
        if (ent and ent->has<TTFText>())
            ent->get<TTFText>()->setText(std::to_string(count));
    }
}
