#include "hudbarsystem.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"

#include <SDL2/SDL.h>

void HudBarSystem::init()
{
    createButtons();
}

void HudBarSystem::onEvent(const TickEvent&)
{
    updateMissionButtonVisibility();
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
