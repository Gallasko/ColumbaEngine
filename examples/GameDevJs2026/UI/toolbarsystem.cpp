#include "toolbarsystem.h"

#include "UI/prefabspec.h"
#include "UI/prefabbuilder.h"
#include "UI/prefab.h"
#include "2D/simple2dobject.h"
#include "2D/position.h"

#include <SDL2/SDL.h>

using namespace pg;

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
    const auto windowId = windowEnt->id;
    const float totalSlotsWidth = registry->count() * SLOT_SIZE + (registry->count() - 1) * SLOT_SPACING;

    // ----- Spec: backdrop containing a flow-row of slots, horizontally centered. -----
    //
    // The outer NodeSpec is the backdrop (Shape2D). Always-wrap turns it into a Prefab
    // container so we can address its named children. The middle child is an invisible
    // Shape2D that horizontally-centers in the backdrop and uses Flow::Horizontal to
    // chain the slots without manual leftMargin math.
    NodeSpec spec;
    spec.kind  = "Shape2D";
    spec.name  = "backdrop";
    spec.props = {
        {"width",    0.0f},
        {"height",   TOOLBAR_HEIGHT},
        {"r",        30.0f},
        {"g",        30.0f},
        {"b",        40.0f},
        {"a",        200.0f},
        {"z",        90.0f},
        {"viewport", static_cast<int>(UI_VIEWPORT)},
    };
    spec.anchors = {
        AnchorSpec{windowId, AnchorType::Left,   AnchorType::Left,   0.0f},
        AnchorSpec{windowId, AnchorType::Right,  AnchorType::Right,  0.0f},
        AnchorSpec{windowId, AnchorType::Bottom, AnchorType::Bottom, 0.0f},
    };

    // Slot row: invisible Shape2D sized to the row width, centered horizontally on backdrop.
    NodeSpec slotRow;
    slotRow.kind  = "Shape2D";
    slotRow.name  = "slotRow";
    slotRow.props = {
        {"width",  totalSlotsWidth},
        {"height", SLOT_SIZE},
        {"a",      0.0f},                                // invisible
        {"viewport", static_cast<int>(UI_VIEWPORT)},
    };
    slotRow.anchors = {
        AnchorSpec{"main", AnchorType::HorizontalCenter, AnchorType::HorizontalCenter, 0.0f},
        AnchorSpec{"main", AnchorType::Top, SLOT_PADDING},
    };
    slotRow.flow    = Flow::Horizontal;
    slotRow.spacing = SLOT_SPACING;

    for (size_t i = 0; i < registry->count(); ++i)
    {
        const auto& def = registry->get(i);
        NodeSpec slot;
        slot.name = "slot" + std::to_string(i);

        if (not def.textureName.empty())
        {
            slot.kind  = "Texture";
            slot.props = {
                {"texture",  def.textureName + ".0"},
                {"width",    SLOT_SIZE},
                {"height",   SLOT_SIZE},
                {"z",        95.0f},
                {"viewport", static_cast<int>(UI_VIEWPORT)},
            };
        }
        else
        {
            slot.kind  = "Shape2D";
            slot.props = {
                {"width",    SLOT_SIZE},
                {"height",   SLOT_SIZE},
                {"r",        def.color.x},
                {"g",        def.color.y},
                {"b",        def.color.z},
                {"a",        def.color.w},
                {"z",        95.0f},
                {"viewport", static_cast<int>(UI_VIEWPORT)},
            };
        }
        slotRow.children.push_back(std::move(slot));
    }
    spec.children.push_back(std::move(slotRow));

    // Selection highlight: an invisible-by-default white overlay. Its anchors are set
    // dynamically in updateHighlight() based on the currently selected slot.
    NodeSpec highlightSpec;
    highlightSpec.kind  = "Shape2D";
    highlightSpec.name  = "highlight";
    highlightSpec.props = {
        {"width",    SLOT_SIZE + 4.0f},
        {"height",   SLOT_SIZE + 4.0f},
        {"r",        255.0f},
        {"g",        255.0f},
        {"b",        255.0f},
        {"a",        60.0f},
        {"z",        0.96f},
        {"viewport", static_cast<int>(UI_VIEWPORT)},
    };
    spec.children.push_back(std::move(highlightSpec));

    // ----- Build. The returned wrap is the toolbar root. -----
    backdrop = buildNode(ecsRef, spec);
    auto backdropPrefab = backdrop->get<Prefab>();

    // The composite "slotRow" child is non-trivial (it has slot siblings), so its wrap is
    // kept whole — we drill in to grab each slot's EntityRef.
    auto slotRowEnt = backdropPrefab->getEntity("slotRow");
    auto slotRowPrefab = slotRowEnt->get<Prefab>();

    slots.clear();
    slots.reserve(registry->count());
    for (size_t i = 0; i < registry->count(); ++i)
        slots.push_back(slotRowPrefab->getEntity("slot" + std::to_string(i)));

    highlight = backdropPrefab->getEntity("highlight");
    updateHighlight();
}

void ToolbarSystem::updateHighlight()
{
    if (selectedSlot >= slots.size() or not highlight)
        return;

    auto hlAnchor = highlight->get<UiAnchor>();
    if (not hlAnchor)
        hlAnchor = ecsRef->attach<UiAnchor>(highlight);

    hlAnchor->clearAnchors();
    hlAnchor->setLeftAnchor(PosAnchor{slots[selectedSlot].id, AnchorType::Left});
    hlAnchor->setLeftMargin(-2.0f);
    hlAnchor->setTopAnchor(PosAnchor{slots[selectedSlot].id, AnchorType::Top});
    hlAnchor->setTopMargin(-2.0f);
}
