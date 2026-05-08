#include "toolbarsystem.h"

#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "2D/position.h"

#include <SDL2/SDL.h>

void ToolbarSystem::init()
{
    createUICamera();
    createToolbarUI();
}

void ToolbarSystem::onEvent(const OnSDLScanCode& event)
{
    // Number keys 1-9 select toolbar slots
    if (event.key >= SDL_SCANCODE_1 and event.key <= SDL_SCANCODE_9)
    {
        size_t index = event.key - SDL_SCANCODE_1;
        if (index < registry->count())
            selectSlot(index);
    }
}

void ToolbarSystem::onProcessEvent(const OnMouseClick& event)
{
    if (event.button != SDL_BUTTON_LEFT)
        return;

    // Check if click is in toolbar area
    if (event.pos.y < screenHeight - TOOLBAR_HEIGHT)
        return;

    // Determine which slot was clicked
    float totalSlotsWidth = registry->count() * SLOT_SIZE + (registry->count() - 1) * SLOT_SPACING;
    float startX = (screenWidth - totalSlotsWidth) * 0.5f;

    for (size_t i = 0; i < registry->count(); ++i)
    {
        float slotX = startX + i * (SLOT_SIZE + SLOT_SPACING);
        float slotY = screenHeight - TOOLBAR_HEIGHT + SLOT_PADDING;

        if (event.pos.x >= slotX and event.pos.x <= slotX + SLOT_SIZE and
            event.pos.y >= slotY and event.pos.y <= slotY + SLOT_SIZE)
        {
            selectSlot(i);
            return;
        }
    }
}

void ToolbarSystem::selectSlot(size_t index)
{
    if (index == selectedSlot)
        return;

    selectedSlot = index;
    updateHighlight();
    LOG_INFO("Toolbar", "selected: " << registry->get(index).name);
}

void ToolbarSystem::createUICamera()
{
    // Create a 2D orthographic camera for the UI, registered as viewport 2
    uiCameraEntity = ecsRef->createEntity();
    auto cam = ecsRef->_attach<BaseCamera2D>(uiCameraEntity);
    cam->setWidth(screenWidth);
    cam->setHeight(screenHeight);
    masterRenderer->queueRegisterCamera(uiCameraEntity->id);
}

void ToolbarSystem::createToolbarUI()
{
    auto windowEnt = ecsRef->getEntity("__MainWindow");
    auto windowId = windowEnt->id;

    // Backdrop: anchored to __MainWindow — fills width, sticks to bottom
    auto backdrop = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{30.0f, 30.0f, 40.0f, 200.0f});

    auto backdropPos = backdrop.get<PositionComponent>();
    backdropPos->setZ(90.f);
    backdropPos->setHeight(TOOLBAR_HEIGHT);
    backdrop.get<ViewportComponent>()->setViewport(UI_VIEWPORT);
    backdropEntityId = backdrop.entity->id;

    auto bdAnchor = ecsRef->attach<UiAnchor>(backdrop.entity);
    bdAnchor->setLeftAnchor(PosAnchor{windowId, AnchorType::Left});
    bdAnchor->setRightAnchor(PosAnchor{windowId, AnchorType::Right});
    bdAnchor->setBottomAnchor(PosAnchor{windowId, AnchorType::Bottom});

    // Invisible container — horizontally centered in backdrop, slot row top-aligned with padding
    float totalSlotsWidth = registry->count() * SLOT_SIZE + (registry->count() - 1) * SLOT_SPACING;
    auto container = ecsRef->createEntity();
    auto containerPos = ecsRef->attach<PositionComponent>(container);
    containerPos->setWidth(totalSlotsWidth);
    containerPos->setHeight(SLOT_SIZE);
    containerEntityId = container->id;

    auto cAnchor = ecsRef->attach<UiAnchor>(container);
    cAnchor->setHorizontalCenter(PosAnchor{backdropEntityId, AnchorType::HorizontalCenter});
    cAnchor->setTopAnchor(PosAnchor{backdropEntityId, AnchorType::Top});
    cAnchor->setTopMargin(SLOT_PADDING);

    // Slots — anchored to container with per-index leftMargin
    for (size_t i = 0; i < registry->count(); ++i)
    {
        const auto& def = registry->get(i);
        float slotLeftMargin = static_cast<float>(i) * (SLOT_SIZE + SLOT_SPACING);

        uint64_t slotId = 0;
        EntityRef slotEntity;

        if (not def.textureName.empty())
        {
            std::string texName = def.textureName + ".0";
            auto slot = make2DTexture(ecsRef, SLOT_SIZE, SLOT_SIZE, texName);
            slot.get<PositionComponent>()->setZ(95.f);
            slot.get<ViewportComponent>()->setViewport(UI_VIEWPORT);
            slotId = slot.entity->id;
            slotEntity = slot.entity;
        }
        else
        {
            auto slot = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, def.color);
            auto pos = slot.get<PositionComponent>();
            pos->setZ(95.f);
            pos->setWidth(SLOT_SIZE);
            pos->setHeight(SLOT_SIZE);
            slot.get<ViewportComponent>()->setViewport(UI_VIEWPORT);
            slotId = slot.entity->id;
            slotEntity = slot.entity;
        }

        auto anchor = ecsRef->attach<UiAnchor>(slotEntity);
        anchor->setLeftAnchor(PosAnchor{containerEntityId, AnchorType::Left});
        anchor->setLeftMargin(slotLeftMargin);
        anchor->setTopAnchor(PosAnchor{containerEntityId, AnchorType::Top});

        slotEntityIds.push_back(slotId);
    }

    // Selection highlight overlay — anchored to selected slot in updateHighlight()
    auto highlight = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
        constant::Vector4D{255.0f, 255.0f, 255.0f, 60.0f});

    auto hlPos = highlight.get<PositionComponent>();
    hlPos->setZ(0.96f);
    hlPos->setWidth(SLOT_SIZE + 4.0f);
    hlPos->setHeight(SLOT_SIZE + 4.0f);
    highlight.get<ViewportComponent>()->setViewport(UI_VIEWPORT);
    ecsRef->attach<UiAnchor>(highlight.entity);

    highlightEntityId = highlight.entity->id;
    updateHighlight();
}

void ToolbarSystem::updateHighlight()
{
    if (selectedSlot >= slotEntityIds.size())
        return;

    auto hlEnt = ecsRef->getEntity(highlightEntityId);
    if (not hlEnt)
        return;

    auto hlAnchor = hlEnt->get<UiAnchor>();
    if (not hlAnchor)
        hlAnchor = ecsRef->attach<UiAnchor>(hlEnt);

    hlAnchor->clearAnchors();
    hlAnchor->setLeftAnchor(PosAnchor{slotEntityIds[selectedSlot], AnchorType::Left});
    hlAnchor->setLeftMargin(-2.0f);
    hlAnchor->setTopAnchor(PosAnchor{slotEntityIds[selectedSlot], AnchorType::Top});
    hlAnchor->setTopMargin(-2.0f);
}
