#include "hudbarsystem.h"

#include "missionsystem.h"

#include "2D/simple2dobject.h"
#include "2D/position.h"
#include "2D/texture.h"
#include "UI/ttftext.h"
#include "UI/utils.h"

#include <SDL2/SDL.h>

using namespace pg;

void HudBarSystem::init()
{
    createButtons();

    // Subscribe to ticket changes through GameDataView instead of polling
    // playerInv->getTickets() every tick. The publisher is
    // PlayerInventorySystem; the path is PlayerInventorySystem::TICKETS_PATH.
    if (auto* view = ecsRef->getSystem<GameDataView>())
    {
        view->subscribe(PlayerInventorySystem::TICKETS_PATH,
            [this](const ElementType& v) {
                onTicketsChanged(static_cast<uint32_t>(v.get<size_t>()));
            });

        // Seed with the current value if the publisher already wrote it.
        if (view->has(PlayerInventorySystem::TICKETS_PATH))
            onTicketsChanged(static_cast<uint32_t>(view->get(PlayerInventorySystem::TICKETS_PATH).get<size_t>()));
    }
}

void HudBarSystem::onEvent(const TickEvent&)
{
    updateMissionButtonVisibility();
    updateMissionBadge();
}

void HudBarSystem::onEvent(const MissionUIOpenedEvent&)
{
    missionTabOpen = true;
    if (auto* worldFacts = ecsRef->getSystem<WorldFacts>())
        worldFacts->setFact("mission_attention_pending", false);
    setEntityVisibility(missionBadgeId, false);
}

void HudBarSystem::onEvent(const MissionUIClosedEvent&)
{
    missionTabOpen = false;
}

void HudBarSystem::onEvent(const HudInventoryButtonClicked&)
{
    // The MouseLeftClickComponent fires this synchronously during click
    // dispatch — mark the click as a panel click so GameSystem's
    // click-outside-to-close logic skips it on the same frame.
    ecsRef->sendEvent(PanelWasClickedEvent{});

    if (inventoryToggle)
        inventoryToggle();
}

void HudBarSystem::onEvent(const HudMissionButtonClicked&)
{
    if (not missionButtonVisible)
        return;

    ecsRef->sendEvent(PanelWasClickedEvent{});

    if (missionToggle)
        missionToggle();
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

    auto windowEnt = ecsRef->getEntity("__MainWindow");
    uint64_t windowId = windowEnt ? windowEnt->id : 0;

    for (size_t i = 0; i < NUM_BUTTONS; ++i)
    {
        // Right-anchored: button[N-1] sits at MARGIN_RIGHT, others stack to the left
        float rightMargin = MARGIN_RIGHT
                          + static_cast<float>(NUM_BUTTONS - 1 - i) * (BUTTON_SIZE + BUTTON_GAP);

        // Button background
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{40.0f, 40.0f, 50.0f, 180.0f});
        auto bgPos = bg.get<PositionComponent>();
        bgPos->setZ(95.0f);
        bgPos->setWidth(BUTTON_SIZE);
        bgPos->setHeight(BUTTON_SIZE);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        buttonBgId[i] = bg.entity->id;

        auto bgA = ecsRef->attach<UiAnchor>(bg.entity);
        if (windowId != 0)
        {
            bgA->setRightAnchor(PosAnchor{windowId, AnchorType::Right});
            bgA->setTopAnchor(PosAnchor{windowId, AnchorType::Top});
            bgA->setRightMargin(rightMargin);
            bgA->setTopMargin(MARGIN_TOP);
        }

        if (i == BTN_INVENTORY)
        {
            ecsRef->attach<MouseLeftClickComponent>(bg.entity,
                makeCallable<HudInventoryButtonClicked>(),
                MouseStateTrigger::OnPress);
            ecsRef->attach<EntityName>(bg.entity, "HudInventoryButton");
        }
        else if (i == BTN_MISSIONS)
        {
            ecsRef->attach<MouseLeftClickComponent>(bg.entity,
                makeCallable<HudMissionButtonClicked>(),
                MouseStateTrigger::OnPress);
            ecsRef->attach<EntityName>(bg.entity, "HudMissionButton");
        }
        // BTN_SETTINGS: no callback yet (placeholder).

        // Icon — centered in button via anchor
        auto icon = make2DTexture(ecsRef, ICON_SIZE, ICON_SIZE, ICON_TEXTURES[i]);
        icon.get<PositionComponent>()->setZ(96.0f);
        icon.get<ViewportComponent>()->setViewport(UI_VP);
        buttonIconId[i] = icon.entity->id;

        auto iA = ecsRef->attach<UiAnchor>(icon.entity);
        iA->setHorizontalCenter(PosAnchor{buttonBgId[i], AnchorType::HorizontalCenter});
        iA->setVerticalCenter(PosAnchor{buttonBgId[i], AnchorType::VerticalCenter});
    }

    createTicketDisplay();
    createMissionBadge();
}

void HudBarSystem::updateMissionButtonVisibility()
{
    // Mission button is always visible.
}

void HudBarSystem::setMissionButtonVisible(bool vis)
{
    missionButtonVisible = vis;
    setEntityVisibility(buttonBgId[BTN_MISSIONS],   vis);
    setEntityVisibility(buttonIconId[BTN_MISSIONS], vis);
    if (not vis)
        setEntityVisibility(missionBadgeId, false);
}

void HudBarSystem::createTicketDisplay()
{
    // Background pill — anchored below the mission button (left edge offset by -4)
    auto bg = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{30.0f, 30.0f, 40.0f, 180.0f});
    auto bgPos = bg.get<PositionComponent>();
    bgPos->setZ(95.0f);
    bgPos->setWidth(BUTTON_SIZE + 8.0f);
    bgPos->setHeight(20.0f);
    bgPos->setVisibility(false);
    bg.get<ViewportComponent>()->setViewport(UI_VP);
    ticketBgId = bg.entity->id;
    {
        auto a = ecsRef->attach<UiAnchor>(bg.entity);
        a->setLeftAnchor(PosAnchor{buttonBgId[BTN_MISSIONS], AnchorType::Left});
        a->setLeftMargin(-4.0f);
        a->setTopAnchor(PosAnchor{buttonBgId[BTN_MISSIONS], AnchorType::Bottom});
        a->setTopMargin(6.0f);
    }

    // Ticket icon — anchored to ticket bg
    auto icon = make2DTexture(ecsRef, 14.0f, 14.0f, "PixelwoodIcons.103");
    icon.get<PositionComponent>()->setZ(96.0f);
    icon.get<PositionComponent>()->setVisibility(false);
    icon.get<ViewportComponent>()->setViewport(UI_VP);
    ticketIconId = icon.entity->id;
    {
        auto a = ecsRef->attach<UiAnchor>(icon.entity);
        a->setLeftAnchor(PosAnchor{ticketBgId, AnchorType::Left});
        a->setLeftMargin(4.0f);
        a->setTopAnchor(PosAnchor{ticketBgId, AnchorType::Top});
        a->setTopMargin(3.0f);
    }

    // Count text — anchored to ticket bg, after the icon
    auto txt = makeTTFText(ecsRef, 0.0f, 0.0f, 97.0f,
        FONT_PATH, "0", TEXT_SCALE, {255.0f, 220.0f, 100.0f, 255.0f});
    txt.get<PositionComponent>()->setVisibility(false);
    txt.get<ViewportComponent>()->setViewport(UI_VP);
    ticketTextId = txt.entity->id;
    {
        auto a = ecsRef->attach<UiAnchor>(txt.entity);
        a->setLeftAnchor(PosAnchor{ticketBgId, AnchorType::Left});
        a->setLeftMargin(20.0f);
        a->setTopAnchor(PosAnchor{ticketBgId, AnchorType::Top});
        a->setTopMargin(2.0f);
    }
}

void HudBarSystem::onTicketsChanged(uint32_t newCount)
{
    if (ticketBgId == 0)
        return;

    uint16_t count = static_cast<uint16_t>(std::min(newCount, static_cast<uint32_t>(65535)));

    if (not ticketDisplayVisible)
    {
        ticketDisplayVisible = true;
        auto setVis = [this](uint64_t id, bool vis) {
            if (id == 0)
                return;
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
    auto badge = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{220.0f, 60.0f, 60.0f, 255.0f});
    auto pos = badge.get<PositionComponent>();
    pos->setZ(97.0f);
    pos->setWidth(MISSION_BADGE_SIZE);
    pos->setHeight(MISSION_BADGE_SIZE);
    pos->setVisibility(false);
    badge.get<ViewportComponent>()->setViewport(UI_VP);
    missionBadgeId = badge.entity->id;

    auto a = ecsRef->attach<UiAnchor>(badge.entity);
    a->setRightAnchor(PosAnchor{buttonBgId[BTN_MISSIONS], AnchorType::Right});
    a->setRightMargin(2.0f);
    a->setTopAnchor(PosAnchor{buttonBgId[BTN_MISSIONS], AnchorType::Top});
    a->setTopMargin(2.0f);
}

int HudBarSystem::countUnlockedMissions() const
{
    auto* missionSystem = ecsRef->getSystem<MissionSystem>();
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
    auto* missionSystem = ecsRef->getSystem<MissionSystem>();
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
    auto* missionSystem = ecsRef->getSystem<MissionSystem>();
    auto* worldFacts   = ecsRef->getSystem<WorldFacts>();
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

    if (missionTabOpen or not missionButtonVisible)
    {
        setEntityVisibility(missionBadgeId, false);
        return;
    }

    // Badge stays lit while there's a completable main mission, even after
    // the player has opened (and closed) the tab — the action is still
    // pending until they actually validate it. Unlock notifications are the
    // one-shot kind cleared by opening the tab.
    bool pending = worldFacts->getFact<bool>("mission_attention_pending", false);

    setEntityVisibility(missionBadgeId, pending or currentCompletable > 0);
}

void HudBarSystem::setEntityVisibility(uint64_t id, bool vis)
{
    pg::setEntityVisibility(ecsRef, id, vis);
}
