#include "hudbarsystem.h"

#include "missionsystem.h"

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
    updateMissionBadge();
}

void HudBarSystem::onEvent(const MissionUIOpenedEvent&)
{
    missionTabOpen = true;
    if (worldFacts)
        worldFacts->setFact("mission_attention_pending", false);
    setEntityVisibility(missionBadgeId, false);
}

void HudBarSystem::onEvent(const MissionUIClosedEvent&)
{
    missionTabOpen = false;
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
                if (inventoryToggle)
                    inventoryToggle();
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
        "PixelwoodIcons.15",   // Backpack / chest
        "PixelwoodIcons.14",   // Mission / scroll
        "PixelwoodIcons.36",   // Settings / gear
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

    createTicketDisplay();
    createMissionBadge();
}

void HudBarSystem::updateMissionButtonVisibility()
{
    // Mission button is always visible.
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

    if (ticketBgId == 0)
        return;

    // Show ticket display on first tick
    if (not ticketDisplayVisible)
    {
        ticketDisplayVisible = true;
        auto setVis = [this](uint64_t id, bool vis) {
            if (id == 0) return;
            auto ent = ecsRef->getEntity(id);
            if (ent)
                ent->get<PositionComponent>()->setVisibility(vis);
        };
        setVis(ticketBgId, true);
        setVis(ticketIconId, true);
        setVis(ticketTextId, true);
    }

    if (count != lastTicketCount)
    {
        lastTicketCount = count;
        auto ent = ecsRef->getEntity(ticketTextId);
        if (ent and ent->has<TTFText>())
            ent->get<TTFText>()->setText(std::to_string(count));
    }
}

void HudBarSystem::createMissionBadge()
{
    // Small red square at the top-right corner of the mission button.
    float bx = buttonX[BTN_MISSIONS] + BUTTON_SIZE - MISSION_BADGE_SIZE - 2.0f;
    float by = buttonY[BTN_MISSIONS] + 2.0f;

    auto badge = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{220.0f, 60.0f, 60.0f, 255.0f});
    auto pos = badge.get<PositionComponent>();
    pos->setX(bx);
    pos->setY(by);
    pos->setZ(97.0f);
    pos->setWidth(MISSION_BADGE_SIZE);
    pos->setHeight(MISSION_BADGE_SIZE);
    pos->setVisibility(false);
    badge.get<ViewportComponent>()->setViewport(UI_VP);
    missionBadgeId = badge.entity->id;
}

int HudBarSystem::countUnlockedMissions() const
{
    if (not missionSystem)
        return 0;
    const auto& defs = missionSystem->getDefs();
    int count = 0;
    for (size_t i = 0; i < defs.size(); ++i)
        if (missionSystem->isMissionUnlocked(i))
            ++count;
    return count;
}

int HudBarSystem::countCompletableMainMissions() const
{
    if (not missionSystem)
        return 0;
    const auto& defs = missionSystem->getDefs();
    int count = 0;
    for (size_t i = 0; i < defs.size(); ++i)
        if (missionSystem->canValidateMainMission(i))
            ++count;
    return count;
}

void HudBarSystem::updateMissionBadge()
{
    if (missionBadgeId == 0 or not missionSystem or not worldFacts)
        return;

    int currentUnlocks = countUnlockedMissions();
    int currentCompletable = countCompletableMainMissions();

    // Lazy seed: on the first tick after load we don't know the player's
    // baseline, so we just record current counts without arming the badge.
    // Missions with empty unlockFact report as unlocked from the start.
    if (prevUnlockCount < 0 or prevCompletableCount < 0)
    {
        prevUnlockCount = currentUnlocks;
        prevCompletableCount = currentCompletable;
    }

    // Detect transitions. We only ARM the badge when the count goes UP and
    // the player isn't already in the mission tab — anything they'd see by
    // opening the tab is implicitly "seen".
    if (not missionTabOpen)
    {
        bool unlockTransition     = currentUnlocks     > prevUnlockCount;
        bool completableTransition = currentCompletable > prevCompletableCount;
        if (unlockTransition or completableTransition)
            worldFacts->setFact("mission_attention_pending", true);
    }

    prevUnlockCount = currentUnlocks;
    prevCompletableCount = currentCompletable;

    if (missionTabOpen)
    {
        setEntityVisibility(missionBadgeId, false);
        return;
    }

    bool pending = worldFacts->getFact<bool>("mission_attention_pending", false);
    // Also show the badge while the tutorial is on the "open mission tab"
    // step so it doubles as a visual ping pointing at the button.
    bool tutorialPointsHere = (worldFacts->getFact<int>("tutorial_step", 0) == 3);

    setEntityVisibility(missionBadgeId, pending or tutorialPointsHere);
}

void HudBarSystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0)
        return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}
