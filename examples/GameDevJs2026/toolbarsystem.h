#pragma once

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "Input/inputcomponent.h"

#include "buildingregistry.h"

using namespace pg;

class ToolbarSystem : public System<InitSys, Listener<OnSDLScanCode>, QueuedListener<OnMouseClick>>
{
public:
    static constexpr float TOOLBAR_HEIGHT = 48.0f;
    static constexpr float SLOT_SIZE = 32.0f;
    static constexpr float SLOT_SPACING = 4.0f;
    static constexpr float SLOT_PADDING = 8.0f; // Padding from toolbar edges

    ToolbarSystem(BuildingRegistry* registry, float screenWidth, float screenHeight)
        : registry(registry), screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Toolbar System"; }

    void init() override
    {
        createToolbarUI();
    }

    virtual void onEvent(const OnSDLScanCode& event) override
    {
        // Number keys 1-9 select toolbar slots
        if (event.key >= SDL_SCANCODE_1 and event.key <= SDL_SCANCODE_9)
        {
            size_t index = event.key - SDL_SCANCODE_1;
            if (index < registry->count())
                selectSlot(index);
        }
    }

    virtual void onProcessEvent(const OnMouseClick& event) override
    {
        if (event.button != SDL_BUTTON_LEFT)
            return;

        // Check if click is in toolbar area
        if (event.pos.y < screenHeight - TOOLBAR_HEIGHT)
            return;

        // Determine which slot was clicked
        float totalWidth = registry->count() * SLOT_SIZE + (registry->count() - 1) * SLOT_SPACING;
        float startX = (screenWidth - totalWidth) * 0.5f;

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

    size_t getSelectedSlot() const { return selectedSlot; }

    bool isMouseOverToolbar(float mouseY) const
    {
        return mouseY > screenHeight - TOOLBAR_HEIGHT;
    }

private:
    void selectSlot(size_t index)
    {
        if (index == selectedSlot)
            return;

        selectedSlot = index;
        updateHighlight();
        printf("Selected: %s\n", registry->get(index).name.c_str());
    }

    void createToolbarUI()
    {
        // Backdrop bar at bottom of screen
        auto backdrop = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{30.0f, 30.0f, 40.0f, 200.0f});

        auto backdropPos = backdrop.get<PositionComponent>();
        backdropPos->setX(0.0f);
        backdropPos->setY(screenHeight - TOOLBAR_HEIGHT);
        backdropPos->setZ(0.9f);
        backdropPos->setWidth(screenWidth);
        backdropPos->setHeight(TOOLBAR_HEIGHT);
        // Viewport 0 = default screen-space (no setViewport needed)

        // Create slots centered horizontally
        float totalWidth = registry->count() * SLOT_SIZE + (registry->count() - 1) * SLOT_SPACING;
        float startX = (screenWidth - totalWidth) * 0.5f;
        float slotY = screenHeight - TOOLBAR_HEIGHT + SLOT_PADDING;

        for (size_t i = 0; i < registry->count(); ++i)
        {
            const auto& def = registry->get(i);
            float slotX = startX + i * (SLOT_SIZE + SLOT_SPACING);

            uint64_t slotId = 0;

            if (not def.textureName.empty())
            {
                // Textured slot (use first frame of first direction)
                std::string texName = def.textureName + ".0";
                auto slot = make2DTexture(ecsRef, SLOT_SIZE, SLOT_SIZE, texName);

                auto pos = slot.get<PositionComponent>();
                pos->setX(slotX);
                pos->setY(slotY);
                pos->setZ(0.95f);

                slotId = slot.entity->id;
            }
            else
            {
                // Colored square slot
                auto slot = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, def.color);

                auto pos = slot.get<PositionComponent>();
                pos->setX(slotX);
                pos->setY(slotY);
                pos->setZ(0.95f);
                pos->setWidth(SLOT_SIZE);
                pos->setHeight(SLOT_SIZE);

                slotId = slot.entity->id;
            }

            slotEntityIds.push_back(slotId);
        }

        // Selection highlight overlay
        auto highlight = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f,
            constant::Vector4D{255.0f, 255.0f, 255.0f, 60.0f});

        auto hlPos = highlight.get<PositionComponent>();
        hlPos->setZ(0.96f);
        hlPos->setWidth(SLOT_SIZE + 4.0f);
        hlPos->setHeight(SLOT_SIZE + 4.0f);

        highlightEntityId = highlight.entity->id;
        updateHighlight();
    }

    void updateHighlight()
    {
        if (selectedSlot >= slotEntityIds.size())
            return;

        auto slotEnt = ecsRef->getEntity(slotEntityIds[selectedSlot]);
        auto hlEnt = ecsRef->getEntity(highlightEntityId);

        if (not slotEnt or not hlEnt)
            return;

        auto slotPos = slotEnt->get<PositionComponent>();
        auto hlPos = hlEnt->get<PositionComponent>();

        // Center highlight around the slot
        hlPos->setX(slotPos->getX() - 2.0f);
        hlPos->setY(slotPos->getY() - 2.0f);
    }

    BuildingRegistry* registry = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    size_t selectedSlot = 0;
    std::vector<uint64_t> slotEntityIds;
    uint64_t highlightEntityId = 0;
};
