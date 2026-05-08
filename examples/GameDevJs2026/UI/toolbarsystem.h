#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"
#include "Renderer/renderer.h"
#include "Renderer/camera.h"

#include "buildingregistry.h"

using namespace pg;

// Viewport index for the toolbar UI camera
inline constexpr size_t UI_VIEWPORT = 2;

class ToolbarSystem : public System<InitSys, Listener<OnSDLScanCode>, QueuedListener<OnMouseClick>>
{
public:
    static constexpr float TOOLBAR_HEIGHT = 48.0f;
    static constexpr float SLOT_SIZE = 32.0f;
    static constexpr float SLOT_SPACING = 4.0f;
    static constexpr float SLOT_PADDING = 8.0f; // Padding from toolbar edges

    ToolbarSystem(BuildingRegistry* registry, MasterRenderer* masterRenderer, float screenWidth, float screenHeight)
        : registry(registry), masterRenderer(masterRenderer), screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Toolbar System"; }

    void init() override;

    virtual void onEvent(const OnSDLScanCode& event) override;
    virtual void onProcessEvent(const OnMouseClick& event) override;

    size_t getSelectedSlot() const { return selectedSlot; }

    bool isMouseOverToolbar(float mouseY) const
    {
        return mouseY > screenHeight - TOOLBAR_HEIGHT;
    }

private:
    void selectSlot(size_t index);
    void createUICamera();
    void createToolbarUI();
    void updateHighlight();

    BuildingRegistry* registry = nullptr;
    MasterRenderer* masterRenderer = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    EntityRef uiCameraEntity;
    size_t selectedSlot = 0;
    std::vector<uint64_t> slotEntityIds;
    uint64_t highlightEntityId = 0;
    uint64_t backdropEntityId = 0;
    uint64_t containerEntityId = 0;
};
