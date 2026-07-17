#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"
#include "Renderer/renderer.h"
#include "Renderer/camera.h"

#include "buildingregistry.h"

// Viewport index for the toolbar UI camera
inline constexpr size_t UI_VIEWPORT = 2;

class ToolbarSystem : public pg::System<pg::InitSys, pg::Listener<pg::OnSDLScanCode>, pg::QueuedListener<pg::OnMouseClick>>
{
public:
    static constexpr float TOOLBAR_HEIGHT = 48.0f;
    static constexpr float SLOT_SIZE = 32.0f;
    static constexpr float SLOT_SPACING = 4.0f;
    static constexpr float SLOT_PADDING = 8.0f; // Padding from toolbar edges

    ToolbarSystem(BuildingRegistry* registry, pg::MasterRenderer* masterRenderer, float screenWidth, float screenHeight)
        : registry(registry), masterRenderer(masterRenderer), screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Toolbar System"; }

    void init() override;

    virtual void onEvent(const pg::OnSDLScanCode& event) override;
    virtual void onProcessEvent(const pg::OnMouseClick& event) override;

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
    pg::MasterRenderer* masterRenderer = nullptr;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    pg::EntityRef uiCameraEntity;
    size_t selectedSlot = 0;

    // Direct EntityRef handles populated post-build by walking the prefab tree. Saves a
    // hash-map lookup on every access compared to storing _unique_id and re-resolving.
    pg::EntityRef backdrop;
    pg::EntityRef highlight;
    std::vector<pg::EntityRef> slots;
};
